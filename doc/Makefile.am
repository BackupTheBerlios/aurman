# We have to do some funny stuff here with the manpages. In order to ensure
# a dist tarball doesn't get put out there without manpages, we keep those
# files listed in EXTRA_DIST no matter what. However, we only add them to
# man_MANS if --enable-asciidoc and/or --enable-doxygen are used.

ASCIIDOC_MANS = \
	aurman.8 \
	aurmanrc.5

DOXYGEN_MANS = $(wildcard man3/*.3)

HTML_MANPAGES = \
	aurman.8.html \
	aurmanrc.5.html

HTML_OTHER = \
	index.html \
	submitting-patches.html \
	translation-help.html

HTML_DOCS = \
	$(HTML_MANPAGES) \
	$(HTML_OTHER)

EXTRA_DIST = \
	asciidoc.conf \
	aurman.8.txt \
	aurmanrc.5.txt \
	footer.txt \
	index.txt \
	submitting-patches.txt \
	translation-help.txt \
	Doxyfile \
	$(ASCIIDOC_MANS) \
	$(DOXYGEN_MANS)

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = *.xml $(ASCIIDOC_MANS) $(HTML_DOCS)

# Ensure manpages are fresh when building a dist tarball
dist-hook:
	$(MAKE) $(AM_MAKEFLAGS) clean
	$(MAKE) $(AM_MAKEFLAGS) all

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 | sed s/^v//')-dirty
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

man_MANS =
dist_man_MANS = $(ASCIIDOC_MANS)

if USE_DOXYGEN
man_MANS += $(DOXYGEN_MANS)

all-local: doxygen.in

doxygen.in:
	$(DOXYGEN) $(srcdir)/Doxyfile
endif

html: $(HTML_DOCS)

ASCIIDOC_OPTS = \
	-f asciidoc.conf \
	-a aurman_version="$(REAL_PACKAGE_VERSION)" \
	-a aurman_date="`date +%Y-%m-%d`" \
	-a sysconfdir=$(sysconfdir)

A2X_OPTS = \
	--no-xmllint \
	-d manpage \
	-f manpage \
	--xsltproc-opts='-param man.endnotes.list.enabled 0 -param man.endnotes.are.numbered 0'

# These rules are due to the includes and files of the asciidoc text
$(ASCIIDOC_MANS): asciidoc.conf footer.txt
	a2x $(A2X_OPTS) --asciidoc-opts="$(ASCIIDOC_OPTS)" $@.txt

%.html: %.txt
	asciidoc $(ASCIIDOC_OPTS) -a linkcss $*.txt
	dos2unix $@

# Customizations for certain HTML docs
$(HTML_MANPAGES): asciidoc.conf footer.txt
$(HTML_OTHER): asciidoc.conf
%.8.html: ASCIIDOC_OPTS += -d manpage
%.5.html: ASCIIDOC_OPTS += -d manpage
%.3.html: ASCIIDOC_OPTS += -d manpage

# Dependency rules
aurman.8 aurman.8.html: aurman.8.txt
aurmanrc.5 aurmanrc.5.html: aurmanrc.5.txt
# this one is just a symlink

# vim:set ts=2 sw=2 noet:
