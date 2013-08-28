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
#include "utils/builtins.h"


/* PG_MODULE_MAGIC must appear exactly once in the entire module. */
PG_MODULE_MAGIC;
