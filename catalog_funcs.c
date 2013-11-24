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
#include "catalog/pg_attribute.h"
#include "catalog/pg_class.h"
#include "catalog/pg_trigger.h"
#include "utils/lsyscache.h"


/*
 * F_INT2EQ and F_OIDEQ definitions copied from
 * backend/utils/fmgroids.h
 */
#define F_INT2EQ 63
#define F_OIDEQ 184


#include "catalog_funcs.h"


HeapTuple catalog_fetch_tuple(Oid relation,
							  Oid index,
							  ScanKeyData *keys,
							  int num_keys,
							  Snapshot snapshot);


HeapTuple
pgclass_fetch_tuple(Oid reloid, Snapshot snapshot)
{
	Oid relation = RelationRelationId;
	Oid index = ClassOidIndexId;
	ScanKeyData keys[1];

	/* Scan key is an Oid. */
	ScanKeyInit(&keys[0],
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber,
				F_OIDEQ,
				ObjectIdGetDatum(reloid));

	return catalog_fetch_tuple(relation, index, keys, 1, snapshot);
}


HeapTuple
pgattribute_fetch_tuple(Oid reloid, int16 attnum, Snapshot snapshot)
{
	Oid relation = AttributeRelationId;
	Oid index = AttributeRelidNumIndexId;
	ScanKeyData keys[2];

	ScanKeyInit(&keys[0],
				Anum_pg_attribute_attrelid,
				BTEqualStrategyNumber,
				F_OIDEQ,
				ObjectIdGetDatum(reloid));

	ScanKeyInit(&keys[1],
				Anum_pg_attribute_attnum,
				BTEqualStrategyNumber,
				F_INT2EQ,
				Int16GetDatum(attnum));
				
	return catalog_fetch_tuple(relation, index, keys, 2, snapshot);
}


HeapTuple
pgtrigger_fetch_tuple(Oid trigoid, Snapshot snapshot)
{
	Oid relation = TriggerRelationId;
	Oid index = TriggerOidIndexId;
	ScanKeyData keys[1];

	/* Scan key is an Oid. */
	ScanKeyInit(&keys[0],
				ObjectIdAttributeNumber,
				BTEqualStrategyNumber,
				F_OIDEQ,
				ObjectIdGetDatum(trigoid));

	return catalog_fetch_tuple(relation, index, keys, 1, snapshot);


}


/*
 * Fetch a tuple from a system catalog given a suitable scan key.  The returned
 * HeapTuple is suitable for use with HeapTupleGetDatum(), and must be freed by
 * calling heap_freetuple().
 */
HeapTuple
catalog_fetch_tuple(Oid relation, Oid index, ScanKeyData *keys, int num_keys, Snapshot snapshot)
{
    Relation	reldesc;
    SysScanDesc	relscan;
    HeapTuple	reltuple;
    Oid			reltypeid;
	
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
								 num_keys, keys);
	reltuple = systable_getnext(relscan);

	/* Copy the tuple. */
	if (!HeapTupleIsValid(reltuple))
		goto finish;

	/* Copy the tuple and ensure that the Datum headers are set. */
	reltuple = heap_copytuple(reltuple);
	HeapTupleHeaderSetDatumLength(reltuple->t_data, reltuple->t_len);
	HeapTupleHeaderSetTypeId(reltuple->t_data, reltypeid);
	HeapTupleHeaderSetTypMod(reltuple->t_data, -1);
		
finish:
	/* Close the relation and return the copied tuple. */
	systable_endscan(relscan);
	heap_close(reldesc, AccessShareLock);
	return reltuple;
}
