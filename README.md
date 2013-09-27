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
This extension adds support for several additional events that may be used with
the CREATE EVENT TRIGGER command.

    Event Name            Description
    --------------------  ----------------------------------------------------
    stmt.listen.before    Immediately before/after executing LISTEN ...;  the
    stmt.listen.after     TG_TAG variable is the channel name.  If the .before
                          trigger raises an exception, the LISTEN will not be
                          permitted.

    relation.create       New relation (table, view, or index) created;  note
                          that at the point that this event fires, the table's
                          constraints and column defaults have NOT yet been
                          created.  [This corresponds to the OAT_POST_CREATE
                          hook.]

                          Within the event trigger, additional information is
                          available from the pg_eventinfo_relation_create()
                          function:

                              relation      REGCLASS
                              relnamespace  OID

The following events are planned, but have not yet been implemented:

    relation.rename       ...
    relation.drop         ...
    column.add            ALTER TABLE ... ADD COLUMN ...
                          (Note that no "column.add" events will be fired for
                          a new relation being created.)
    column.alter_type     ALTER TABLE ... ALTER COLUMN ... SET DATA TYPE ...
    column.drop           ALTER TABLE ... DROP COLUMN ...
    column.rename         ALTER TABLE ... RENAME COLUMN ... TO ...


Examples
--------
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

This example issues a NOTICE whenever a table is created with a name that
begins with `test_`.

    postgres=# CREATE FUNCTION on_relation_create()
               RETURNS event_trigger
               LANGUAGE plpgsql
               AS $$
                 DECLARE
                   event_info RECORD;
                 BEGIN
                   event_info := pg_eventinfo_relation_create();
                   IF NOT event_info.relation::text LIKE 'test_%' THEN RETURN; END IF;
                   RAISE NOTICE 'Relation (%) created in namespace oid:(%).',
                     event_info.relation,
                     event_info.relnamespace;
                 END;
               $$;
    CREATE FUNCTION
    postgres=# CREATE EVENT TRIGGER test_relations ON "relation.create"
               EXECUTE PROCEDURE on_relation_create();
    CREATE EVENT TRIGGER
    postgres=# create table foobar();
    CREATE TABLE
    postgres=# create table test_foobar();
    NOTICE:  Relation (test_foobar) created in namespace oid:(2200).
    CREATE TABLE


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
