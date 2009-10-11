dist_sysconf_DATA = aurman.conf
EXTRA_DIST = aurman.conf.in

# Files that should be removed, but which Automake does not know.
MOSTLYCLEANFILES = $(dist_sysconf_DATA)

#### Taken from the autoconf scripts Makefile.am ####
edit = sed \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@PACKAGE_NAME[@]|$(PACKAGE_NAME)|g' \
	-e 's|@PKGEXT[@]|$(PKGEXT)|g' \
	-e 's|@SRCEXT[@]|$(SRCEXT)|g' \
	-e 's|@CARCH[@]|$(CARCH)|g' \
	-e 's|@CHOST[@]|$(CHOST)|g' \
	-e 's|@ARCHSWITCH[@]|$(ARCHSWITCH)|g' \
	-e 's|@CARCHFLAGS[@]|$(CARCHFLAGS)|g' \
	-e 's|@ROOTDIR[@]|$(ROOTDIR)|g'

$(dist_sysconf_DATA): Makefile
	@echo '    ' GEN $@;
	@rm -f $@ $@.tmp
	@$(edit) `test -f ./$@.in || echo $(srcdir)/`$@.in >$@.tmp
	@mv $@.tmp $@

makepkg.conf: $(srcdir)/aurman.conf.in

# vim:set ts=2 sw=2 noet:
