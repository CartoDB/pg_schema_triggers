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


/* PG_MODULE_MAGIC must appear exactly once in the entire module. */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static ProcessUtility_hook_type old_utility_hook = NULL;
static object_access_hook_type old_objectaccess_hook = NULL;
static int stmt_createEventTrigger_before(CreateEventTrigStmt *stmt);

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
	ObjectAddress object;

	object.classId = classId;
	object.objectId = objectId;
	object.objectSubId = subId;

	switch (access)
	{
		case OAT_POST_CREATE:
			{
				ObjectAccessPostCreate *args = (ObjectAccessPostCreate *)arg;

				if (!args->is_internal)
					object_post_create(&object);
			}
			break;
		case OAT_POST_ALTER:
			{
				ObjectAccessPostAlter *args = (ObjectAccessPostAlter *)arg;

				if (!args->is_internal)
					object_post_alter(&object, args->auxiliary_id);
			}
			break;
		case OAT_DROP:
			{
				ObjectAccessDrop *args = (ObjectAccessDrop *)arg;

				object_drop(&object, args->dropflags);
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
};

struct event supported_events[] = {
	{"stmt.listen.before"},
	{"stmt.listen.after"},
	{"relation.create"},

	/* end of list marker */
	{NULL}
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
		funcoid = LookupFuncName(stmt->funcname, 0, NULL, false);
		recognized = 1;
		break;
	}
	if (!recognized)
	{
		elog(INFO, "pg_schema_triggers:  didn't recognize event name, ignoring.");
		return 0;
	}

	/* Check the trigger function's return type. */
    if (get_func_rettype(funcoid) != EVTTRIGGEROID)
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_OBJECT_DEFINITION),
                 errmsg("function \"%s\" must return type \"%s\"",
                        get_func_name(funcoid), format_type_be(EVTTRIGGEROID))));

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
