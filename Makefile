# schema_triggers/Makefile

MODULE_big = schema_triggers
OBJS = catalog_funcs.o events.o hook_objacc.o init.o trigger_funcs.o
SHLIB_LINK = $(filter -lcrypt, $(LIBS))

EXTENSION = schema_triggers
DATA = schema_triggers--0.1.sql
DOCS = README.md
REGRESS = event_trigger relation column

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
