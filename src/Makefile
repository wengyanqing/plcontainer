# plcontainer/src/Makefile

MODULE_big = plc_coordinator

EXTENSION = plcontainer
SRCDIR = ./
FILES = co_coordinator.c
OBJS = co_coordinator.o
SHLIB_LINK = $(libpq)

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
