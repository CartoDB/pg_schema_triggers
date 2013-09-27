-- Protect against this script being sourced by psql.
\echo Use "CREATE EXTENSION" to load this file. \quit


CREATE FUNCTION pg_eventinfo_relation_create(OUT relation REGCLASS,
											 OUT relnamespace OID)
	RETURNS record
	LANGUAGE C
	AS 'pg_schema_triggers', 'pg_eventinfo_relation_create';

