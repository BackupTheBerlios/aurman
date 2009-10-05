/*
 *  cache.h
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
#ifndef _ALAM_CACHE_H
#define _ALAM_CACHE_H

#include "db.h"
#include "alam_list.h"
#include "group.h"
#include "package.h"

/* packages */
int _alam_db_load_pkgcache(amdb_t *db);
void _alam_db_free_pkgcache(amdb_t *db);
int _alam_db_add_pkgincache(amdb_t *db, ampkg_t *pkg);
int _alam_db_remove_pkgfromcache(amdb_t *db, ampkg_t *pkg);
alam_list_t *_alam_db_get_pkgcache(amdb_t *db);
int _alam_db_ensure_pkgcache(amdb_t *db, amdbinfrq_t infolevel);
ampkg_t *_alam_db_get_pkgfromcache(amdb_t *db, const char *target);
/* groups */
int _alam_db_load_grpcache(amdb_t *db);
void _alam_db_free_grpcache(amdb_t *db);
alam_list_t *_alam_db_get_grpcache(amdb_t *db);
amgrp_t *_alam_db_get_grpfromcache(amdb_t *db, const char *target);

#endif /* _ALAM_CACHE_H */

/* vim: set ts=2 sw=2 noet: */
