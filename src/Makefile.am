### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
AM_MODFLAGS = -module -avoid-version

headerdir = $(includedir)/unserding

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
unsermod_LTLIBRARIES =
noinst_LTLIBRARIES =
noinst_HEADERS =
header_HEADERS =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_HEADERS += boobs.h
noinst_HEADERS += nifty.h
noinst_HEADERS += ud-sock.h
noinst_HEADERS += wrp-debug.h

noinst_PROGRAMS += umoc
umoc_LDFLAGS = $(unserding_LDFLAGS)
umoc_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
umoc_CPPFLAGS += $(unserding_CFLAGS)
umoc_CPPFLAGS += $(uterus_CFLAGS)

noinst_PROGRAMS += oq
oq_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
oq_CPPFLAGS += $(unserding_CFLAGS) $(uterus_CFLAGS)

if HAVE_SQLITE
noinst_PROGRAMS += uschi
uschi_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
uschi_CPPFLAGS += $(sqlite_CFLAGS)
uschi_CPPFLAGS += $(unserding_CFLAGS)
uschi_CPPFLAGS += $(uterus_CFLAGS)
uschi_LDFLAGS = $(AM_LDFLAGS)
uschi_LDFLAGS += $(sqlite_LIBS)
endif  ## HAVE_SQLITE

if HAVE_MXML
noinst_PROGRAMS += blood-mm
blood_mm_LDFLAGS =
blood_mm_CPPFLAGS = $(AM_CPPFLAGS)
blood_mm_CPPFLAGS += $(unserding_CFLAGS)
blood_mm_CPPFLAGS += $(uterus_CFLAGS)
blood_mm_CPPFLAGS += $(mxml_CFLAGS)
blood_mm_LDFLAGS += $(mxml_LIBS)
endif  ## HAVE_MXML

if HAVE_LIBEV
bin_PROGRAMS += um-quomon
um_quomon_SOURCES = um-quomon.c um-quomon-clo.ggo
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

bin_PROGRAMS += um-quosnp
um_quosnp_SOURCES = um-quosnp.c um-quosnp-clo.ggo
um_quosnp_SOURCES += gq.c gq.h
um_quosnp_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quosnp_CPPFLAGS += $(libev_CFLAGS)
um_quosnp_CPPFLAGS += $(unserding_CFLAGS)
um_quosnp_CPPFLAGS += $(uterus_CFLAGS)
um_quosnp_CPPFLAGS += $(fixc_CFLAGS)
um_quosnp_LDFLAGS = $(AM_LDFLAGS)
um_quosnp_LDFLAGS += $(uterus_LIBS)
um_quosnp_LDFLAGS += $(unserding_LIBS)
um_quosnp_LDFLAGS += $(fixc_LIBS)
um_quosnp_LDFLAGS += $(libev_LIBS)
endif  ## HAVE_LIBEV
BUILT_SOURCES += um-quomon-clo.c um-quomon-clo.h
BUILT_SOURCES += um-apfmon-clo.c um-apfmon-clo.h
BUILT_SOURCES += um-quodmp-clo.c um-quodmp-clo.h
BUILT_SOURCES += um-quosnp-clo.c um-quosnp-clo.h

bin_PROGRAMS += um-xmit
um_xmit_SOURCES = um-xmit.c
BUILT_SOURCES += um-xmit-clo.c um-xmit-clo.h
um_xmit_CPPFLAGS = $(AM_CPPFLAGS)
um_xmit_CPPFLAGS += $(uterus_CFLAGS)
um_xmit_CPPFLAGS += $(unserding_CFLAGS)
um_xmit_LDFLAGS = $(AM_LDFLAGS)
um_xmit_LDFLAGS += $(uterus_LIBS)
um_xmit_LDFLAGS += $(unserding_LIBS)

if HAVE_LIBEV
noinst_PROGRAMS += ox-matchall
ox_matchall_SOURCES = ox-matchall.c
ox_matchall_CPPFLAGS = $(AM_CPPFLAGS)
ox_matchall_CPPFLAGS += $(uterus_CFLAGS)
ox_matchall_CPPFLAGS += $(unserding_CFLAGS)
ox_matchall_CPPFLAGS += $(libev_CFLAGS)
ox_matchall_LDFLAGS = $(AM_LDFLAGS)
ox_matchall_LDFLAGS += $(uterus_LIBS)
ox_matchall_LDFLAGS += $(unserding_LIBS)
ox_matchall_LDFLAGS += $(libev_LIBS)
endif  ## HAVE_LIBEV
BUILT_SOURCES += ox-matchall-clo.c ox-matchall-clo.h

