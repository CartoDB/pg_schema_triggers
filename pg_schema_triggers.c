/*
 * ProcessUtility hook and associated functions to provide additional schema
 * change events for the CREATE EVENT TRIGGER functionality.
 *
 *
 * pg_schema_triggers/pg_schema_triggers.c
 */


#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "parser/parse_func.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"


/* PG_MODULE_MAGIC must appear exactly once in the entire module. */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static ProcessUtility_hook_type old_utility_hook = NULL;

static void utility_hook(Node *parsetree,
	const char *queryString,
	ProcessUtilityContext context,
	ParamListInfo params,
	DestReceiver *dest,
	char *completionTag);
static int stmt_createEventTrigger_before(CreateEventTrigStmt *stmt);
static int stmt_listen_before(ListenStmt *stmt);
static int stmt_listen_after(ListenStmt *stmt);


/*
 * Install and uninstall the hook.
 *
 * We react poorly if we discover another module has also installed a
 * ProcessUtility hook.
 */

void
_PG_init(void)
{
	if (ProcessUtility_hook != NULL)
		elog(FATAL, "oops, someone else has already hooked ProcessUtility.");

	old_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = utility_hook;
}

void
_PG_fini(void)
{
	if (ProcessUtility_hook != utility_hook)
		elog(FATAL, "hook conflict, unable to safely unload.");

	ProcessUtility_hook = old_utility_hook;
}


/*
 * The meat of the hook.
 */

static void
utility_hook(Node *parsetree,
	const char *queryString,
	ProcessUtilityContext context,
	ParamListInfo params,
	DestReceiver *dest,
	char *completionTag)
{
	int suppress;

	/* Call the *_before() function, if any. */
	switch (nodeTag(parsetree))
	{
		case T_CreateEventTrigStmt:
			suppress = stmt_createEventTrigger_before((CreateEventTrigStmt *) parsetree);
			break;
		case T_ListenStmt:
			suppress = stmt_listen_before((ListenStmt *) parsetree);
			break;
		default:
			suppress = 0;
			break;
	}

	/* Did the before() function handle the command itself? */
	if (suppress)
		return;
	standard_ProcessUtility(parsetree, queryString, context, params, dest, completionTag);

	/* Call the *_after() function, if any. */
	switch (nodeTag(parsetree))
	{
		case T_ListenStmt:
			stmt_listen_after((ListenStmt *) parsetree);
			break;
		default:
			break;
	}
}


/*
 * Much of this code is copied from the CreateEventTrigger() function in
 * backend/commands/event_trigger.c, as there is no other way to add new
 * event types.
 */
static int
stmt_createEventTrigger_before(CreateEventTrigStmt *stmt)
{
	/* Declarations from CreateEventTrigger(). */
    HeapTuple   tuple;
    Oid         funcoid;
    Oid         funcrettype;
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
	 * If we don't recognize the event name, let the real CreateEventTrigger()
	 * function handle it.
	 */
	if (strcmp(stmt->eventname, "stmt.listen.before") != 0 &&
	    strcmp(stmt->eventname, "stmt.listen.after") != 0)
	{
		elog(INFO, "pg_schema_triggers:  didn't recognize event name, ignoring.");
		return 0;
	}

	/* None of our events support the WHEN clause, so ensure that it is empty. */
	if (stmt->whenclause)
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("event \"%s\" cannot have a WHEN clause",
						stmt->eventname)));

	/*** Everything below is copied more-or-less exactly as it appears in
	 *** CreateEventTrigger() and insert_event_trigger_tuple().  Only choice
	 *** was to copy-and-paste, as we need to modify some of the logic, and
	 *** further because insert_event_trigger_tuple() is not exported.
	 ***/

    /*
     * It would be nice to allow database owners or even regular users to do
     * this, but there are obvious privilege escalation risks which would have
     * to somehow be plugged first.
     */
    if (!superuser())
        ereport(ERROR,
                (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
                 errmsg("permission denied to create event trigger \"%s\"",
                        stmt->trigname),
                 errhint("Must be superuser to create an event trigger.")));

    /*
     * Give user a nice error message if an event trigger of the same name
     * already exists.
     */
    tuple = SearchSysCache1(EVENTTRIGGERNAME, CStringGetDatum(stmt->trigname));
    if (HeapTupleIsValid(tuple))
        ereport(ERROR,
                (errcode(ERRCODE_DUPLICATE_OBJECT),
                 errmsg("event trigger \"%s\" already exists",
                        stmt->trigname)));

    /* Find and validate the trigger function. */
    funcoid = LookupFuncName(stmt->funcname, 0, NULL, false);
    funcrettype = get_func_rettype(funcoid);
    if (funcrettype != EVTTRIGGEROID)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
                 errmsg("function \"%s\" must return type \"event_trigger\"",
                        NameListToString(stmt->funcname))));

	/*** Code below copied verbatim from insert_event_trigger_tuple(). ***/

    /* Insert catalog entries. */

    /* Open pg_event_trigger. */
    tgrel = heap_open(EventTriggerRelationId, RowExclusiveLock);

    /* Build the new pg_event_trigger tuple. */
    memset(nulls, false, sizeof(nulls));
    namestrcpy(&evtnamedata, stmt->trigname);
    values[Anum_pg_event_trigger_evtname - 1] = NameGetDatum(&evtnamedata);
    namestrcpy(&evteventdata, stmt->eventname);
    values[Anum_pg_event_trigger_evtevent - 1] = NameGetDatum(&evteventdata);
    values[Anum_pg_event_trigger_evtowner - 1] = ObjectIdGetDatum(evtOwner);
    values[Anum_pg_event_trigger_evtfoid - 1] = ObjectIdGetDatum(funcoid);
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
    referenced.objectId = funcoid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

    /* Post creation hook for new operator family */
    InvokeObjectPostCreateHook(EventTriggerRelationId, trigoid, 0);

    /* Close pg_event_trigger. */
    heap_close(tgrel, RowExclusiveLock);

	/* And skip the call to CreateEventTrigger(). */
	return 1;
}


static int
stmt_listen_before(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_before: channel=\"%s\"", stmt->conditionname);
	return 0;
}


static int
stmt_listen_after(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_after: channel=\"%s\"", stmt->conditionname);
	return 1;
}
