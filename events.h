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


int stmt_listen_before(ListenStmt *stmt);
int stmt_listen_after(ListenStmt *stmt);
void object_post_create(ObjectAddress *object);
void object_post_alter(ObjectAddress *object, Oid auxObjId);
void object_drop(ObjectAddress *object, int dropflags);
void relation_created(ObjectAddress *object);


#endif	/* SCHEMA_TRIGGERS_EVENTS_H */