if HAVE_TWSTOOLS
noinst_LTLIBRARIES += tws-snarf.la
tws_snarf_la_LDFLAGS = -module -rpath /none
tws_snarf_la_SOURCES = tws-snarf.cpp
tws_snarf_la_CPPFLAGS = $(AM_CPPFLAGS)
tws_snarf_la_CPPFLAGS += $(twstools_CFLAGS)
tws_snarf_la_CPPFLAGS += $(uterus_CFLAGS)
tws_snarf_la_CPPFLAGS += $(unserding_CFLAGS)
tws_snarf_la_LDFLAGS += $(unserding_LIBS)
## grrr, automake passes --tag=CXX to libtool

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

if HAVE_LIBEV
bin_PROGRAMS += um-netdania
um_netdania_SOURCES = um-netdania.c um-netdania.h um-netdania-clo.ggo
um_netdania_CPPFLAGS = $(AM_CPPFLAGS)
um_netdania_CPPFLAGS += $(uterus_CFLAGS)
um_netdania_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_netdania_CPPFLAGS += $(libev_CFLAGS)
um_netdania_LDFLAGS = $(AM_LDFLAGS)
um_netdania_LDFLAGS += $(unserding_LIBS)
um_netdania_LDFLAGS += $(libev_LIBS)
endif  ## HAVE_LIBEV
BUILT_SOURCES += um-netdania-clo.c um-netdania-clo.h

noinst_PROGRAMS += tk-example
tk_example_SOURCES = tk-example.c
tk_example_CPPFLAGS = $(AM_CPPFLAGS)
tk_example_CPPFLAGS += $(uterus_CFLAGS)
tk_example_CPPFLAGS += $(unserding_CFLAGS)
tk_example_LDFLAGS = $(AM_LDFLAGS)
tk_example_LDFLAGS += $(unserding_LIBS)

noinst_PROGRAMS += annul-pf
annul_pf_SOURCES = annul-pf.c
annul_pf_SOURCES += iso4217.c iso4217.h
annul_pf_SOURCES += ccy-graph.h
annul_pf_CPPFLAGS = $(AM_CPPFLAGS)
annul_pf_CPPFLAGS += $(uterus_CFLAGS)
annul_pf_CPPFLAGS += $(unserding_CFLAGS)
annul_pf_LDFLAGS = $(AM_LDFLAGS)
annul_pf_LDFLAGS += $(unserding_LIBS)
annul_pf_LDADD =
annul_pf_LDADD += -lm

if HAVE_TWSAPI
if HAVE_LIBEV
noinst_PROGRAMS += annul-virtfx
annul_virtfx_SOURCES = annul-virtfx.cpp
annul_virtfx_SOURCES += gq.c gq.h
annul_virtfx_CPPFLAGS = $(AM_CPPFLAGS)
annul_virtfx_CPPFLAGS += $(uterus_CFLAGS)
annul_virtfx_CPPFLAGS += $(twsapi_CFLAGS)
annul_virtfx_CPPFLAGS += $(libev_CFLAGS)
annul_virtfx_LDFLAGS = $(AM_LDFLAGS)
annul_virtfx_LDFLAGS += $(uterus_LIBS)
annul_virtfx_LDFLAGS += $(twsapi_LIBS) -lstdc++
annul_virtfx_LDFLAGS += $(libev_LIBS)
annul_virtfx_LDADD =
annul_virtfx_LDADD += -lm
endif  HAVE_LIBEV
endif  HAVE_TWSAPI
BUILT_SOURCES += annul-virtfx-clo.c annul-virtfx-clo.h


noinst_PROGRAMS += ccy-graph
ccy_graph_SOURCES = ccy-graph.c ccy-graph.h
ccy_graph_SOURCES += iso4217.c iso4217.h
ccy_graph_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE

if HAVE_LIBEV
noinst_PROGRAMS += xross-quo
xross_quo_SOURCES = xross-quo.c xross-quo-clo.ggo
xross_quo_SOURCES += ccy-graph.h
xross_quo_SOURCES += iso4217.c iso4217.h
xross_quo_SOURCES += svc-uterus.c svc-uterus.h
xross_quo_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
xross_quo_CPPFLAGS += $(libev_CFLAGS)
xross_quo_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
xross_quo_CPPFLAGS += $(uterus_CFLAGS)
xross_quo_LDFLAGS = $(AM_LDFLAGS)
xross_quo_LDFLAGS += $(uterus_LIBS)
xross_quo_LDFLAGS += $(unserding_LIBS)
xross_quo_LDFLAGS += $(libev_LIBS)
endif  HAVE_LIBEV
BUILT_SOURCES += xross-quo-clo.c xross-quo-clo.h

unsermod_LTLIBRARIES += svc-uterus.la
svc_uterus_la_SOURCES = svc-uterus.c svc-uterus.h
svc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_uterus_la_CPPFLAGS += -DUD_NEW_API
svc_uterus_la_CPPFLAGS += $(unserding_CFLAGS)
svc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) $(AM_MODFLAGS)


## our rule for gengetopt
%.c %.h: %.ggo
	$(AM_V_GEN) gengetopt -l -i $< -F $*

## our rule for gperf
%.c: %.gperf
	$(AM_V_GEN) $(GPERF) $< --output-file $@

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
