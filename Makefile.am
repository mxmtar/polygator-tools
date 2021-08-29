
ACLOCAL_AMFLAGS = -I m4 --install

bin_PROGRAMS = polygator

polygator_CFLAGS = $(AM_CFLAGS) -Wall
polygator_LDFLAGS = $(AM_LDFLAGS)

polygator_SOURCES = \
		polygator-tools.c

noinst_HEADERS = \
		polygator-tools.h \
		x_sllist.h \
		x_timer.h

CLEANFILES = *~
