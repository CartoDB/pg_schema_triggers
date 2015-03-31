CREATE EXTENSION schema_triggers;

-- Create an event trigger for the relation_create event.
CREATE FUNCTION on_relation_create()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.RELATION_CREATE_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_relation_create_eventinfo();
		RAISE NOTICE 'on_relation_create: "%"', event_info.relation;
		RAISE NOTICE '  (relnamespace=%, relkind=''%'', relnatts=%, relhaspkey=''%'')',
			(event_info.new).relnamespace, (event_info.new).relkind,
			(event_info.new).relnatts, (event_info.new).relhaspkey;
		IF (event_info.new).relname LIKE 'test_%' THEN
			RAISE EXCEPTION 'relation name cannot begin with "test_"';
		END IF;
	END;
$$;
CREATE EVENT TRIGGER relcreate ON relation_create
	EXECUTE PROCEDURE on_relation_create();

-- Create an event trigger for the relation_alter event.
CREATE FUNCTION on_relation_alter()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.RELATION_ALTER_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_relation_alter_eventinfo();
		IF (event_info.new).relname LIKE 'test_%' THEN
			RAISE EXCEPTION 'relation name cannot begin with "test_"';
		END IF;
		RAISE NOTICE 'on_relation_alter:';
		RAISE NOTICE '  old.relname="%", old.reloptions=%',
			(event_info.old).relname, (event_info.old).reloptions;
		RAISE NOTICE '  new.relname="%", new.reloptions=%',
			(event_info.new).relname, (event_info.new).reloptions;
	END;
$$;
CREATE EVENT TRIGGER relalter ON relation_alter
	EXECUTE PROCEDURE on_relation_alter();

-- Create an event trigger for the relation_drop event.
CREATE FUNCTION on_relation_drop()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.RELATION_DROP_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_relation_drop_eventinfo();
		IF (event_info.old).relkind = 'r' THEN
		  RAISE NOTICE 'on_relation_drop:  old.relname="%", old.relnatts=%, old.relhaspkey=''%''',
			(event_info.old).relname, (event_info.old).relnatts, (event_info.old).relhaspkey;
		END IF;
	END;
$$;
CREATE EVENT TRIGGER reldrop ON relation_drop
	EXECUTE PROCEDURE on_relation_drop();

-- Create some tables.
CREATE TABLE foobar();
CREATE TABLE test_foobar();
CREATE TABLE baz(a INTEGER PRIMARY KEY, b TEXT, c BOOLEAN);

-- Column DDL shouldn't trigger the relation_* events.
ALTER TABLE foobar ADD COLUMN x TEXT NOT NULL;
ALTER TABLE baz DROP COLUMN b;

-- But let's rename them and change some options.
ALTER TABLE foobar RENAME TO foobar_two;
ALTER TABLE baz RENAME TO test_baz;		-- (this won't work)
ALTER TABLE baz SET (fillfactor = 50);

-- Then drop them.
DROP TABLE foobar_two;
DROP TABLE IF EXISTS test_foobar;  -- (it shouldn't exist...)
DROP TABLE baz;

-- Create multiple tables with a single DDL statement.
-- CREATE SCHEMA multi_table
--   CREATE TABLE table1 (a INTEGER, b TEXT)
--   CREATE TABLE table2 (c BOOLEAN)
--   CREATE TABLE table3 (d DATE);

-- ...and then drop them all, again with a single DDL statement.
-- DROP SCHEMA multi_table CASCADE;

-- Clean up the event trigger.
DROP EVENT TRIGGER relcreate;
DROP EVENT TRIGGER relalter;
DROP EVENT TRIGGER reldrop;
DROP FUNCTION on_relation_create();
DROP FUNCTION on_relation_alter();
DROP FUNCTION on_relation_drop();
DROP EXTENSION schema_triggers;
