/*-------------------------------------------------------------------------
 *
 * events.h
 *    Declarations for event-handling functions.
 *
 *
 * pg_schema_triggers/events.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SCHEMA_TRIGGERS_EVENTS_H
#define SCHEMA_TRIGGERS_EVENTS_H


#include "postgres.h"
#include "fmgr.h"


void relation_create_event(Oid rel);
Datum relation_create_eventinfo(PG_FUNCTION_ARGS);

void relation_alter_event(Oid rel);
Datum relation_alter_eventinfo(PG_FUNCTION_ARGS);

void relation_drop_event(Oid rel);
Datum relation_drop_eventinfo(PG_FUNCTION_ARGS);

void column_add_event(Oid rel, int16 attnum);
Datum column_add_eventinfo(PG_FUNCTION_ARGS);

void column_alter_event(Oid rel, int16 attnum);
Datum column_alter_eventinfo(PG_FUNCTION_ARGS);


#endif	/* SCHEMA_TRIGGERS_EVENTS_H */
