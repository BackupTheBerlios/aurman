# paths set at make time
conffile  = ${sysconfdir}/aurmanrc
dbpath    = ${localstatedir}/lib/pacman/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/aurman.log

bin_PROGRAMS = aurman

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       -DLOGFILE=\"$(logfile)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalpm

AM_CFLAGS = -pedantic -D_GNU_SOURCE -lcurl -larchive -lalpm

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 | sed s/^v//')-dirty
DEFS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

aurman_SOURCES = \
	conf.h conf.c \
	aurman.h aurman.c \
	util.h util.c


## LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalpm/.libs/libalpm.la

# vim:set ts=2 sw=2 noet:
