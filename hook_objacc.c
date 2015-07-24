/*
 * ObjectAccess hook and associated functions.
 *
 *
 * pg_schema_triggers/hook_objacc.c
 */


#include "postgres.h"
#include "catalog/dependency.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_class.h"
#include "catalog/pg_trigger.h"
#include "utils/builtins.h"

#include "events.h"
#include "hook_objacc.h"


static object_access_hook_type old_objectaccess_hook = NULL;

static void objectaccess_hook(ObjectAccessType access,
	Oid classId,
	Oid objectId,
	int subId,
	void *arg);
static void on_create(Oid classId, Oid objectId, int subId, ObjectAccessPostCreate *args);
static void on_alter(Oid classId, Oid objectId, int subId, ObjectAccessPostAlter *args);
static void on_drop(Oid classId, Oid objectId, int subId, ObjectAccessDrop *args);


/*
 * Install our objectaccess hook.
 *
 * We react poorly if we discover another module has also installed an
 * objectaccess hook.
 */
void
install_objacc_hook()
{
	if (object_access_hook != NULL)
		elog(WARNING, "pg_schema_triggers is getting into an object_access hook chain");
	old_objectaccess_hook = object_access_hook; /* save the old hook */
	object_access_hook = objectaccess_hook; /* put in our hook */
}


/*
 * Remove our objectaccess hook.
 *
 * We react poorly if we discover another module has already removed
 * our hook.
 */
void
remove_objacc_hook()
{
	if (object_access_hook != objectaccess_hook)
		elog(WARNING, "pg_schema_triggers is getting out of an object_access hook chain");
	object_access_hook = old_objectaccess_hook; /* put back the old hook */
}


/*
 * We abuse the OAT_* hooks (which are intended for access control frameworks
 * such as sepgsql) for getting notified on relation create, alter, and drop
 * actions.  This is more reliable than digging around in the utility command
 * parse trees, as these hooks are called from the internal functions which
 * actually update the system catalogs, regardless of how those functions were
 * invoked.
 */
static void
objectaccess_hook(ObjectAccessType access,
	Oid classId,
	Oid objectId,
	int subId,
	void *arg)
{

	/* Allow other hooks to run first */
	if ( old_objectaccess_hook )
		(*old_objectaccess_hook) (access, classId, objectId, subId, arg);

	switch (access)
	{
		case OAT_POST_CREATE:
			on_create(classId, objectId, subId, (ObjectAccessPostCreate *)arg);
			break;

		/*
		 * The OAT_POST_ALTER hook is called from the following functions:
		 *
		 *	 [func]						[class]				[obj]			[subobj]
		 *   renameatt_internal			RelationRelationId	pg_class.oid	attnum
		 *   RenameRelationInternal		RelationRelationId	pg_class.oid	0
		 */
		case OAT_POST_ALTER:
			on_alter(classId, objectId, subId, (ObjectAccessPostAlter *)arg);
			break;

		case OAT_DROP:
			on_drop(classId, objectId, subId, (ObjectAccessDrop *)arg);
			break;

		case OAT_NAMESPACE_SEARCH:
		case OAT_FUNCTION_EXECUTE:
			/* Ignore these events. */
			break;
	}
}


static void
on_create(Oid classId, Oid objectId, int subId, ObjectAccessPostCreate *args)
{
	if (args->is_internal)
		return;

	switch (classId)
	{
		case RelationRelationId:
			if (subId == 0)
				relation_create_event(objectId);
			else
				column_add_event(objectId, subId);
			break;
		case TriggerRelationId:
			trigger_create_event(objectId, args->is_internal);
			break;
	}
}


static void
on_alter(Oid classId, Oid objectId, int subId, ObjectAccessPostAlter *args)
{
	if (args->is_internal)
		return;

	switch (classId)
	{
		case RelationRelationId:
			if (subId == 0)
				relation_alter_event(objectId);
			else
				column_alter_event(objectId, subId);
			break;
		case TriggerRelationId:
			trigger_alter_event(objectId);
			break;
	}
}


static void
on_drop(Oid classId, Oid objectId, int subId, ObjectAccessDrop *args)
{
	if (args->dropflags & PERFORM_DELETION_INTERNAL)
		return;

	switch (classId)
	{
		case RelationRelationId:
			if (subId == 0)
				relation_drop_event(objectId);
			else
				column_drop_event(objectId, subId);
			break;
		case TriggerRelationId:
			trigger_drop_event(objectId);
			break;
	}
}
