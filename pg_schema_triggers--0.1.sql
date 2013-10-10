-- Protect against this script being sourced by psql.
\echo Use "CREATE EXTENSION" to load this file. \quit


-- Info for relation_create event.
CREATE TYPE relation_create_eventinfo AS (
	relation        REGCLASS,
	new				PG_CATALOG.PG_CLASS
);
CREATE FUNCTION get_relation_create_eventinfo()
	RETURNS relation_create_eventinfo
	LANGUAGE C
	AS 'pg_schema_triggers', 'relation_create_eventinfo';


-- Info for relation_alter event.
CREATE TYPE relation_alter_eventinfo AS (
	relation		REGCLASS,
	old				PG_CATALOG.PG_CLASS,
	new				PG_CATALOG.PG_CLASS
);
CREATE FUNCTION get_relation_alter_eventinfo()
	RETURNS relation_alter_eventinfo
	LANGUAGE C
	AS 'pg_schema_triggers', 'relation_alter_eventinfo';
