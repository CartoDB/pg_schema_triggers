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
#include "parser/parse_func.h"
#include "tcop/utility.h"

#include "pg_schema_triggers.h"


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
 * Intercept CREATE EVENT TRIGGER statements with event names that we
 * recognize, and pass them to our own CreateEventTriggerEx() function.
 * Pass everything else through to ProcessUtility_standard otherwise.
 */
static int
stmt_createEventTrigger_before(CreateEventTrigStmt *stmt)
{
    Oid         funcoid;

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

	/* Create the event trigger. */
    funcoid = LookupFuncName(stmt->funcname, 0, NULL, false);
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
