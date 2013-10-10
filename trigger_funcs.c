/*
 * Utility functions for creating and firing event triggers.  This code is
 * mostly copied verbatim from commands/event_trigger.c, with minor tweaks to
 * allow for new event names.
 *
 * (The exported functions dealing with event triggers are too high-level for
 * our needs.  It's a shame we have to duplicate code...)
 *
 * pg_schema_triggers/trigger_funcs.c
 */


#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/event_trigger.h"
#include "commands/trigger.h"
#include "parser/parse_func.h"
#include "pgstat.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"


#include "trigger_funcs.h"


/* Holds the current event info. */
typedef struct EventTriggerContext {
	MemoryContext mcontext;
	EventTriggerData trigdata;
	EventInfo *info;
	struct EventTriggerContext *prev;
} EventTriggerContext;

EventTriggerContext *current_context = NULL;


static void fire_event(EventInfo *info);
static void invoke_event_triggers(List *runlist);
List * find_event_triggers_for_event(const char *eventname);


/*
 * Create an event trigger for the given event name which will call the
 * function 'trigfunc'.  Note that this function does not check that the event
 * name is valid, nor does it check that the function has the right number
 * and type of arguments or the correct return type.
 */
Oid
CreateEventTriggerEx(const char *eventname, const char *trigname, Oid trigfunc)
{
	/* Declarations from CreateEventTrigger(). */
    HeapTuple   tuple;
    Oid			evtOwner = GetUserId();

	/* Declarations from insert_event_trigger_tuple(). */
    Relation    tgrel;
    Oid         trigoid;
    HeapTuple   tgtuple;
    Datum       values[Natts_pg_event_trigger];
    bool        nulls[Natts_pg_event_trigger];
    NameData    evtnamedata,
                evteventdata;
    ObjectAddress myself,
                referenced;

    /*
     * It would be nice to allow database owners or even regular users to do
     * this, but there are obvious privilege escalation risks which would have
     * to somehow be plugged first.
     */
    if (!superuser())
        ereport(ERROR,
                (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                 errmsg("permission denied to create event trigger \"%s\"",
                        trigname),
                 errhint("Must be superuser to create an event trigger.")));

    /*
     * Give user a nice error message if an event trigger of the same name
     * already exists.
     */
    tuple = SearchSysCache1(EVENTTRIGGERNAME, CStringGetDatum(trigname));
    if (HeapTupleIsValid(tuple))
        ereport(ERROR,
                (errcode(ERRCODE_DUPLICATE_OBJECT),
                 errmsg("event trigger \"%s\" already exists",
                        trigname)));

    /* Open pg_event_trigger. */
    tgrel = heap_open(EventTriggerRelationId, RowExclusiveLock);

    /* Build the new pg_event_trigger tuple. */
    memset(nulls, false, sizeof(nulls));
    namestrcpy(&evtnamedata, trigname);
    values[Anum_pg_event_trigger_evtname - 1] = NameGetDatum(&evtnamedata);
    namestrcpy(&evteventdata, eventname);
    values[Anum_pg_event_trigger_evtevent - 1] = NameGetDatum(&evteventdata);
    values[Anum_pg_event_trigger_evtowner - 1] = ObjectIdGetDatum(evtOwner);
    values[Anum_pg_event_trigger_evtfoid - 1] = ObjectIdGetDatum(trigfunc);
    values[Anum_pg_event_trigger_evtenabled - 1] =
        CharGetDatum(TRIGGER_FIRES_ON_ORIGIN);
	nulls[Anum_pg_event_trigger_evttags - 1] = true;

    /* Insert heap tuple. */
    tgtuple = heap_form_tuple(tgrel->rd_att, values, nulls);
    trigoid = simple_heap_insert(tgrel, tgtuple);
    CatalogUpdateIndexes(tgrel, tgtuple);
    heap_freetuple(tgtuple);

    /* Depend on owner. */
    recordDependencyOnOwner(EventTriggerRelationId, trigoid, evtOwner);

    /* Depend on event trigger function. */
    myself.classId = EventTriggerRelationId;
    myself.objectId = trigoid;
    myself.objectSubId = 0;
    referenced.classId = ProcedureRelationId;
    referenced.objectId = trigfunc;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

    /* Post creation hook for new operator family */
    InvokeObjectPostCreateHook(EventTriggerRelationId, trigoid, 0);

    /* Close pg_event_trigger and return. */
    heap_close(tgrel, RowExclusiveLock);
	return trigoid;
}


