
PG_CONFIG = /home/heikki/pgsql.master/bin/pg_config

PROGRAM = pg_internals_explorer
OBJS = main.o dblib.o display.o

PG_CPPFLAGS = -I$(libpq_srcdir)
PG_LIBS = $(libpq_pgport) -lcurses

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
