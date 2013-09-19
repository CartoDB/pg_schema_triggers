/*
 * Utility functions for creating and firing event triggers.  This code is
 * mostly copied verbatim from commands/event_trigger.c, with minor tweaks to
 * allow for new event names.
 *
 * (The exported functions dealing with event triggers are too high-level for
 * our needs.  It's a shame we have to duplicate code...)
 *
 * pg_schema_triggers/event_trigger.c
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

#include "pg_schema_triggers.h"


Oid
CreateEventTriggerEx(const char *eventname, const char *trigname, Oid trigfunc)
{
	/* Declarations from CreateEventTrigger(). */
    HeapTuple   tuple;
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

    /* Validate the trigger function. */
    funcrettype = get_func_rettype(trigfunc);
    if (funcrettype != EVTTRIGGEROID)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
                 errmsg("function \"%s\" must return type \"event_trigger\"",
                        get_func_name(trigfunc))));

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
 * Invoke any event triggers which are registered for the given event name.
 *
 * Unfortunately this code, similar to CreateEventTriggerEx(), has to cut-and-
 * paste a bunch of stuff from commands/event_trigger.c as there is no clean
 * API for adding new events.
 */
void
InvokeEventTriggers(const char *eventname)
{


}
