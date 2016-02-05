include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
DISTCLEANFILES =
EXTRA_DIST = $(DISTCLEANFILES)
BUILT_SOURCES =

SUBDIRS += build-aux
SUBDIRS += src

doc_DATA = README
EXTRA_DIST += $(doc_DATA)

pkgconfigdir = @libdir@/pkgconfig
pkgconfig_DATA =
pkgconfig_DATA += unsermarkt.pc
EXTRA_DIST += unsermarkt.pc.in
DISTCLEANFILES += $(pkgconfig_DATA)

DISTCLEANFILES += version.mk
DISTCLEANFILES += .version
EXTRA_DIST += version.mk.in

## pkg-config rule
unsermarkt.pc: .version unsermarkt.pc.in
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver -n \
			--reference .version $(srcdir)/unsermarkt.pc.in | \
		$(SHELL) "$(builddir)/config.status" --file=- > $@

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version

## Makefile.am ends here
