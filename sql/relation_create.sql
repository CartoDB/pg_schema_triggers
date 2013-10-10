-- Create an event trigger for the relation_create event.
CREATE EXTENSION pg_schema_triggers;
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

-- Create some tables, and then drop them.
CREATE TABLE foobar();
CREATE TABLE test_foobar();
CREATE TABLE baz(a INTEGER PRIMARY KEY, b TEXT, c BOOLEAN);
ALTER TABLE foobar ADD COLUMN x TEXT NOT NULL;
ALTER TABLE baz DROP COLUMN b;
DROP TABLE foobar;
DROP TABLE IF EXISTS test_foobar;  -- (it shouldn't exist...)
DROP TABLE baz;

-- Clean up the event trigger.
DROP EVENT TRIGGER relcreate;
DROP FUNCTION on_relation_create();
DROP EXTENSION pg_schema_triggers;
