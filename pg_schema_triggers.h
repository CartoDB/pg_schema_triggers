

/* pg_schema_triggers/event_trigger.c */
Oid CreateEventTriggerEx(const char *eventname, const char *trigname, Oid trigfunc);
void FireEventTriggers(const char *eventname, const char *tag);
