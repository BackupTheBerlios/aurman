#!/bin/sh -xu

[ -f Makefile ] && make distclean
rm -rf autom4te.cache
rm -f {Makefile.in,Makefile}
rm -f {config.h.in,config.h}
rm -f config.status
rm -f configure
rm -f stamp*
rm -f aclocal.m4
rm -f compile
rm -f libtool

rm -f lib/libalam/{Makefile.in,Makefile}
rm -f src/util/{Makefile.in,Makefile}
rm -f src/aurman/{Makefile.in,Makefile}
rm -f scripts/{Makefile.in,Makefile}
rm -f etc/{Makefile.in,Makefile}
rm -f etc/pacman.d/{Makefile.in,Makefile}
rm -f etc/abs/{Makefile.in,Makefile}
rm -f contrib/{Makefile.in,Makefile}
rm -f doc/{Makefile.in,Makefile}

rm -f doc/html/*.html
rm -f doc/man3/*.3

rm -f po/{Makefile.in,Makefile}
rm -f po/POTFILES
rm -f po/stamp-po
rm -f po/*.gmo

