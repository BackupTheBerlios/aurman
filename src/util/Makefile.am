# paths set at make time
conffile  = ${sysconfdir}/aurmanrc
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/

bin_PROGRAMS =

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE

# vim:set ts=2 sw=2 noet:
