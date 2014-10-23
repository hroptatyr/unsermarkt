include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4
EXTRA_DIST =

SUBDIRS =
DISTCLEANFILES =
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
EXTRA_DIST += version.mk.in

BUILT_SOURCES += .version
EXTRA_DIST += $(BUILT_SOURCES)

## pkg-config rule
unsermarkt.pc: .version unsermarkt.pc.in
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver -n \
			--reference .version $(srcdir)/unsermarkt.pc.in | \
		$(SHELL) "$(builddir)/config.status" --file=- > $@

## .version rule
.version:
	$(AM_V_GEN) echo "v$(VERSION)" > $@

## make sure .version is read-only in the dist
dist-hook:
	chmod ugo-w $(distdir)/.version

## Makefile.am ends here
