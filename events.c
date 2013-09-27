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
#include "access/hash.h"
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
	info.relation = object->objectId;
	info.relnamespace = get_object_namespace(object);

	/* Prepare the tag string. */
	nspname = get_namespace_name(info.relnamespace);
	relname = get_rel_name(object->objectId);
	tag = quote_qualified_identifier(nspname, relname); 

	/* Fire the trigger. */
	FireEventTriggers("relation.create", tag, (EventInfo*)&info);
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
