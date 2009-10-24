
ACLOCAL_AMFLAGS = -I m4

SUBDIRS =  lib/libalam src/util src/aurman scripts etc po contrib
if WANT_DOC
SUBDIRS += doc
endif

# Make sure we test and build manpages when doing distcheck
DISTCHECK_CONFIGURE_FLAGS = --enable-doc --disable-git-version

# Some files automatically included, so they aren't specified below:
#   AUTHORS, COPYING, NEWS, README
EXTRA_DIST = HACKING

# Sample makepkg prototype files
pkgdatadir = ${datadir}/${PACKAGE}
dist_pkgdata_DATA =  ChangeLog.proto

# create the aurman DB and cache directories upon install
install-data-local:
	for dir in "$(DESTDIR)$(localstatedir)/lib/aurman" "$(DESTDIR)$(localstatedir)/cache/aurman/pkg"; do \
	  test -z "$$dir" || $(MKDIR_P) "$$dir"; \
	done

# vim:set ts=2 sw=2 noet:
