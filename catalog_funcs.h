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


HeapTuple catalog_fetch_tuple(Oid relation, Oid index, Oid row, Snapshot snapshot, MemoryContext mcontext);

/* Convenience wrappers around catalog_fetch_tuple(). */
HeapTuple pgclass_fetch_tuple(Oid row, Snapshot snapshot, MemoryContext mcontext);


#endif	/* SCHEMA_TRIGGERS_CATALOG_FUNCS_H */