/*
 * Beginning a new statement;  allocate a new EventTriggerContext.
 */
void
StartNewEvent()
{
	EventTriggerContext *prev = current_context;

	current_context = palloc(sizeof(EventTriggerContext));
	current_context->mcontext = AllocSetContextCreate(CurrentMemoryContext,
                                     "event info context",
                                     ALLOCSET_DEFAULT_MINSIZE,
                                     ALLOCSET_DEFAULT_INITSIZE,
                                     ALLOCSET_DEFAULT_MAXSIZE);
    current_context->prev = prev;
}


void
EndEvent()
{
	EventTriggerContext *prev;

	Assert(current_context != NULL);
	MemoryContextDelete(current_context->mcontext);
	prev = current_context->prev;
	pfree(current_context);
	current_context = prev;
}


/*
 * Allocate space for an EventInfo struct.
 */
EventInfo *
EventInfoAlloc(const char *eventname, size_t struct_size)
{
	MemoryContext old_mcontext;
	EventInfo *info;

	old_mcontext = MemoryContextSwitchTo(current_context->mcontext);	
	info = (EventInfo *)palloc0(struct_size);
	strncpy(info->eventname, eventname, sizeof(info->eventname) - 1);
	info->eventname[sizeof(info->eventname) - 1] = '\0';
	info->mcontext = current_context->mcontext;
	MemoryContextSwitchTo(old_mcontext);

	return info;
}


/*
 * Enqueue an event.
 *
 * The 'info' pointer may be retrieved by calling GetCurrentEvent(), but
 * only during execution of an event trigger.
 */
void
EnqueueEvent(EventInfo *info)
{
	/* FIXME:  right now, we just fire the event immediately. */
	fire_event(info);
}


/*
 * Fire the event trigger(s) for a given event.
 *
 * Unfortunately much of this code is copied from commands/event_trigger.c
 * and utils/cache/evtcache.c, as there is no clean API for invoking an
 * arbitrary event trigger by name.  (The existing code uses an enum, not
 * a string, for invoking event triggers.)
 */
void
fire_event(EventInfo *info)
{
	List *runlist;

	/* Event triggers are completely disabled in standalone mode. */
	if (!IsUnderPostmaster)
		return;

	/* Guard against stack overflow due to recursive event trigger. */
	check_stack_depth();

	/* Do we have any event triggers to fire? */
	Assert(info->eventname != NULL);
	runlist = find_event_triggers_for_event(info->eventname);
	if (runlist == NIL)
		return;

	/* Set up the event trigger context. */
	current_context->trigdata.type = T_EventTriggerData;
	current_context->trigdata.event = info->eventname;
	current_context->trigdata.tag = "";				/* Can't be NULL. */
	current_context->trigdata.parsetree = NULL;
	current_context->info = info;

	/*
	 * Fire the event triggers inside a PG_TRY() block to ensure that we
	 * clean up current_event_info, even in the event of an exception.
	 */
	PG_TRY();
	{
		invoke_event_triggers(runlist);
	}
	PG_CATCH();
	{
		current_context = NULL;
		PG_RE_THROW();
	}
	PG_END_TRY();

	/* Cleanup. */
	list_free(runlist);
	current_context->info = NULL;
}


