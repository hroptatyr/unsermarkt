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

if HAVE_MXML
mod_LTLIBRARIES += dso-unsermarkt.la
dso_unsermarkt_la_LDFLAGS = -module
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
dso_unsermarkt_la_CPPFLAGS += $(mxml_CFLAGS)
dso_unsermarkt_la_LDFLAGS += $(mxml_LIBS)
dso_unsermarkt_la_LIBADD =
## check for me!
dso_unsermarkt_la_LIBADD += -lsqlite3
EXTRA_dso_unsermarkt_la_SOURCES =
EXTRA_dso_unsermarkt_la_SOURCES += con6ity.c
EXTRA_dso_unsermarkt_la_SOURCES += mmls.c
endif  ## HAVE_MXML

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
blood_mm_LDFLAGS =
blood_mm_CPPFLAGS = $(AM_CPPFLAGS)
blood_mm_CPPFLAGS += $(unserding_CFLAGS)
blood_mm_CPPFLAGS += $(uterus_CFLAGS)
blood_mm_CPPFLAGS += $(mxml_CFLAGS)
blood_mm_LDFLAGS += $(mxml_LIBS)
endif  ## HAVE_MXML

bin_PROGRAMS += um-quomon
um_quomon_SOURCES = um-quomon.c um-quomon-clo.ggo
BUILT_SOURCES += um-quomon-clo.c um-quomon-clo.h
um_quomon_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quomon_CPPFLAGS += $(libev_CFLAGS)
um_quomon_CPPFLAGS += $(unserding_CFLAGS)
um_quomon_CPPFLAGS += $(uterus_CFLAGS)
um_quomon_LDFLAGS = $(AM_LDFLAGS)
um_quomon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_quomon_LDFLAGS += $(unserding_LIBS)
um_quomon_LDFLAGS += $(libev_LIBS)
um_quomon_LDFLAGS += -lreadline

bin_PROGRAMS += um-quodmp
um_quodmp_SOURCES = um-quodmp.c um-quodmp-clo.ggo
BUILT_SOURCES += um-quodmp-clo.c um-quodmp-clo.h
um_quodmp_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quodmp_CPPFLAGS += $(libev_CFLAGS)
um_quodmp_CPPFLAGS += $(unserding_CFLAGS)
um_quodmp_CPPFLAGS += $(uterus_CFLAGS)
um_quodmp_LDFLAGS = $(AM_LDFLAGS)
um_quodmp_LDFLAGS += $(uterus_LIBS)
um_quodmp_LDFLAGS += $(unserding_LIBS)
um_quodmp_LDFLAGS += $(libev_LIBS)

bin_PROGRAMS += um-apfmon
um_apfmon_SOURCES = um-apfmon.c um-apfmon-clo.ggo
BUILT_SOURCES += um-apfmon-clo.c um-apfmon-clo.h
um_apfmon_SOURCES += gq.c gq.h
um_apfmon_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_apfmon_CPPFLAGS += $(libev_CFLAGS)
um_apfmon_CPPFLAGS += $(unserding_CFLAGS)
um_apfmon_CPPFLAGS += $(uterus_CFLAGS)
um_apfmon_LDFLAGS = $(AM_LDFLAGS)
um_apfmon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_apfmon_LDFLAGS += $(unserding_LIBS)
um_apfmon_LDFLAGS += $(libev_LIBS)
um_apfmon_LDFLAGS += -lreadline

bin_PROGRAMS += um-xmit
um_xmit_SOURCES = um-xmit.c
BUILT_SOURCES += um-xmit-clo.c um-xmit-clo.h
um_xmit_CPPFLAGS = $(AM_CPPFLAGS)
um_xmit_CPPFLAGS += $(uterus_CFLAGS)
um_xmit_CPPFLAGS += $(unserding_CFLAGS)
um_xmit_LDFLAGS = $(AM_LDFLAGS)
um_xmit_LDFLAGS += $(uterus_LIBS)
um_xmit_LDFLAGS += $(unserding_LIBS)

noinst_PROGRAMS += ox-matchall
ox_matchall_SOURCES = ox-matchall.c
BUILT_SOURCES += ox-matchall-clo.c ox-matchall-clo.h
ox_matchall_CPPFLAGS = $(AM_CPPFLAGS)
ox_matchall_CPPFLAGS += $(uterus_CFLAGS)
ox_matchall_CPPFLAGS += $(unserding_CFLAGS)
ox_matchall_CPPFLAGS += $(libev_CFLAGS)
ox_matchall_LDFLAGS = $(AM_LDFLAGS)
ox_matchall_LDFLAGS += $(uterus_LIBS)
ox_matchall_LDFLAGS += $(unserding_LIBS)
ox_matchall_LDFLAGS += $(libev_LIBS)

if HAVE_TWSTOOLS
noinst_LTLIBRARIES += tws-snarf.la
tws_snarf_la_LDFLAGS = -module -rpath /none
tws_snarf_la_SOURCES = tws-snarf.cpp
tws_snarf_la_CPPFLAGS = $(AM_CPPFLAGS)
tws_snarf_la_CPPFLAGS += $(twstools_CFLAGS)
tws_snarf_la_CPPFLAGS += $(uterus_CFLAGS)
tws_snarf_la_CPPFLAGS += $(unserding_CFLAGS)
tws_snarf_la_LDFLAGS += $(unserding_LIBS)

