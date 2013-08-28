pg\_schema\_triggers 0.1
========================
This extension adds schema change events to the Event Triggers feature in
Postgres 9.3 and higher.  By using the ProcessUtility hook, it transparently
adds new event types to the CREATE EVENT TRIGGER command.  These new events
include column add, drop, and alter as well as table create and rename.

Each event has access to relevant information, such as the table's oid and
the old and new names.

Compatible with PostgreSQL 9.3rc1 and newer.


Usage
-----
To enable this extension for a database, ensure that the extension has been
installed (`make install`) and then use the CREATE EXTENSION mechanism:

    CREATE EXTENSION pg_schema_triggers;

Once installed, the `pg_schema_triggers` extension allows new event names to
be used with the CREATE EVENT TRIGGER command.

    Event Name            Description
    --------------------  ----------------------------------------------------
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


Build/install
-------------
To build, install, and test:

    $ make
    $ make install
    $ make installcheck


Authors and Credits
-------------------
Andrew Tipton       andrew@kiwidrew.com

Developed by malloc() labs limited (www.malloclabs.com).


License
-------
This Postgres extension is free software;  you may use it under the same terms
as Postgres itself.  (See the `LICENSE` file for full licensing information.)
