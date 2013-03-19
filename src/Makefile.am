### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =
AM_MODFLAGS = -module -shared -avoid-version

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
unsermod_LTLIBRARIES =
unserinc_HEADERS =
noinst_LTLIBRARIES =
noinst_HEADERS =

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_HEADERS += boobs.h
noinst_HEADERS += nifty.h
noinst_HEADERS += ud-sock.h
noinst_HEADERS += wrp-debug.h
noinst_HEADERS += um-types.h

bin_PROGRAMS += um-quomon
um_quomon_SOURCES = um-quomon.c um-quomon-clo.ggo
um_quomon_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quomon_CPPFLAGS += $(libev_CFLAGS)
um_quomon_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_quomon_CPPFLAGS += $(uterus_CFLAGS)
um_quomon_LDFLAGS = $(AM_LDFLAGS)
um_quomon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_quomon_LDFLAGS += $(unserding_LIBS)
um_quomon_LDFLAGS += $(libev_LIBS)
um_quomon_LDFLAGS += -lreadline
um_quomon_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-quomon-clo.c um-quomon-clo.h

noinst_PROGRAMS += um-quodmp
um_quodmp_SOURCES = um-quodmp.c um-quodmp-clo.ggo
um_quodmp_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quodmp_CPPFLAGS += $(libev_CFLAGS)
um_quodmp_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_quodmp_CPPFLAGS += $(uterus_CFLAGS)
um_quodmp_LDFLAGS = $(AM_LDFLAGS)
um_quodmp_LDFLAGS += $(uterus_LIBS)
um_quodmp_LDFLAGS += $(unserding_LIBS)
um_quodmp_LDFLAGS += $(libev_LIBS)
um_quodmp_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-quodmp-clo.c um-quodmp-clo.h

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
BUILT_SOURCES += um-apfmon-clo.c um-apfmon-clo.h

bin_PROGRAMS += um-quod
um_quod_SOURCES = um-quod.c um-quod.h
um_quod_SOURCES += um-quod-clo.ggo
um_quod_SOURCES += gq.c gq.h
um_quod_SOURCES += web.c web.h
um_quod_SOURCES += quod-cache.h
um_quod_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quod_CPPFLAGS += -DWEB_ASP_QUOTREQ
um_quod_CPPFLAGS += $(libev_CFLAGS)
um_quod_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_quod_CPPFLAGS += $(uterus_CFLAGS)
um_quod_CPPFLAGS += $(fixc_CFLAGS)
um_quod_LDFLAGS = $(AM_LDFLAGS)
um_quod_LDFLAGS += $(uterus_LIBS)
um_quod_LDFLAGS += $(unserding_LIBS)
um_quod_LDFLAGS += $(fixc_LIBS)
um_quod_LDFLAGS += $(libev_LIBS)
um_quod_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-quod-clo.c um-quod-clo.h

bin_PROGRAMS += um-apfd
um_apfd_SOURCES = um-apfd.c um-apfd.h
um_apfd_SOURCES += um-apfd-clo.ggo
um_apfd_SOURCES += gq.c gq.h
um_apfd_SOURCES += web.c web.h
um_apfd_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_apfd_CPPFLAGS += -DWEB_ASP_REQFORPOSS
um_apfd_CPPFLAGS += $(libev_CFLAGS)
um_apfd_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_apfd_CPPFLAGS += $(uterus_CFLAGS)
um_apfd_CPPFLAGS += $(fixc_CFLAGS)
um_apfd_LDFLAGS = $(AM_LDFLAGS)
um_apfd_LDFLAGS += $(uterus_LIBS)
um_apfd_LDFLAGS += $(unserding_LIBS)
um_apfd_LDFLAGS += $(fixc_LIBS)
um_apfd_LDFLAGS += $(libev_LIBS)
um_apfd_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-apfd-clo.c um-apfd-clo.h

bin_PROGRAMS += um-xmit
um_xmit_SOURCES = um-xmit.c
BUILT_SOURCES += um-xmit-clo.c um-xmit-clo.h
um_xmit_CPPFLAGS = $(AM_CPPFLAGS)
um_xmit_CPPFLAGS += $(uterus_CFLAGS)
um_xmit_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_xmit_LDFLAGS = $(AM_LDFLAGS)
um_xmit_LDFLAGS += $(uterus_LIBS)
um_xmit_LDFLAGS += $(unserding_LIBS)
um_xmit_LDFLAGS += -static libsvc-uterus.la

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
um_netdania_LDFLAGS += -static libsvc-uterus.la
endif  ## HAVE_LIBEV
BUILT_SOURCES += um-netdania-clo.c um-netdania-clo.h


noinst_PROGRAMS += ccy-graph
ccy_graph_SOURCES = ccy-graph.c ccy-graph.h
ccy_graph_SOURCES += iso4217.c iso4217.h
ccy_graph_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE

if HAVE_LIBEV
noinst_PROGRAMS += xross-quo
xross_quo_SOURCES = xross-quo.c xross-quo-clo.ggo
xross_quo_SOURCES += ccy-graph.h
xross_quo_SOURCES += iso4217.c iso4217.h
xross_quo_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
xross_quo_CPPFLAGS += $(libev_CFLAGS)
xross_quo_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
xross_quo_CPPFLAGS += $(uterus_CFLAGS)
xross_quo_LDFLAGS = $(AM_LDFLAGS)
xross_quo_LDFLAGS += $(uterus_LIBS)
xross_quo_LDFLAGS += $(unserding_LIBS)
xross_quo_LDFLAGS += $(libev_LIBS)
xross_quo_LDFLAGS += -static libsvc-uterus.la
endif  HAVE_LIBEV
BUILT_SOURCES += xross-quo-clo.c xross-quo-clo.h

unsermod_LTLIBRARIES += svc-uterus.la
svc_uterus_la_SOURCES = svc-uterus.c svc-uterus.h
svc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_uterus_la_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
svc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) $(AM_MODFLAGS)

unsermod_LTLIBRARIES += libsvc-uterus.la
unserinc_HEADERS += svc-uterus.h
libsvc_uterus_la_SOURCES = svc-uterus.c svc-uterus.h
libsvc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS)
libsvc_uterus_la_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
libsvc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
libsvc_uterus_la_LDFLAGS += $(unserding_LIBS)


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
