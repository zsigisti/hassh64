# PGXS build: make && make install (as a user who can write to pg_config's dirs),
# then CREATE EXTENSION hassh64. `make check-local` runs the regression test
# against a throwaway cluster without installing anything system-wide.
EXTENSION = hassh64
MODULE_big = hassh64
OBJS = hassh64.o
DATA = hassh64--2.1.sql
REGRESS = hassh64
PG_CPPFLAGS = -I$(CURDIR)/../..
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
