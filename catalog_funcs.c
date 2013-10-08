/*
 * Utility functions for looking up information in the system catalogs which
 * is not provided by the catcache/relcache infrastructure.
 *
 * pg_schema_triggers/catalog_funcs.c
 */


#include "postgres.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/pg_class.h"
#include "utils/lsyscache.h"


#include "catalog_funcs.h"


/*
 * Fetch a tuple from a system catalog by Oid.  The returned HeapTuple is
 * suitable for use with HeapTupleGetDatum(), and must be freed by calling
 * heap_freetuple().
 */
HeapTuple
catalog_fetch_tuple(Oid relation, Oid index, Oid row, Snapshot snapshot, MemoryContext mcontext)
{
    Relation	reldesc;
    SysScanDesc	relscan;
    ScanKeyData	key[1];
    HeapTuple	reltuple;
    Oid			reltypeid;
    MemoryContext old_mcontext;
	
	/* Scan key is an Oid. */
	ScanKeyInit(&key[0],
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber,
				184,		/* F_OIDEQ (from backend/utils/fmgroids.h) */
				ObjectIdGetDatum(row));

	/* Get the Oid of the relation's rowtype. */
	reltypeid = get_rel_type_id(relation);
	if (reltypeid == InvalidOid)
		elog(ERROR, "catalog_fetch_tuple:  relation %u has no rowtype", relation);

	/* Open the catalog relation and fetch a tuple using the given index. */
	reldesc = heap_open(relation, AccessShareLock);
	relscan = systable_beginscan(reldesc,
								 index,
								 true,
								 snapshot,
								 1, key);
	reltuple = systable_getnext(relscan);

	/* Copy the tuple. */
	if (!HeapTupleIsValid(reltuple))
		goto finish;

	/* Copy the tuple and ensure that the Datum headers are set. */
	old_mcontext = MemoryContextSwitchTo(mcontext);
	reltuple = heap_copytuple(reltuple);
	HeapTupleHeaderSetDatumLength(reltuple->t_data, reltuple->t_len);
	HeapTupleHeaderSetTypeId(reltuple->t_data, reltypeid);
	HeapTupleHeaderSetTypMod(reltuple->t_data, -1);
	MemoryContextSwitchTo(old_mcontext);
		
finish:
	/* Close the relation and return the copied tuple. */
	systable_endscan(relscan);
	heap_close(reldesc, AccessShareLock);
	return reltuple;
}


HeapTuple
pgclass_fetch_tuple(Oid row, Snapshot snapshot, MemoryContext mcontext)
{
	return catalog_fetch_tuple(RelationRelationId, ClassOidIndexId, row, snapshot, mcontext);
}
