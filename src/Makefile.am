

### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS = 

headerdir = $(includedir)/unserding
moddir = $(libdir)/unserding

bin_PROGRAMS =
noinst_PROGRAMS =
mod_LTLIBRARIES =

BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

mod_LTLIBRARIES += dso-oq.la
mod_LTLIBRARIES += dso-uschi.la
mod_LTLIBRARIES += dso-unsermarkt.la
noinst_PROGRAMS += umoc
noinst_PROGRAMS += oq
noinst_PROGRAMS += uschi
noinst_PROGRAMS += blood-mm

dso_oq_la_LDFLAGS = -module
dso_oq_la_LDFLAGS += $(unserding_LDFLAGS)
dso_oq_la_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
dso_oq_la_SOURCES = dso-oq.c
dso_oq_la_SOURCES += oq.c oq.h

dso_uschi_la_LDFLAGS = -module
dso_uschi_la_LDFLAGS += $(unserding_LDFLAGS) -lsqlite3
dso_uschi_la_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
dso_uschi_la_SOURCES = dso-uschi.c
dso_uschi_la_SOURCES += uschi.c uschi.h

dso_unsermarkt_la_LDFLAGS = -module
dso_unsermarkt_la_LDFLAGS += $(unserding_LDFLAGS) -lsqlite3 -lmxml
dso_unsermarkt_la_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)
dso_unsermarkt_la_SOURCES = dso-unsermarkt.c
dso_unsermarkt_la_SOURCES += oq.c oq.h
dso_unsermarkt_la_SOURCES += uschi.c uschi.h

umoc_LDFLAGS = $(unserding_LDFLAGS)
umoc_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)

oq_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS) -DSTANDALONE

uschi_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS) -DSTANDALONE
uschi_LDFLAGS = $(AM_LDFLAGS) -lsqlite3

blood_mm_LDFLAGS = $(unserding_LDFLAGS) -lmxml -lpthread
blood_mm_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
