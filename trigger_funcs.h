/*-------------------------------------------------------------------------
 *
 * trigger_funcs.h
 *    Declarations for event trigger creation and invocation functions.
 *
 *
 * pg_schema_triggers/trigger_funcs.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SCHEMA_TRIGGERS_TRIGGER_FUNCS_H
#define SCHEMA_TRIGGERS_TRIGGER_FUNCS_H


#include "postgres.h"
#include "lib/ilist.h"


typedef struct EventInfo {
	char eventname[NAMEDATALEN];
	MemoryContext mcontext;
	slist_node next;
} EventInfo;


void StartNewEvent(void);
void EndEvent(void);
EventInfo *EventInfoAlloc(const char *eventname, size_t struct_size);
Oid CreateEventTriggerEx(const char *eventname, const char *trigname, Oid trigfunc);
void EnqueueEvent(EventInfo *info);
EventInfo* GetCurrentEvent(const char *eventname);


#endif	/* SCHEMA_TRIGGERS_TRIGGER_FUNCS_H */
