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
#include "tcop/utility.h"
#include "utils/builtins.h"


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
static void stmt_listen_before(ListenStmt *stmt);
static void stmt_listen_after(ListenStmt *stmt);


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
	/* Call the *_before() function, if any. */
	switch (nodeTag(parsetree))
	{
		case T_ListenStmt:
			stmt_listen_before((ListenStmt *) parsetree);
			break;
		default:
			break;
	}

	/* Handle the utility command. */
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


static void
stmt_listen_before(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_before: channel=\"%s\"", stmt->conditionname);
}


static void
stmt_listen_after(ListenStmt *stmt)
{
	elog(NOTICE, "stmt_listen_after: channel=\"%s\"", stmt->conditionname);
}
