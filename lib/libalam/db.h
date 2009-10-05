/*
 *  db.h
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
#ifndef _ALAM_DB_H
#define _ALAM_DB_H

#include "alam.h"
#include <limits.h>
#include <time.h>

/* Database entries */
typedef enum _amdbinfrq_t {
	INFRQ_BASE = 0x01,
	INFRQ_DESC = 0x02,
	INFRQ_DEPENDS = 0x04,
	INFRQ_FILES = 0x08,
	INFRQ_SCRIPTLET = 0x10,
	INFRQ_DELTAS = 0x20,
	/* ALL should be sum of all above */
	INFRQ_ALL = 0x3F
} amdbinfrq_t;

/* Database */
struct __amdb_t {
	char *path;
	char *treename;
	unsigned short pkgcache_loaded;
	alam_list_t *pkgcache;
	unsigned short grpcache_loaded;
	alam_list_t *grpcache;
	alam_list_t *servers;
};

/* db.c, database general calls */
amdb_t *_alam_db_new(const char *dbpath, const char *treename);
void _alam_db_free(amdb_t *db);
int _alam_db_cmp(const void *d1, const void *d2);
alam_list_t *_alam_db_search(amdb_t *db, const alam_list_t *needles);
amdb_t *_alam_db_register_local(void);
amdb_t *_alam_db_register_sync(const char *treename);

/* be.c, backend specific calls */
int _alam_db_populate(amdb_t *db);
int _alam_db_read(amdb_t *db, ampkg_t *info, amdbinfrq_t inforeq);
int _alam_db_prepare(amdb_t *db, ampkg_t *info);
int _alam_db_write(amdb_t *db, ampkg_t *info, amdbinfrq_t inforeq);
int _alam_db_remove(amdb_t *db, ampkg_t *info);

#endif /* _ALAM_DB_H */

/* vim: set ts=2 sw=2 noet: */
