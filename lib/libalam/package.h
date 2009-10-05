/*
 *  package.h
 *
 *  Copyright (c) 2009 Laszlo Papp <djszapi@archlinux.us>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ALAM_PACKAGE_H
#define _ALAM_PACKAGE_H

#include <sys/types.h> /* off_t */
#include <time.h> /* time_t */

#include "alam.h"
#include "db.h"

typedef enum _ampkgfrom_t {
	PKG_FROM_CACHE = 1,
	PKG_FROM_FILE
} ampkgfrom_t;

struct __ampkg_t {
	char *filename;
	char *name;
	char *version;
	char *desc;
	char *url;
	time_t builddate;
	time_t installdate;
	char *packager;
	char *md5sum;
	char *arch;
	off_t size;
	off_t isize;
	off_t download_size;
	unsigned short scriptlet;
	unsigned short force;
	ampkgreason_t reason;
	alam_list_t *licenses;
	alam_list_t *replaces;
	alam_list_t *groups;
	alam_list_t *files;
	alam_list_t *backup;
	alam_list_t *depends;
	alam_list_t *optdepends;
	alam_list_t *conflicts;
	alam_list_t *provides;
	alam_list_t *deltas;
	alam_list_t *delta_path;
	alam_list_t *removes; /* in transaction targets only */
	/* internal */
	ampkgfrom_t origin;
	/* Replaced 'void *data' with this union as follows:
  origin == PKG_FROM_CACHE, use pkg->origin_data.db
  origin == PKG_FROM_FILE, use pkg->origin_data.file
	*/
  union {
		amdb_t *db;
		char *file;
	} origin_data;
	amdbinfrq_t infolevel;
};

ampkg_t* _alam_pkg_new(void);
ampkg_t *_alam_pkg_dup(ampkg_t *pkg);
void _alam_pkg_free(ampkg_t *pkg);
void _alam_pkg_free_trans(ampkg_t *pkg);
int _alam_pkg_cmp(const void *p1, const void *p2);
int _alam_pkg_compare_versions(ampkg_t *local_pkg, ampkg_t *pkg);
ampkg_t *_alam_pkg_find(alam_list_t *haystack, const char *needle);
int _alam_pkg_should_ignore(ampkg_t *pkg);

#endif /* _ALAM_PACKAGE_H */

/* vim: set ts=2 sw=2 noet: */
