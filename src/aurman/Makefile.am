# paths set at make time
conffile  = ${sysconfdir}/aurman.conf
dbpath    = ${localstatedir}/lib/aurman/
cachedir  = ${localstatedir}/cache/aurman/pkg/
logfile   = ${localstatedir}/log/aurman.log

bin_PROGRAMS = aurman

DEFS = -DLOCALEDIR=\"@localedir@\" \
       -DCONFFILE=\"$(conffile)\" \
       -DROOTDIR=\"$(ROOTDIR)\" \
       -DDBPATH=\"$(dbpath)\" \
       -DCACHEDIR=\"$(cachedir)\" \
       -DLOGFILE=\"$(logfile)\" \
       @DEFS@
INCLUDES = -I$(top_srcdir)/lib/libalam

AM_CFLAGS = -pedantic -D_GNU_SOURCE -lcurl

if USE_GIT_VERSION
GIT_VERSION := $(shell sh -c 'git describe --abbrev=4 | sed s/^v//')-dirty
DEFS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

aurman_SOURCES = \
	conf.h conf.c \
	db.h db.c \
	aurman.h aurman.c \
	util.h util.c


LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalam/.libs/libalam.la

# vim:set ts=2 sw=2 noet:
