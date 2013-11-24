CREATE EXTENSION schema_triggers;

-- Create an event trigger for the trigger_create event.
CREATE FUNCTION on_trigger_create()
 RETURNS event_trigger
 LANGUAGE plpgsql
 AS $$
	DECLARE
		event_info SCHEMA_TRIGGERS.TRIGGER_CREATE_EVENTINFO;
	BEGIN
		event_info := schema_triggers.get_trigger_create_eventinfo();
		RAISE NOTICE 'on_trigger_create: (tgname=''%'', tgrelid=''%'', tgenabled=''%'', is_internal=''%'')',
			(event_info.new).tgname, (event_info.new).tgrelid::REGCLASS,
			(event_info.new).tgenabled, event_info.is_internal;
		IF (event_info.new).tgname LIKE 'test_%' THEN
			RAISE EXCEPTION 'trigger name cannot begin with "test_"';
		END IF;
		IF (event_info.new).tgrelid = 'untriggered'::REGCLASS THEN
			RAISE EXCEPTION 'cannot add triggers to relation named "untriggered".';
		END IF;
	END;
$$;
CREATE EVENT TRIGGER trigcreate ON trigger_create
	EXECUTE PROCEDURE on_trigger_create();

-- Create an event trigger for the trigger_adjust event.
-- TODO

-- Create an event trigger for the trigger_rename event.
-- TODO

-- Create an event trigger for the trigger_drop event.
-- TODO

-- Create some empty tables.
CREATE TABLE foo();
CREATE TABLE foobar();
CREATE TABLE untriggered();

-- Create a do-nothing trigger function for the following tests.
CREATE FUNCTION ignore()
 RETURNS trigger
 LANGUAGE plpgsql
 AS $$ BEGIN RETURN NULL; END; $$;

-- Create some triggers on the tables.
CREATE TRIGGER foo_trig BEFORE INSERT ON foo
	EXECUTE PROCEDURE ignore();
CREATE TRIGGER foobar_trig BEFORE UPDATE OR DELETE ON foobar
	EXECUTE PROCEDURE ignore();
CREATE TRIGGER test_will_not_work BEFORE INSERT ON foobar
	EXECUTE PROCEDURE ignore();
CREATE TRIGGER also_will_not_work BEFORE INSERT ON untriggered
	EXECUTE PROCEDURE ignore();

-- Rename, enable, and disable the triggers.
ALTER TABLE foo DISABLE TRIGGER foo_trig;
ALTER TRIGGER foo_trig ON foo RENAME TO foo_trig2;
ALTER TABLE foo ENABLE TRIGGER foo_trig2;
ALTER TABLE foobar ENABLE REPLICA TRIGGER foobar_trig;
ALTER TABLE foobar ENABLE ALWAYS TRIGGER foobar_trig;
ALTER TABLE foobar DISABLE TRIGGER foobar_trig;

-- Drop the triggers.
DROP TRIGGER foo_trig2 ON foo;
DROP TRIGGER foobar_trig ON foobar;

-- Create a table with an automatically-created constraint trigger (for
-- the FOREIGN KEY) and then drop the tables, which will also drop the 
-- constraint triggers.
CREATE TABLE baz(a INTEGER PRIMARY KEY, b TEXT, c BOOLEAN);
CREATE TABLE xyzzy(d INTEGER NOT NULL REFERENCES baz);
DROP TABLE baz CASCADE;

-- Clean up.
DROP EVENT TRIGGER trigcreate;
DROP FUNCTION on_trigger_create();
DROP FUNCTION ignore();
DROP EXTENSION schema_triggers;
