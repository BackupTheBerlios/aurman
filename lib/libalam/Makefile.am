AUTOMAKE_OPTIONS = gnu

SUBDIRS = po

lib_LTLIBRARIES = libalam.la
include_HEADERS = alam_list.h alam.h

DEFS = -DLOCALEDIR=\"@localedir@\" @DEFS@

AM_CFLAGS = -pedantic -D_GNU_SOURCE

if ENABLE_VISIBILITY_CC
if DARWIN
AM_CFLAGS += -fvisibility=hidden
else
AM_CFLAGS += -fvisibility=internal
endif
endif
if ENABLE_GNU89_INLINE_CC
AM_CFLAGS += -fgnu89-inline
endif

libalam_la_SOURCES = \
	add.h add.c \
	alam.h alam.c \
	alam_list.h alam_list.c \
	backup.h backup.c \
	be_files.c \
	be_package.c \
	cache.h cache.c \
	conflict.h conflict.c \
	db.h db.c \
	delta.h delta.c \
	deps.h deps.c \
	dload.h dload.c \
	error.c \
	graph.h \
	group.h group.c \
	handle.h handle.c \
	log.h log.c \
	md5.h md5.c \
	package.h package.c \
	remove.h remove.c \
	sync.h sync.c \
	trans.h trans.c \
	util.h util.c

libalam_la_LDFLAGS = -no-undefined -version-info $(LIB_VERSION_INFO)
libalam_la_LIBADD = $(LTLIBINTL)

# vim:set ts=2 sw=2 noet:
