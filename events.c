/*
 * Event functions that are called from pg_schema_triggers.c when various
 * interesting database events happen.
 *
 * These functions will (usually) set up an EventContext struct and then
 * invoke FireEventTriggers().
 *
 * pg_schema_triggers/events.c
 */


#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_class.h"
#include "catalog/pg_type.h"
#include "parser/parse_func.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"


#include "events.h"
#include "trigger_funcs.h"


typedef struct RelationCreate_EventInfo {
	char *eventname;
	Oid relnamespace;
	Oid relation;
} RelationCreate_EventInfo;


Datum pg_eventinfo_relation_create(PG_FUNCTION_ARGS);


int
stmt_listen_before(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_before: channel=\"%s\"", stmt->conditionname);
	FireEventTriggers("stmt.listen.before", stmt->conditionname, NULL);
	return 0;
}


int
stmt_listen_after(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_after: channel=\"%s\"", stmt->conditionname);
	FireEventTriggers("stmt.listen.after", stmt->conditionname, NULL);
	return 1;
}


void
object_post_create(ObjectAddress *object)
{
	return;
}


void
relation_created(ObjectAddress *object)
{
	RelationCreate_EventInfo info;
	char *nspname;
	char *relname;
	char *tag;

	Assert(object->classId == RelationRelationId);

	/* Bump the command counter so we can see the newly-created relation. */
	CommandCounterIncrement();

	/* Set up the event info. */
	info.eventname = "relation.create";
	info.relation = object->objectId;
	info.relnamespace = get_object_namespace(object);

	/* Prepare the tag string. */
	nspname = get_namespace_name(info.relnamespace);
	relname = get_rel_name(object->objectId);
	tag = quote_qualified_identifier(nspname, relname); 

	/* Fire the trigger. */
	FireEventTriggers("relation.create", tag, (EventInfo*)&info);
}


PG_FUNCTION_INFO_V1(pg_eventinfo_relation_create);
Datum
pg_eventinfo_relation_create(PG_FUNCTION_ARGS)
{
	RelationCreate_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[2];
	bool result_isnull[2];
	HeapTuple tuple;
	
	/* Extract the information from our EventInfo struct. */
	info = (RelationCreate_EventInfo *)GetCurrentEventInfo("relation.create");
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = ObjectIdGetDatum(info->relnamespace);
	result_isnull[0] = false;
	result_isnull[1] = false;

	/* Build our composite result and return it. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


void
object_post_alter(ObjectAddress *object, Oid auxObjId)
{
	return;
}


void
object_drop(ObjectAddress *object, int dropflags)
{
	return;
}
