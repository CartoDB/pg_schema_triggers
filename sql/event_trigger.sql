-- Test that the CREATE/ALTER/DROP EVENT TRIGGER commands work as expected.

-- First, try creating a bogus event trigger before we have loaded the
-- extension.
CREATE EVENT TRIGGER wont_work ON "relation.create"
	EXECUTE PROCEDURE foo();
CREATE EXTENSION pg_schema_triggers;

-- Exercise the various cases that shouldn't work.
CREATE EVENT TRIGGER wont_work ON phase_of_the_moon
	EXECUTE PROCEDURE foo();
CREATE EVENT TRIGGER wont_work ON "relation.create"
	EXECUTE PROCEDURE undefined_function();
CREATE EVENT TRIGGER wrong_func_result_type ON "relation.create"
	EXECUTE PROCEDURE pg_client_encoding();

-- Before we create a working event trigger, we need a function to execute.
CREATE FUNCTION raise_notice()
 RETURNS event_trigger
 AS $$ BEGIN RAISE NOTICE 'do_notice:  event=(%)', TG_EVENT; END; $$
 LANGUAGE plpgsql;

-- Ensure that we're not allowed to specify a WHEN clause.
CREATE EVENT TRIGGER cannot_have_when_clause ON "relation.create"
	WHEN tag IN ('foo')
	EXECUTE PROCEDURE raise_notice();

-- Exercise the basic event trigger DDL.
CREATE EVENT TRIGGER one ON "relation.create"
	EXECUTE PROCEDURE raise_notice();
CREATE EVENT TRIGGER two ON "relation.alter"
	EXECUTE PROCEDURE raise_notice();
CREATE TABLE foobar();
ALTER TABLE foobar ADD COLUMN a TEXT;
DROP TABLE foobar;
ALTER EVENT TRIGGER one DISABLE;
ALTER EVENT TRIGGER two DISABLE;
CREATE TABLE foobar();
ALTER TABLE foobar ADD COLUMN a TEXT;
DROP TABLE foobar;
DROP EVENT TRIGGER one;
DROP EVENT TRIGGER two;
