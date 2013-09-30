-- Protect against this script being sourced by psql.
\echo Use "CREATE EXTENSION" to load this file. \quit


-- Info for "relation.create" event.
CREATE TYPE relation_create_info AS (
	relation        REGCLASS,
	relnamespace    OID,
	relkind         CHAR			-- same as the "pg_class.relkind" column
);
CREATE FUNCTION get_relation_create_info()
	RETURNS relation_create_info
	LANGUAGE C
	AS 'pg_schema_triggers', 'relation_create_getinfo';
