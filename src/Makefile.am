### Makefile.am

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_LDFLAGS =

headerdir = $(includedir)/unserding
moddir = $(libdir)/unserding

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
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
if HAVE_SQLITE
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
dso_unsermarkt_la_CPPFLAGS += $(sqlite_CFLAGS)
dso_unsermarkt_la_LDFLAGS += $(mxml_LIBS)
dso_unsermarkt_la_LDFLAGS += $(sqlite_LIBS)
EXTRA_dso_unsermarkt_la_SOURCES =
EXTRA_dso_unsermarkt_la_SOURCES += con6ity.c
EXTRA_dso_unsermarkt_la_SOURCES += mmls.c
endif  ## HAVE_SQLITE
endif  ## HAVE_MXML

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
endif  ## HAVE_LIBEV
BUILT_SOURCES += um-quomon-clo.c um-quomon-clo.h
BUILT_SOURCES += um-apfmon-clo.c um-apfmon-clo.h
BUILT_SOURCES += um-quodmp-clo.c um-quodmp-clo.h

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
tws_snarf_la_LINK = $(LINK) $(tws_snarf_la_LDFLAGS)

noinst_LTLIBRARIES += tws-order.la
tws_order_la_LDFLAGS = -module -rpath /none
tws_order_la_SOURCES = tws-order.cpp
tws_order_la_SOURCES += iso4217.c iso4217.h
tws_order_la_CPPFLAGS = $(AM_CPPFLAGS)
tws_order_la_CPPFLAGS += $(twstools_CFLAGS)
tws_order_la_CPPFLAGS += $(uterus_CFLAGS)
tws_order_la_CPPFLAGS += $(unserding_CFLAGS)
tws_order_la_LDFLAGS += $(unserding_LIBS)
tws_order_la_LINK = $(LINK) $(tws_snarf_la_LDFLAGS)
endif  ## HAVE_TWSTOOLS

if HAVE_TWSAPI
if HAVE_LIBEV
noinst_PROGRAMS += ox-tws
EXTRA_ox_tws_SOURCES =
ox_tws_SOURCES = ox-tws.c ox-tws-clo.ggo
ox_tws_SOURCES += ox-tws-wrapper.cpp ox-tws-wrapper.h
ox_tws_SOURCES += ox-tws-contract-glue.cpp
ox_tws_SOURCES += iso4217.c iso4217.h
ox_tws_SOURCES += ox-tws-private.h
ox_tws_SOURCES += wrp-debug.h
EXTRA_ox_tws_SOURCES += gq.c gq.h
ox_tws_CPPFLAGS = $(AM_CPPFLAGS)
ox_tws_CPPFLAGS += $(uterus_CFLAGS)
ox_tws_CPPFLAGS += $(unserding_CFLAGS)
ox_tws_CPPFLAGS += $(twsapi_CFLAGS)
ox_tws_CPPFLAGS += $(libev_CFLAGS)
ox_tws_LDFLAGS = $(AM_LDFLAGS)
ox_tws_LDFLAGS += $(unserding_LIBS)
ox_tws_LDFLAGS += $(twsapi_LIBS) -lstdc++
ox_tws_LDFLAGS += $(libev_LIBS)
ox_tws_LINK = $(LINK) $(ox_tws_LDFLAGS)

noinst_PROGRAMS += pf-tws
EXTRA_pf_tws_SOURCES =
pf_tws_SOURCES = pf-tws.c pf-tws-clo.ggo
pf_tws_SOURCES += pf-tws-wrapper.cpp pf-tws-wrapper.h
pf_tws_SOURCES += iso4217.c iso4217.h
pf_tws_SOURCES += strops.h
pf_tws_SOURCES += pf-tws-private.h
pf_tws_SOURCES += wrp-debug.h
pf_tws_CPPFLAGS = $(AM_CPPFLAGS)
pf_tws_CPPFLAGS += $(uterus_CFLAGS)
pf_tws_CPPFLAGS += $(unserding_CFLAGS)
pf_tws_CPPFLAGS += $(twsapi_CFLAGS)
pf_tws_CPPFLAGS += $(libev_CFLAGS)
pf_tws_LDFLAGS = $(AM_LDFLAGS)
pf_tws_LDFLAGS += $(unserding_LIBS)
pf_tws_LDFLAGS += $(twsapi_LIBS) -lstdc++
pf_tws_LDFLAGS += $(libev_LIBS)
pf_tws_LINK = $(LINK) $(pf_tws_LDFLAGS)

