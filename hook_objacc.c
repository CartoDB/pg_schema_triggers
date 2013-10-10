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
		elog(FATAL, "an object_access hook is already installed.");
	old_objectaccess_hook = object_access_hook;
	object_access_hook = objectaccess_hook;
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
		elog(FATAL, "hook conflict, our object_access hook has been removed.");
	object_access_hook = old_objectaccess_hook;
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
	}
}
