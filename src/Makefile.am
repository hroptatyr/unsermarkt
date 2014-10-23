### Makefile.am
include $(top_builddir)/version.mk

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
SUFFIXES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_HEADERS += boobs.h
noinst_HEADERS += nifty.h
noinst_HEADERS += ud-sock.h
noinst_HEADERS += wrp-debug.h
noinst_HEADERS += um-types.h

bin_PROGRAMS += um-quomon
um_quomon_SOURCES = um-quomon.c um-quomon.yuck
um_quomon_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_quomon_CPPFLAGS += $(libev_CFLAGS)
um_quomon_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_quomon_CPPFLAGS += $(uterus_CFLAGS)
um_quomon_LDFLAGS = $(AM_LDFLAGS)
um_quomon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_quomon_LDFLAGS += $(unserding_LIBS)
um_quomon_LDFLAGS += $(uterus_LIBS)
um_quomon_LDFLAGS += $(libev_LIBS)
um_quomon_LDFLAGS += $(readline_LIBS)
um_quomon_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-quomon.yucc

bin_PROGRAMS += um-apfmon
um_apfmon_SOURCES = um-apfmon.c um-apfmon.yuck
um_apfmon_SOURCES += gq.c gq.h
um_apfmon_CPPFLAGS = $(AM_CPPFLAGS) -D_GNU_SOURCE
um_apfmon_CPPFLAGS += $(libev_CFLAGS)
um_apfmon_CPPFLAGS += $(unserding_CFLAGS)
um_apfmon_CPPFLAGS += $(uterus_CFLAGS)
um_apfmon_LDFLAGS = $(AM_LDFLAGS)
um_apfmon_LDFLAGS += $(AM_LDFLAGS) -lncurses
um_apfmon_LDFLAGS += $(unserding_LIBS)
um_apfmon_LDFLAGS += $(libev_LIBS)
um_apfmon_LDFLAGS += $(readline_LIBS)
BUILT_SOURCES += um-apfmon.yucc

bin_PROGRAMS += um-quod
um_quod_SOURCES = um-quod.c um-quod.h um-quod.yuck
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
BUILT_SOURCES += um-quod.yucc

bin_PROGRAMS += um-apfd
um_apfd_SOURCES = um-apfd.c um-apfd.h um-apfd.yuck
um_apfd_SOURCES += gq.c gq.h
um_apfd_SOURCES += web.c web.h
um_apfd_SOURCES += apfd-cache.h
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
BUILT_SOURCES += um-apfd.yucc

bin_PROGRAMS += um-xmit
um_xmit_SOURCES = um-xmit.c um-xmit.yuck
um_xmit_CPPFLAGS = $(AM_CPPFLAGS)
um_xmit_CPPFLAGS += $(uterus_CFLAGS)
um_xmit_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_xmit_LDFLAGS = $(AM_LDFLAGS)
um_xmit_LDFLAGS += $(uterus_LIBS)
um_xmit_LDFLAGS += $(unserding_LIBS)
um_xmit_LDFLAGS += -static libsvc-uterus.la
BUILT_SOURCES += um-xmit.yucc

if HAVE_LIBEV
bin_PROGRAMS += um-netdania
um_netdania_SOURCES = um-netdania.c um-netdania.h um-netdania.yuck
um_netdania_CPPFLAGS = $(AM_CPPFLAGS)
um_netdania_CPPFLAGS += $(uterus_CFLAGS)
um_netdania_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
um_netdania_CPPFLAGS += $(libev_CFLAGS)
um_netdania_LDFLAGS = $(AM_LDFLAGS)
um_netdania_LDFLAGS += $(unserding_LIBS)
um_netdania_LDFLAGS += $(uterus_LIBS)
um_netdania_LDFLAGS += $(libev_LIBS)
um_netdania_LDFLAGS += -static libsvc-uterus.la
endif  ## HAVE_LIBEV
BUILT_SOURCES += um-netdania.yucc


noinst_PROGRAMS += ccy-graph
ccy_graph_SOURCES = ccy-graph.c ccy-graph.h
ccy_graph_SOURCES += iso4217.c iso4217.h
ccy_graph_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE

if HAVE_LIBEV
noinst_PROGRAMS += xross-quo
xross_quo_SOURCES = xross-quo.c xross-quo.yuck
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
BUILT_SOURCES += xross-quo.yucc

unsermod_LTLIBRARIES += svc-uterus.la
svc_uterus_la_SOURCES = svc-uterus.c svc-uterus.h
svc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS) -DUNSERMON_DSO
svc_uterus_la_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
svc_uterus_la_CPPFLAGS += $(uterus_CFLAGS)
svc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS) $(AM_MODFLAGS)
svc_uterus_la_LDFLAGS += $(uterus_LIBS)

unsermod_LTLIBRARIES += libsvc-uterus.la
unserinc_HEADERS += svc-uterus.h
libsvc_uterus_la_SOURCES = svc-uterus.c svc-uterus.h
libsvc_uterus_la_CPPFLAGS = $(AM_CPPFLAGS)
libsvc_uterus_la_CPPFLAGS += $(unserding_CFLAGS) -DUD_NEW_API
libsvc_uterus_la_CPPFLAGS += $(uterus_CFLAGS)
libsvc_uterus_la_LDFLAGS = $(AM_LDFLAGS) $(XCCLDFLAGS)
libsvc_uterus_la_LDFLAGS += $(unserding_LIBS)


## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

## our rule for gperf
SUFFIXES += .gperf
.gperf.c:
	$(AM_V_GEN) $(GPERF) -L ANSI-C $< --output-file $@

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
