/*
 * Event functions that are called from pg_schema_triggers.c when various
 * interesting database events happen.
 *
 * These functions will (usually) set up an EventContext struct and then
 * invoke FireEventTriggers().
 *
 * pg_schema_triggers/events.c
 */


#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "access/hash.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_class.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "parser/parse_func.h"
#include "storage/itemptr.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/tqual.h"


#include "catalog_funcs.h"
#include "events.h"
#include "trigger_funcs.h"


/*** Event:  relation_create ***/


typedef struct RelationCreate_EventInfo {
	EventInfo header;
	Oid relation;
	HeapTuple new;
} RelationCreate_EventInfo;


void
relation_create_event(Oid rel)
{
	RelationCreate_EventInfo *info;

	/* Set up the event info. */
	EnterEventMemoryContext();
	info = (RelationCreate_EventInfo *)EventInfoAlloc("relation_create", sizeof(*info));
	info->relation = rel;
	info->new = pgclass_fetch_tuple(rel, SnapshotSelf);
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->new))
		elog(ERROR, "couldn't find new pg_class row for oid=(%u)", rel);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(relation_create_eventinfo);
Datum
relation_create_eventinfo(PG_FUNCTION_ARGS)
{
	RelationCreate_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[2];
	bool result_isnull[2];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (RelationCreate_EventInfo *)GetCurrentEvent("relation_create");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = HeapTupleGetDatum(info->new);
	result_isnull[0] = false;
	result_isnull[1] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  relation_alter ***/


typedef struct RelationAlter_EventInfo {
	EventInfo header;
	Oid relation;
	HeapTuple old;
	HeapTuple new;
} RelationAlter_EventInfo;


void
relation_alter_event(Oid rel)
{
	RelationAlter_EventInfo *info;

	/* Set up the event info and save the old and new pg_class rows. */
	EnterEventMemoryContext();
	info = (RelationAlter_EventInfo *)EventInfoAlloc("relation_alter", sizeof(*info));
	info->relation = rel;
	info->old = pgclass_fetch_tuple(rel, GetCatalogSnapshot(rel));
	info->new = pgclass_fetch_tuple(rel, SnapshotSelf);
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->old))
		elog(ERROR, "couldn't find old pg_class row for oid=(%u)", rel);
	if (!HeapTupleIsValid(info->new))
		elog(ERROR, "couldn't find new pg_class row for oid=(%u)", rel);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(relation_alter_eventinfo);
Datum
relation_alter_eventinfo(PG_FUNCTION_ARGS)
{
	RelationAlter_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[3];
	bool result_isnull[3];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (RelationAlter_EventInfo *)GetCurrentEvent("relation_alter");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = HeapTupleGetDatum(info->old);
	result[2] = HeapTupleGetDatum(info->new);
	result_isnull[0] = false;
	result_isnull[1] = false;
	result_isnull[2] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  relation_drop ***/


typedef struct RelationDrop_EventInfo {
	EventInfo header;
	Oid relation;
	HeapTuple old;
} RelationDrop_EventInfo;


void
relation_drop_event(Oid rel)
{
	RelationDrop_EventInfo *info;

	/* Set up the event info and save the old pg_class row. */
	EnterEventMemoryContext();
	info = (RelationDrop_EventInfo *)EventInfoAlloc("relation_drop", sizeof(*info));
	info->relation = rel;
	info->old = pgclass_fetch_tuple(rel, GetCatalogSnapshot(rel));
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->old))
		elog(ERROR, "couldn't find old pg_class row for oid=(%u)", rel);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(relation_drop_eventinfo);