if HAVE_EXPAT
if HAVE_GPERF
noinst_PROGRAMS += quo-tws
EXTRA_quo_tws_SOURCES =
quo_tws_SOURCES = quo-tws.c quo-tws-clo.ggo
quo_tws_SOURCES += ox-tws-contract-glue.cpp
quo_tws_SOURCES += iso4217.c iso4217.h
quo_tws_SOURCES += strops.h
quo_tws_SOURCES += quo-tws-private.h
quo_tws_SOURCES += wrp-debug.h
quo_tws_SOURCES += prchunk.c prchunk.h
EXTRA_quo_tws_SOURCES += proto-twsxml-tag.gperf
EXTRA_quo_tws_SOURCES += proto-twsxml-attr.gperf proto-twsxml-attr.h
EXTRA_quo_tws_SOURCES += proto-tx-ns.gperf proto-tx-ns.h
quo_tws_CPPFLAGS = $(AM_CPPFLAGS)
quo_tws_CPPFLAGS += $(uterus_CFLAGS)
quo_tws_CPPFLAGS += $(unserding_CFLAGS)
quo_tws_CPPFLAGS += $(twsapi_CFLAGS)
quo_tws_CPPFLAGS += $(libev_CFLAGS)
quo_tws_CPPFLAGS += $(expat_CFLAGS)
quo_tws_LDFLAGS = $(AM_LDFLAGS)
quo_tws_LDFLAGS += $(uterus_LIBS)
quo_tws_LDFLAGS += $(unserding_LIBS)
quo_tws_LDFLAGS += $(twsapi_LIBS) -lstdc++
quo_tws_LDFLAGS += $(libev_LIBS)
quo_tws_LDFLAGS += $(expat_LIBS)
quo_tws_LDADD = libtwscapi.la
quo_tws_LINK = $(LINK) $(quo_tws_LDFLAGS)
endif  ## HAVE_GPERF
endif  ## HAVE_EXPAT
endif  ## HAVE_LIBEV
endif  ## HAVE_TWSAPI
BUILT_SOURCES += ox-tws-clo.c ox-tws-clo.h
BUILT_SOURCES += pf-tws-clo.c pf-tws-clo.h
BUILT_SOURCES += quo-tws-clo.c quo-tws-clo.h

if HAVE_LIBEV
bin_PROGRAMS += um-netdania
um_netdania_SOURCES = um-netdania.c um-netdania.h um-netdania-clo.ggo
um_netdania_CPPFLAGS = $(AM_CPPFLAGS)
um_netdania_CPPFLAGS += $(uterus_CFLAGS)
um_netdania_CPPFLAGS += $(unserding_CFLAGS)
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
annul_virtfx_LINK = $(LINK) $(annul_virtfx_LDFLAGS)
endif  HAVE_LIBEV
endif  HAVE_TWSAPI


noinst_PROGRAMS += ccy-graph
ccy_graph_SOURCES = ccy-graph.c ccy-graph.h
ccy_graph_SOURCES += iso4217.c iso4217.h
ccy_graph_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE

if HAVE_TWSAPI
noinst_LTLIBRARIES += libtwscapi.la
libtwscapi_la_SOURCES = gen-tws.c gen-tws.h
libtwscapi_la_SOURCES += gen-tws-wrapper.cpp
if HAVE_EXPAT
if HAVE_GPERF
libtwscapi_la_SOURCES += gen-tws-cont.c gen-tws-cont.h
libtwscapi_la_SOURCES += gen-tws-cont-glu.cpp gen-tws-cont-glu.h
libtwscapi_la_SOURCES += proto-twsxml-tag.gperf proto-twsxml-tag.h
libtwscapi_la_SOURCES += proto-twsxml-attr.gperf proto-twsxml-attr.h
libtwscapi_la_SOURCES += proto-tx-ns.gperf proto-tx-ns.h
libtwscapi_la_SOURCES += proto-twsxml-reqtyp.gperf proto-twsxml-reqtyp.h
libtwscapi_la_SOURCES += proto-fixml-tag.gperf proto-fixml-tag.h
libtwscapi_la_SOURCES += proto-fixml-attr.gperf proto-fixml-attr.h
endif  HAVE_GPERF
endif  HAVE_EXPAT
libtwscapi_la_SOURCES +=
libtwscapi_la_CPPFLAGS = $(AM_CPPFLAGS)
libtwscapi_la_CPPFLAGS += $(twsapi_CFLAGS)
libtwscapi_la_LDFLAGS = $(AM_LDFLAGS) -rpath /none
libtwscapi_la_LDFLAGS += $(twsapi_LIBS) -lstdc++
if HAVE_EXPAT
libtwscapi_la_CPPFLAGS += $(expat_CFLAGS)
libtwscapi_la_LDFLAGS += $(expat_LIBS)
endif  HAVE_EXPAT
libtwscapi_la_LINK = $(LINK) $(libtwscapi_la_LDFLAGS)
if HAVE_GPERF
BUILT_SOURCES += proto-twsxml-tag.c
BUILT_SOURCES += proto-twsxml-attr.c
BUILT_SOURCES += proto-tx-ns.c
BUILT_SOURCES += proto-twsxml-reqtyp.c
BUILT_SOURCES += proto-fixml-tag.c
BUILT_SOURCES += proto-fixml-attr.c
endif  HAVE_GPERF

noinst_PROGRAMS += gen-tws
gen_tws_SOURCES = gen-tws.c
gen_tws_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
gen_tws_LDADD = libtwscapi.la
gen_tws_LDFLAGS = $(AM_LDFLAGS)
gen_tws_LINK = $(LINK) $(gen_tws_LDFLAGS)
endif  HAVE_TWSAPI


## our rule for gengetopt
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

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
