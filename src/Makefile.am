

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
noinst_PROGRAMS += um-order

dso_oq_la_LDFLAGS = -module
dso_oq_la_LDFLAGS += $(unserding_LDFLAGS)
dso_oq_la_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)

um_order_LDFLAGS = $(unserding_LDFLAGS)
um_order_CPPFLAGS = $(AM_CPPFLAGS) $(unserding_CFLAGS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