Datum
relation_drop_eventinfo(PG_FUNCTION_ARGS)
{
	RelationDrop_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[2];
	bool result_isnull[2];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (RelationDrop_EventInfo *)GetCurrentEvent("relation_drop");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = HeapTupleGetDatum(info->old);
	result_isnull[0] = false;
	result_isnull[1] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  column_add ***/


typedef struct ColumnAdd_EventInfo {
	EventInfo header;
	Oid relation;
	int16 attnum;
	HeapTuple new;
} ColumnAdd_EventInfo;


void
column_add_event(Oid rel, int16 attnum)
{
	ColumnAdd_EventInfo *info;

	/* Set up the event info and save the new pg_attr row. */
	EnterEventMemoryContext();
	info = (ColumnAdd_EventInfo *)EventInfoAlloc("column_add", sizeof(*info));
	info->relation = rel;
	info->attnum = attnum;
	info->new = pgattribute_fetch_tuple(rel, attnum, SnapshotSelf);
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->new))
		elog(ERROR, "couldn't find new pg_attribute row for oid,attnum=(%u,%d)", rel, attnum);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(column_add_eventinfo);
Datum
column_add_eventinfo(PG_FUNCTION_ARGS)
{
	ColumnAdd_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[3];
	bool result_isnull[3];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (ColumnAdd_EventInfo *)GetCurrentEvent("column_add");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = Int16GetDatum(info->attnum);
	result[2] = HeapTupleGetDatum(info->new);
	result_isnull[0] = false;
	result_isnull[1] = false;
	result_isnull[2] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  column_alter ***/


typedef struct ColumnAlter_EventInfo {
	EventInfo header;
	Oid relation;
	int16 attnum;
	HeapTuple old;
	HeapTuple new;
} ColumnAlter_EventInfo;


void
column_alter_event(Oid rel, int16 attnum)
{
	ColumnAlter_EventInfo *info;

	/* Set up the event info and save the old and new pg_attr rows. */
	EnterEventMemoryContext();
	info = (ColumnAlter_EventInfo *)EventInfoAlloc("column_alter", sizeof(*info));
	info->relation = rel;
	info->attnum = attnum;
	info->old = pgattribute_fetch_tuple(rel, attnum, GetCatalogSnapshot(rel));
	info->new = pgattribute_fetch_tuple(rel, attnum, SnapshotSelf);
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->old))
		elog(ERROR, "couldn't find old pg_attr row for oid,attnum=(%u,%d)", rel, attnum);
	if (!HeapTupleIsValid(info->new))
		elog(ERROR, "couldn't find new pg_attr row for oid,attnum=(%u,%d)", rel, attnum);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(column_alter_eventinfo);
Datum
column_alter_eventinfo(PG_FUNCTION_ARGS)
{
	ColumnAlter_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[4];
	bool result_isnull[4];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (ColumnAlter_EventInfo *)GetCurrentEvent("column_alter");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = Int16GetDatum(info->attnum);
	result[2] = HeapTupleGetDatum(info->old);
	result[3] = HeapTupleGetDatum(info->new);
	result_isnull[0] = false;
	result_isnull[1] = false;
	result_isnull[2] = false;
	result_isnull[3] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  column_drop ***/


typedef struct ColumnDrop_EventInfo {
	EventInfo header;
	Oid relation;
	int16 attnum;
	HeapTuple old;
} ColumnDrop_EventInfo;


void
column_drop_event(Oid rel, int16 attnum)
{
	ColumnDrop_EventInfo *info;

	/* Set up the event info and save the old and new pg_attr rows. */
	EnterEventMemoryContext();
	info = (ColumnDrop_EventInfo *)EventInfoAlloc("column_drop", sizeof(*info));
	info->relation = rel;
	info->attnum = attnum;
	info->old = pgattribute_fetch_tuple(rel, attnum, GetCatalogSnapshot(rel));
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->old))
		elog(ERROR, "couldn't find old pg_attribute row for oid,attnum=(%u,%d)", rel, attnum);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(column_drop_eventinfo);
Datum
column_drop_eventinfo(PG_FUNCTION_ARGS)
{
	ColumnDrop_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[3];
	bool result_isnull[3];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (ColumnDrop_EventInfo *)GetCurrentEvent("column_drop");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->relation);
	result[1] = Int16GetDatum(info->attnum);
	result[2] = HeapTupleGetDatum(info->old);
	result_isnull[0] = false;
	result_isnull[1] = false;
	result_isnull[2] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  trigger_create ***/


