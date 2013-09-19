# pg_schema_triggers/Makefile

MODULE_big = pg_schema_triggers
OBJS = event_trigger.o pg_schema_triggers.o
SHLIB_LINK = $(filter -lcrypt, $(LIBS))

EXTENSION = pg_schema_triggers
DATA = pg_schema_triggers--0.1.sql
DOCS = README.md
#REGRESS = test_schema_triggers

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
