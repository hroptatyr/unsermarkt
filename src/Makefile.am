### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =

headerdir = $(includedir)/unserding
moddir = $(libdir)/unserding

bin_PROGRAMS =
noinst_PROGRAMS =
mod_LTLIBRARIES =
noinst_LTLIBRARIES =
noinst_HEADERS =
header_HEADERS =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_HEADERS += boobs.h
noinst_HEADERS += nifty.h

mod_LTLIBRARIES += dso-unsermarkt.la
dso_unsermarkt_la_LDFLAGS = -module
dso_unsermarkt_la_LDFLAGS += $(unserding_LDFLAGS) -lsqlite3 -lmxml
dso_unsermarkt_la_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
dso_unsermarkt_la_SOURCES = dso-unsermarkt.c
dso_unsermarkt_la_SOURCES += md5.c md5.h
dso_unsermarkt_la_SOURCES += htws.c htws.h
dso_unsermarkt_la_SOURCES += oq.c oq.h
dso_unsermarkt_la_SOURCES += uschi.c uschi.h
dso_unsermarkt_la_SOURCES += order.h
dso_unsermarkt_la_SOURCES += instr.h
dso_unsermarkt_la_SOURCES += match.h
dso_unsermarkt_la_SOURCES += um-conn.h
dso_unsermarkt_la_SOURCES += um-stamp.h
dso_unsermarkt_la_SOURCES += um-types.h
dso_unsermarkt_la_SOURCES += um-xml.h
dso_unsermarkt_la_CPPFLAGS += $(unserding_CFLAGS)
dso_unsermarkt_la_CPPFLAGS += $(uterus_CFLAGS)
EXTRA_dso_unsermarkt_la_SOURCES =
EXTRA_dso_unsermarkt_la_SOURCES += con6ity.c
EXTRA_dso_unsermarkt_la_SOURCES += mmls.c

noinst_PROGRAMS += umoc
umoc_LDFLAGS = $(unserding_LDFLAGS)
umoc_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
umoc_CPPFLAGS += $(unserding_CFLAGS)
umoc_CPPFLAGS += $(uterus_CFLAGS)

noinst_PROGRAMS += oq
oq_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
oq_CPPFLAGS += $(unserding_CFLAGS) $(uterus_CFLAGS)

noinst_PROGRAMS += uschi
uschi_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
uschi_CPPFLAGS += $(unserding_CFLAGS)
uschi_CPPFLAGS += $(uterus_CFLAGS)
uschi_LDFLAGS = $(AM_LDFLAGS) -lsqlite3

if HAVE_MXML
noinst_PROGRAMS += blood-mm
blood_mm_LDFLAGS = $(unserding_LDFLAGS) -lmxml -lpthread
blood_mm_CPPFLAGS = $(AM_CPPFLAGS)
blood_mm_CPPFLAGS += $(unserding_CFLAGS)
blood_mm_CPPFLAGS += $(uterus_CFLAGS)
endif  ## HAVE_MXML

if HAVE_UTERUS
noinst_PROGRAMS += um-quomon
um_quomon_SOURCES = um-quomon.c um-quomon-clo.ggo
BUILT_SOURCES += um-quomon-clo.c um-quomon-clo.h
um_quomon_CPPFLAGS = $(AM_CPPFLAGS)
um_quomon_CPPFLAGS += $(libev_CFLAGS)
um_quomon_CPPFLAGS += $(unserding_CFLAGS)
um_quomon_CPPFLAGS += $(uterus_CFLAGS)
um_quomon_LDFLAGS = $(AM_LDFLAGS)
um_quomon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_quomon_LDFLAGS += $(libev_LIBS)
um_quomon_LDFLAGS += $(unserding_LIBS)
endif

if HAVE_TWSTOOLS
noinst_LTLIBRARIES += tws-snarf.la
tws_snarf_la_LDFLAGS = -module -rpath /none
tws_snarf_la_SOURCES = tws-snarf.cpp
tws_snarf_la_CPPFLAGS = $(AM_CPPFLAGS)
tws_snarf_la_CPPFLAGS += $(twstools_CFLAGS)
tws_snarf_la_CPPFLAGS += $(uterus_CFLAGS)
tws_snarf_la_CPPFLAGS += $(unserding_CFLAGS)
tws_snarf_la_LDFLAGS += $(unserding_LIBS)
endif

bin_PROGRAMS += um-netdania
um_netdania_SOURCES = um-netdania.c um-netdania.h um-netdania-clo.ggo
BUILT_SOURCES += um-netdania-clo.c um-netdania-clo.h
um_netdania_CPPFLAGS = $(AM_CPPFLAGS)
um_netdania_CPPFLAGS += $(uterus_CFLAGS)
um_netdania_CPPFLAGS += $(unserding_CFLAGS)
um_netdania_CPPFLAGS += $(libev_CFLAGS)
um_netdania_LDFLAGS = $(AM_LDFLAGS)
um_netdania_LDFLAGS += $(libev_LIBS)
um_netdania_LDFLAGS += $(unserding_LIBS)

## our rule for gengetopt
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
