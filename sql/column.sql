CREATE EXTENSION schema_triggers;

-- Create an event trigger for the column_add event.
CREATE OR REPLACE FUNCTION on_column_add()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.COLUMN_ADD_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_column_add_eventinfo();
		RAISE NOTICE 'on_column_add(%, %)', event_info.relation, event_info.attnum;
		RAISE NOTICE '  new.attname=''%'', new.atttypid=%, new.attnotnull=''%''',
	 		(event_info.new).attname, (event_info.new).atttypid, (event_info.new).attnotnull;
	END;
 $$;
CREATE EVENT TRIGGER coladd ON column_add
	EXECUTE PROCEDURE on_column_add();

-- Create an event trigger for the column_alter event.
CREATE OR REPLACE FUNCTION on_column_alter()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.COLUMN_ALTER_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_column_alter_eventinfo();
		RAISE NOTICE 'on_column_alter(%, %)', event_info.relation, event_info.attnum;
		RAISE NOTICE '  old.attname=''%'', old.atttypid=%, old.attnotnull=''%''',
	 		(event_info.old).attname, (event_info.old).atttypid, (event_info.old).attnotnull;
		RAISE NOTICE '  new.attname=''%'', new.atttypid=%, new.attnotnull=''%''',
	 		(event_info.new).attname, (event_info.new).atttypid, (event_info.new).attnotnull;
	END;
 $$;
CREATE EVENT TRIGGER colalter ON column_alter
	EXECUTE PROCEDURE on_column_alter();

-- Create an event trigger for the column_drop event.
CREATE OR REPLACE FUNCTION on_column_drop()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.COLUMN_DROP_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_column_drop_eventinfo();
		RAISE NOTICE 'on_column_drop(%, %)', event_info.relation, event_info.attnum;
		RAISE NOTICE '  old.attname=''%'', old.atttypid=%, old.attnotnull=''%''',
	 		(event_info.old).attname, (event_info.old).atttypid, (event_info.old).attnotnull;
	END;
 $$;
CREATE EVENT TRIGGER coldrop ON column_drop
	EXECUTE PROCEDURE on_column_drop();

-- Create some tables, and then alter their columns.
CREATE TABLE foobar();
ALTER TABLE foobar ADD COLUMN x TEXT NOT NULL;
ALTER TABLE foobar RENAME COLUMN x TO xxx;
ALTER TABLE foobar ALTER COLUMN xxx DROP NOT NULL;
ALTER TABLE foobar ALTER COLUMN xxx TYPE BOOLEAN USING (FALSE);
ALTER TABLE foobar DROP COLUMN xxx;
DROP TABLE foobar;

CREATE TABLE baz(a INTEGER PRIMARY KEY, b TEXT, c BOOLEAN);
ALTER TABLE baz DROP COLUMN c;
ALTER TABLE baz SET WITH OIDS;
ALTER TABLE baz ALTER COLUMN b SET NOT NULL;
ALTER TABLE baz RENAME COLUMN a TO aaa;
ALTER TABLE baz SET WITHOUT OIDS;
DROP TABLE baz;

CREATE TABLE xyzzy(abc INTEGER, def INTEGER, ghi INTEGER);
ALTER TABLE xyzzy
	ALTER COLUMN abc TYPE TEXT,
	ADD COLUMN jkl BOOLEAN NOT NULL,
	DROP COLUMN def,
	ALTER COLUMN ghi SET NOT NULL;
DROP TABLE xyzzy;

-- Clean up the event trigger.
DROP EVENT TRIGGER coladd;
DROP EVENT TRIGGER colalter;
DROP EVENT TRIGGER coldrop;
DROP FUNCTION on_column_add();
DROP FUNCTION on_column_alter();
DROP FUNCTION on_column_drop();
DROP EXTENSION schema_triggers;
