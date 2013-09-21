/*
 * ProcessUtility hook and associated functions to provide additional schema
 * change events for the CREATE EVENT TRIGGER functionality.
 *
 *
 * pg_schema_triggers/pg_schema_triggers.c
 */


#include "postgres.h"
#include "fmgr.h"
#include "access/hash.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_type.h"
#include "parser/parse_func.h"
#include "tcop/utility.h"

#include "pg_schema_triggers.h"


/* PG_MODULE_MAGIC must appear exactly once in the entire module. */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static ProcessUtility_hook_type old_utility_hook = NULL;
static object_access_hook_type old_objectaccess_hook = NULL;

static void objectaccess_hook(ObjectAccessType access,
	Oid classId,
	Oid objectId,
	int subId,
	void *arg);
static void utility_hook(Node *parsetree,
	const char *queryString,
	ProcessUtilityContext context,
	ParamListInfo params,
	DestReceiver *dest,
	char *completionTag);

static void object_post_create(Oid classId, Oid objectId, int subId);
static void object_post_alter(Oid classId, Oid objectId, Oid auxObjId, int subId);
static void object_drop(Oid classId, Oid objectId, int subId, int dropflags);
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
		elog(FATAL, "a ProcessUtility hook is already installed.");
	old_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = utility_hook;

	if (object_access_hook != NULL)
		elog(FATAL, "an object_access hook is already installed.");
	old_objectaccess_hook = object_access_hook;
	object_access_hook = objectaccess_hook;
}

void
_PG_fini(void)
{
	if (ProcessUtility_hook != utility_hook)
		elog(FATAL, "hook conflict, our ProcessUtility hook has been removed.");
	ProcessUtility_hook = old_utility_hook;

	if (object_access_hook != objectaccess_hook)
		elog(FATAL, "hook conflict, our object_access hook has been removed.");
	object_access_hook = old_objectaccess_hook;
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
 * We abuse the OAT_* hooks (which are intended for access control frameworks
 * such as sepgsql) for getting notified on relation create, alter, and drop
 * actions.  This is more reliable than digging around in the utility command
 * parse trees, as these hooks are called from the internal functions which
 * actually update the system catalogs, regardless of how those functions were
 * invoked.
 */
static void
objectaccess_hook(ObjectAccessType access,
	Oid classId,
	Oid objectId,
	int subId,
	void *arg)
{
	switch (access)
	{
		case OAT_POST_CREATE:
			{
				ObjectAccessPostCreate *args = (ObjectAccessPostCreate *)arg;

				if (!args->is_internal)
					object_post_create(classId, objectId, subId);
			}
			break;
		case OAT_POST_ALTER:
			{
				ObjectAccessPostAlter *args = (ObjectAccessPostAlter *)arg;

				if (!args->is_internal)
					object_post_alter(classId, objectId, args->auxiliary_id, subId);
			}
			break;
		case OAT_DROP:
			{
				ObjectAccessDrop *args = (ObjectAccessDrop *)arg;

				object_drop(classId, objectId, subId, args->dropflags);
			}
			break;
		case OAT_NAMESPACE_SEARCH:
		case OAT_FUNCTION_EXECUTE:
			/* Ignore these events. */
			break;
	}
}


/*
 * List of events that we recognize, along with the number and types of
 * arguments that are passed to the event trigger function.
 */
struct event {
	char *eventname;
	int nargs;
	Oid argtypes[FUNC_MAX_ARGS];
};

struct event supported_events[] = {
	{"stmt.listen.before", 	0, 	{}},
	{"stmt.listen.after", 	0, 	{}},
	{"relation.create", 	2,	{REGCLASSOID, NAMEOID}},

	/* end of list marker */
	{NULL, 0, {}}
};

/*
 * Intercept CREATE EVENT TRIGGER statements with event names that we
 * recognize, and pass them to our own CreateEventTriggerEx() function.
 * Pass everything else through to ProcessUtility_standard otherwise.
 */
static int
stmt_createEventTrigger_before(CreateEventTrigStmt *stmt)
{
    Oid         funcoid;
    int			recognized = 0;
    struct event *evt;

	/*
	 * If we recognize the event name, look up the corresponding function.
	 * Otherwise, fall through to the normal CreateEventTrigger() code.
	 */
	for (evt = supported_events; evt->eventname != NULL; evt++)
	{
		if (strcmp(stmt->eventname, evt->eventname) != 0)
			continue;

		/*
		 * LookupFuncName() will raise an error if it can't find a
		 * function with the appropriate signature.
		 */
		funcoid = LookupFuncName(stmt->funcname, evt->nargs, evt->argtypes, false);
		recognized = 1;
		break;
	}
	if (!recognized)
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

	/* Create the event trigger. */
    CreateEventTriggerEx(stmt->eventname, stmt->trigname, funcoid);

	/* And skip the call to CreateEventTrigger(). */
	return 1;
}


static int
stmt_listen_before(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_before: channel=\"%s\"", stmt->conditionname);
	FireEventTriggers("stmt.listen.before", stmt->conditionname);
	return 0;
}


static int
stmt_listen_after(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_after: channel=\"%s\"", stmt->conditionname);
	FireEventTriggers("stmt.listen.after", stmt->conditionname);
	return 1;
}


static void
object_post_create(Oid classId, Oid objectId, int subId)
{
	elog(NOTICE, "object_post_create: classId=%d, objectId=%d, subId=%d",
				 classId, objectId, subId);
}

static void
object_post_alter(Oid classId, Oid objectId, Oid auxObjId, int subId)
{
	elog(NOTICE, "object_post_alter: classId=%d, objectId=%d, auxObjId=%d, subId=%d",
				 classId, objectId, auxObjId, subId);
}


static void
object_drop(Oid classId, Oid objectId, int subId, int dropflags)
{
	elog(NOTICE, "object_drop: classId=%d, objectId=%d, subId=%d, dropflags=%d",
				 classId, objectId, subId, dropflags);
}
