### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =

headerdir = $(includedir)/unserding
moddir = $(libdir)/unserding

bin_PROGRAMS =
noinst_PROGRAMS =
mod_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

mod_LTLIBRARIES += dso-unsermarkt.la
noinst_PROGRAMS += umoc
noinst_PROGRAMS += oq
noinst_PROGRAMS += uschi
noinst_PROGRAMS += blood-mm

dso_unsermarkt_la_LDFLAGS = -module
dso_unsermarkt_la_LDFLAGS += $(unserding_LDFLAGS) -lsqlite3 -lmxml
dso_unsermarkt_la_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
dso_unsermarkt_la_SOURCES = dso-unsermarkt.c
dso_unsermarkt_la_SOURCES += md5.c md5.h
dso_unsermarkt_la_SOURCES += htws.c htws.h
dso_unsermarkt_la_SOURCES += oq.c oq.h
dso_unsermarkt_la_SOURCES += uschi.c uschi.h
dso_unsermarkt_la_SOURCES += order.h
dso_unsermarkt_la_CPPFLAGS += $(unserding_CFLAGS)
dso_unsermarkt_la_CPPFLAGS += $(uterus_CFLAGS)

umoc_LDFLAGS = $(unserding_LDFLAGS)
umoc_CPPFLAGS = $(AM_CPPFLAGS)
umoc_CPPFLAGS += $(unserding_CFLAGS)
umoc_CPPFLAGS += $(uterus_CFLAGS)

oq_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
oq_CPPFLAGS += $(unserding_CFLAGS) $(uterus_CFLAGS)

uschi_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
uschi_CPPFLAGS += $(unserding_CFLAGS)
uschi_CPPFLAGS += $(uterus_CFLAGS)
uschi_LDFLAGS = $(AM_LDFLAGS) -lsqlite3

blood_mm_LDFLAGS = $(unserding_LDFLAGS) -lmxml -lpthread
blood_mm_CPPFLAGS = $(AM_CPPFLAGS)
blood_mm_CPPFLAGS += $(unserding_CFLAGS)
blood_mm_CPPFLAGS += $(uterus_CFLAGS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