static void
invoke_event_triggers(List *runlist)
{
	MemoryContext mcontext;
	MemoryContext old_mcontext;
	ListCell *lc;
	Node *trigdata;

	/*
	 * Ensure we're actually inside an event trigger!
	 */
	Assert(current_context != NULL);
	trigdata = (Node *)&current_context->trigdata;
	Assert(trigdata->type == T_EventTriggerData);

	/*
	 * Evaluate event triggers in a new memory context, to ensure any
	 * leaks get cleaned up promptly.
	 */
	mcontext = AllocSetContextCreate(CurrentMemoryContext,
                                     "event trigger context",
                                     ALLOCSET_DEFAULT_MINSIZE,
                                     ALLOCSET_DEFAULT_INITSIZE,
                                     ALLOCSET_DEFAULT_MAXSIZE);
    old_mcontext = MemoryContextSwitchTo(mcontext);

	/* Fire each event trigger that matched. */
	foreach(lc, runlist)
	{
		Oid         fnoid = lfirst_oid(lc);
		FmgrInfo    flinfo;
		FunctionCallInfoData fcinfo;
		PgStat_FunctionCallUsage fcusage;

		/* Look up the function. */
		fmgr_info(fnoid, &flinfo);

		InitFunctionCallInfoData(fcinfo, &flinfo, 0,
								InvalidOid, (Node *)trigdata, NULL);

		pgstat_init_function_usage(&fcinfo, &fcusage);
		FunctionCallInvoke(&fcinfo);
		pgstat_end_function_usage(&fcusage, true);

		/*
		 * Make sure anything the event triggers did will be visible to the
		 * next trigger (or the main command, if this was the last trigger).
		 */
		CommandCounterIncrement();

		/* Reclaim memory. */
		MemoryContextReset(mcontext);
	}

	/* Restore the old memory context and delete the temporary one. */
	MemoryContextSwitchTo(old_mcontext);
	MemoryContextDelete(mcontext);
}


/*
 * Retrieve the EventInfo that was passed to FireEventTriggers().  Only valid
 * during execution of an event trigger.
 *
 * If 'eventname' is not NULL, ensure that the returned EventInfo struct is
 * for the correct event.
 */
EventInfo *
GetCurrentEvent(const char *eventname)
{
	if (current_context == NULL)
		elog(ERROR, "may only be called from an event trigger.");
	
	if (current_context->info == NULL)
		elog(ERROR, "the \"%s\" event has no associated EventInfo.",
					current_context->trigdata.event);

	if (eventname == NULL)
		return current_context->info;

	if (strcmp(eventname, current_context->info->eventname) != 0)
		elog(ERROR, "cannot be called during the \"%s\" event.",
					current_context->info->eventname);

	return current_context->info;
}

 
/*
 * Scan (yes, seqscan) through pg_event_triggers to find any enabled event
 * triggers for the given event name, and return a List of function Oids to
 * execute.
 *
 * TODO:  build a cache, so we don't have to seqscan each time.
 */
List *
find_event_triggers_for_event(const char *eventname)
{
	List       *funclist = NIL;
	Relation    rel;
	Relation    irel;
	SysScanDesc	scan;

	/*
	 * Open pg_event_trigger and do a full scan, ordered by the event trigger's
	 * name.
	 *
	 * XXX:  is GetLatestSnapshot() really what we should be using here?
	 */
	rel = relation_open(EventTriggerRelationId, AccessShareLock);
	irel = index_open(EventTriggerNameIndexId, AccessShareLock);
	scan = systable_beginscan_ordered(rel, irel, GetLatestSnapshot(), 0, NULL);
	for (;;)
	{
		HeapTuple   tup;
		Form_pg_event_trigger form;
		char       *evtevent;

		/* Get next tuple. */
		tup = systable_getnext_ordered(scan, ForwardScanDirection);
		if (!HeapTupleIsValid(tup))
			break;

	    /* Skip trigger if disabled. */
        form = (Form_pg_event_trigger) GETSTRUCT(tup);
        if (form->evtenabled == TRIGGER_DISABLED)
            continue;

        /* Check event name.  XXX:  we ignore evttags[] entirely. */
        evtevent = NameStr(form->evtevent);
        if (strcmp(evtevent, eventname) != 0)
        	break;

        /* Event trigger matches.  Add evtfoid to our list. */
		funclist = lappend_oid(funclist, form->evtfoid);
	}

	/* Done with the scan. */
	systable_endscan_ordered(scan);
	index_close(irel, AccessShareLock);
	relation_close(rel, AccessShareLock);

	/* Return the list of functions to invoke. */
	return funclist; 
}
