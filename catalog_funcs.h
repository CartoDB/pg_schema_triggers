/*-------------------------------------------------------------------------
 *
 * catalog_funcs.h
 *    Declarations for system catalog access functions.
 *
 *
 * pg_schema_triggers/catalog_funcs.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SCHEMA_TRIGGERS_CATALOG_FUNCS_H
#define SCHEMA_TRIGGERS_CATALOG_FUNCS_H


#include "postgres.h"
#include "access/htup.h"
#include "utils/snapshot.h"


HeapTuple pgclass_fetch_tuple(Oid reloid, Snapshot snapshot);
HeapTuple pgattribute_fetch_tuple(Oid reloid, int16 attnum, Snapshot snapshot);


#endif	/* SCHEMA_TRIGGERS_CATALOG_FUNCS_H */