typedef struct TriggerCreate_EventInfo {
	EventInfo header;
	Oid trigger_oid;
	bool is_internal;
	HeapTuple new;
} TriggerCreate_EventInfo;


void
trigger_create_event(Oid trigoid, bool is_internal)
{
	TriggerCreate_EventInfo *info;

	/* Set up the event info. */
	EnterEventMemoryContext();
	info = (TriggerCreate_EventInfo *)EventInfoAlloc("trigger_create", sizeof(*info));
	info->trigger_oid = trigoid;
	info->is_internal = is_internal;
	info->new = pgtrigger_fetch_tuple(trigoid, SnapshotSelf);
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->new))
		elog(ERROR, "couldn't find new pg_trigger row for oid=(%u)", trigoid);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(trigger_create_eventinfo);
Datum
trigger_create_eventinfo(PG_FUNCTION_ARGS)
{
	TriggerCreate_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[3];
	bool result_isnull[3];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (TriggerCreate_EventInfo *)GetCurrentEvent("trigger_create");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->trigger_oid);
	result[1] = BoolGetDatum(info->is_internal);
	result[2] = HeapTupleGetDatum(info->new);
	result_isnull[0] = false;
	result_isnull[1] = false;
	result_isnull[2] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}


/*** Event:  trigger_adjust and trigger_rename ***/


typedef struct TriggerAdjust_EventInfo {
	EventInfo header;
	Oid trigger_oid;
	char old_enabled;
	char new_enabled;
} TriggerAdjust_EventInfo;


typedef struct TriggerRename_EventInfo {
	EventInfo header;
	Oid trigger_oid;
	Name old_name;
	Name new_name;
} TriggerRename_EventInfo;


void 
trigger_alter_event(Oid trigoid)
{
	return;
}


/*** Event:  trigger_drop ***/


typedef struct TriggerDrop_EventInfo {
	EventInfo header;
	Oid trigger_oid;
	HeapTuple old;
} TriggerDrop_EventInfo;


void
trigger_drop_event(Oid trigoid)
{
	TriggerDrop_EventInfo *info;

	/* Set up the event info and save the old pg_trigger row. */
	EnterEventMemoryContext();
	info = (TriggerDrop_EventInfo *)EventInfoAlloc("trigger_drop", sizeof(*info));
	info->trigger_oid = trigoid;
	info->old = pgtrigger_fetch_tuple(trigoid, GetCatalogSnapshot(trigoid));
	LeaveEventMemoryContext();
	if (!HeapTupleIsValid(info->old))
		elog(ERROR, "couldn't find old pg_trigger row for oid=(%u)", trigoid);

	/* Enqueue the event. */
	EnqueueEvent((EventInfo*) info);
}


PG_FUNCTION_INFO_V1(trigger_drop_eventinfo);
Datum
trigger_drop_eventinfo(PG_FUNCTION_ARGS)
{
	TriggerDrop_EventInfo *info;
	TupleDesc tupdesc;
	Datum result[2];
	bool result_isnull[2];
	HeapTuple tuple;
	
	/* Get the tupdesc for our return type. */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("function returning record called in context "
				        "that cannot accept type record")));
	BlessTupleDesc(tupdesc);
	Assert(tupdesc->natts == sizeof result / sizeof result[0]);
	Assert(tupdesc->natts == sizeof result_isnull / sizeof result_isnull[0]);

	/* Get our EventInfo struct. */
	info = (TriggerDrop_EventInfo *)GetCurrentEvent("trigger_drop");

	/* Form and return the tuple. */
	result[0] = ObjectIdGetDatum(info->trigger_oid);
	result[1] = HeapTupleGetDatum(info->old);
	result_isnull[0] = false;
	result_isnull[1] = false;
	tuple = heap_form_tuple(tupdesc, result, result_isnull);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}
