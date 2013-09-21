pg\_schema\_triggers 0.1
========================
This extension adds schema change events to the Event Triggers feature in
Postgres 9.3 and higher.  By using the ProcessUtility hook, it transparently
adds new event types to the CREATE EVENT TRIGGER command.  These new events
include column add, drop, and alter as well as table create and rename.

Each event has access to relevant information, such as the table's oid and
the old and new names.

This extension relies on the Event Triggers functionality added in Postgres
version 9.3;  9.4devel ought to be working as well.


New Events
----------
This extension adds a number of additional events that may be used with the
CREATE EVENT TRIGGER command.

    Event Name            Description
    --------------------  ----------------------------------------------------
    stmt.listen.before    Immediately before/after executing LISTEN ...;  the
    stmt.listen.after     TG_TAG variable is the channel name.  If the .before
                          trigger raises an exception, the LISTEN will not be
                          permitted.

	relation.create       New relation (table, view, index) has been created;
	                      filters include "relkind" (t, v, i) and "schema".
	                      Note that at the point that this event fires, the
	                      relation DOES NOT YET have any column defaults or
	                      constraints.  [Corresponds to OAT_CREATE hook.]

	relation.rename
	relation.drop

	constraint.add
	constraint.alter
	constraint.drop

    `column_add`          ALTER TABLE ... ADD COLUMN ...
    `column_alter_type`   ALTER TABLE ... ALTER COLUMN ... SET DATA TYPE ...
    `column_drop`         ALTER TABLE ... DROP COLUMN ...
    `column_rename`       ALTER TABLE ... RENAME COLUMN ... TO ...
    `table_create`        CREATE TABLE ...
    `table_rename`        ALTER TABLE ... RENAME TO ...
    `trigger_disable`     ALTER TABLE ... DISABLE TRIGGER ...
    `trigger_enable`      ALTER TABLE ... ENABLE TRIGGER ...

Note that there is no `table_drop` event, as this case is already handled by the
`sql_drop` event.


Example
-------
This example implements a restriction on the LISTEN command, which prevents
users from listening on a channel whose name begins with `pg_`.

    postgres=# CREATE FUNCTION listen_filter()
               RETURNS event_trigger
               LANGUAGE plpgsql
               AS $$
                 BEGIN
                   IF TG_TAG LIKE 'pg_%' THEN
                     RAISE EXCEPTION 'Not permitted to listen on channel (%).', TG_TAG;
                   END IF;
                 END;
               $$;
    CREATE FUNCTION
    postgres=# CREATE EVENT TRIGGER listen_restricted ON "stmt.listen.before"
               EXECUTE PROCEDURE listen_filter();
    CREATE EVENT TRIGGER
    postgres=# LISTEN foo;
    LISTEN
    postgres=# NOTIFY foo;
    NOTIFY
    Asynchronous notification "foo" received from server process with PID xxx.
    postgres=# LISTEN pg_foo;
    ERROR:  Not permitted to listen on channel (pg_foo).
    postgres=# NOTIFY pg_foo;
    NOTIFY
    postgres=# ALTER EVENT TRIGGER listen_restricted DISABLE;
    ALTER EVENT TRIGGER
    postgres=# LISTEN pg_foo;
    LISTEN


Build/install
-------------
To build, install, and test:

    $ make
    $ make install
    $ make installcheck

To enable this extension for a database, ensure that the extension has been
installed (`make install`) and then use the CREATE EXTENSION mechanism:

    CREATE EXTENSION pg_schema_triggers;

In order for the extension to work (that is, for CREATE EVENT TRIGGER to
recognize the new events and for the event triggers to be fired upon those
events happening) the `pg_schema_triggers.so` library must be loaded.  Add
a line to `postgresql.conf`:

    shared_preload_libraries = pg_schema_triggers.so

Alternately, the new events can be enabled during a single session with the
LOAD command.  This is unlikely to be useful except during testing:

    LOAD 'pg_schema_triggers.so';


Authors and Credits
-------------------
Andrew Tipton       andrew@kiwidrew.com

Developed by malloc() labs limited (www.malloclabs.com) for CartoDB.


License
-------
This Postgres extension is free software;  you may use it under the same terms
as Postgres itself.  (See the `LICENSE` file for full licensing information.)