noinst_LTLIBRARIES += tws-order.la
tws_order_la_LDFLAGS = -module -rpath /none
tws_order_la_SOURCES = tws-order.cpp
tws_order_la_SOURCES += iso4217.c iso4217.h
tws_order_la_CPPFLAGS = $(AM_CPPFLAGS)
tws_order_la_CPPFLAGS += $(twstools_CFLAGS)
tws_order_la_CPPFLAGS += $(uterus_CFLAGS)
tws_order_la_CPPFLAGS += $(unserding_CFLAGS)
tws_order_la_LDFLAGS += $(unserding_LIBS)
endif  ## HAVE_TWSTOOLS

if HAVE_TWSAPI
noinst_PROGRAMS += ox-tws
EXTRA_ox_tws_SOURCES =
ox_tws_SOURCES = ox-tws.c ox-tws-clo.ggo
ox_tws_SOURCES += ox-tws-wrapper.cpp ox-tws-wrapper.h
ox_tws_SOURCES += ox-tws-contract-glue.cpp
ox_tws_SOURCES += iso4217.c iso4217.h
ox_tws_SOURCES += ox-tws-private.h
ox_tws_SOURCES += wrp-debug.h
EXTRA_ox_tws_SOURCES += gq.c gq.h
BUILT_SOURCES += ox-tws-clo.c ox-tws-clo.h
ox_tws_CPPFLAGS = $(AM_CPPFLAGS)
ox_tws_CPPFLAGS += $(uterus_CFLAGS)
ox_tws_CPPFLAGS += $(unserding_CFLAGS)
ox_tws_CPPFLAGS += $(twsapi_CFLAGS)
ox_tws_CPPFLAGS += $(libev_CFLAGS)
ox_tws_LDFLAGS = $(AM_LDFLAGS)
ox_tws_LDFLAGS += $(unserding_LIBS)
ox_tws_LDFLAGS += $(twsapi_LIBS)
ox_tws_LDFLAGS += $(libev_LIBS)

noinst_PROGRAMS += pf-tws
EXTRA_pf_tws_SOURCES =
pf_tws_SOURCES = pf-tws.c pf-tws-clo.ggo
pf_tws_SOURCES += pf-tws-wrapper.cpp pf-tws-wrapper.h
pf_tws_SOURCES += iso4217.c iso4217.h
pf_tws_SOURCES += strops.h
pf_tws_SOURCES += wrp-debug.h
BUILT_SOURCES += pf-tws-clo.c pf-tws-clo.h
pf_tws_CPPFLAGS = $(AM_CPPFLAGS)
pf_tws_CPPFLAGS += $(uterus_CFLAGS)
pf_tws_CPPFLAGS += $(unserding_CFLAGS)
pf_tws_CPPFLAGS += $(twsapi_CFLAGS)
pf_tws_CPPFLAGS += $(libev_CFLAGS)
pf_tws_LDFLAGS = $(AM_LDFLAGS)
pf_tws_LDFLAGS += $(unserding_LIBS)
pf_tws_LDFLAGS += $(twsapi_LIBS)
pf_tws_LDFLAGS += $(libev_LIBS)
endif  ## HAVE_TWSAPI

bin_PROGRAMS += um-netdania
um_netdania_SOURCES = um-netdania.c um-netdania.h um-netdania-clo.ggo
BUILT_SOURCES += um-netdania-clo.c um-netdania-clo.h
um_netdania_CPPFLAGS = $(AM_CPPFLAGS)
um_netdania_CPPFLAGS += $(uterus_CFLAGS)
um_netdania_CPPFLAGS += $(unserding_CFLAGS)
um_netdania_CPPFLAGS += $(libev_CFLAGS)
um_netdania_LDFLAGS = $(AM_LDFLAGS)
um_netdania_LDFLAGS += $(unserding_LIBS)
um_netdania_LDFLAGS += $(libev_LIBS)

noinst_PROGRAMS += tk-example
tk_example_SOURCES = tk-example.c
tk_example_CPPFLAGS = $(AM_CPPFLAGS)
tk_example_CPPFLAGS += $(uterus_CFLAGS)
tk_example_CPPFLAGS += $(unserding_CFLAGS)
tk_example_LDFLAGS = $(AM_LDFLAGS)
tk_example_LDFLAGS += $(unserding_LIBS)

noinst_PROGRAMS += annul-pf
annul_pf_SOURCES = annul-pf.c
annul_pf_CPPFLAGS = $(AM_CPPFLAGS)
annul_pf_CPPFLAGS += $(uterus_CFLAGS)
annul_pf_CPPFLAGS += $(unserding_CFLAGS)
annul_pf_LDFLAGS = $(AM_LDFLAGS)
annul_pf_LDFLAGS += $(unserding_LIBS)
annul_pf_LDADD =
annul_pf_LDADD += -lm

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
