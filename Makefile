MODULES = extra_window_functions
EXTENSION = extra_window_functions
DOCS = README.extra_window_functions

DATA = extra_window_functions--1.0.sql

REGRESS = regression

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
