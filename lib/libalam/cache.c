/*
 *  cache.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* libalam */
#include "cache.h"
#include "alam_list.h"
#include "log.h"
#include "alam.h"
#include "util.h"
#include "package.h"
#include "group.h"
#include "db.h"

/* Returns a new package cache from db.
 * It frees the cache if it already exists.
 */
int _alam_db_load_pkgcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}
	_alam_db_free_pkgcache(db);

	_alam_log(AM_LOG_DEBUG, "loading package cache for repository '%s'\n",
			db->treename);
	if(_alam_db_populate(db) == -1) {
		_alam_log(AM_LOG_DEBUG,
				"failed to load package cache for repository '%s'\n", db->treename);
		return(-1);
	}

	db->pkgcache_loaded = 1;
	return(0);
}

void _alam_db_free_pkgcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	if(db == NULL || !db->pkgcache_loaded) {
		return;
	}

	_alam_log(AM_LOG_DEBUG, "freeing package cache for repository '%s'\n",
	                        db->treename);

	alam_list_free_inner(db->pkgcache, (alam_list_fn_free)_alam_pkg_free);
	alam_list_free(db->pkgcache);
	db->pkgcache = NULL;
	db->pkgcache_loaded = 0;

	_alam_db_free_grpcache(db);
}

alam_list_t *_alam_db_get_pkgcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(!db->pkgcache_loaded) {
		_alam_db_load_pkgcache(db);
	}

	/* hmmm, still NULL ?*/
	if(!db->pkgcache) {
		_alam_log(AM_LOG_DEBUG, "warning: pkgcache is NULL for db '%s'\n", db->treename);
	}

	return(db->pkgcache);
}

/* "duplicate" pkg with BASE info (to spare some memory) then add it to pkgcache */
int _alam_db_add_pkgincache(amdb_t *db, ampkg_t *pkg)
{
	ampkg_t *newpkg;

	ALAM_LOG_FUNC;

	if(db == NULL || !db->pkgcache_loaded || pkg == NULL) {
		return(-1);
	}

	newpkg = _alam_pkg_new();
	if(newpkg == NULL) {
		return(-1);
	}
	newpkg->name = strdup(pkg->name);
	newpkg->version = strdup(pkg->version);
	if(newpkg->name == NULL || newpkg->version == NULL) {
		am_errno = AM_ERR_MEMORY;
		_alam_pkg_free(newpkg);
		return(-1);
	}
	newpkg->origin = PKG_FROM_CACHE;
	newpkg->origin_data.db = db;
	newpkg->infolevel = INFRQ_BASE;

	_alam_log(AM_LOG_DEBUG, "adding entry '%s' in '%s' cache\n",
						alam_pkg_get_name(newpkg), db->treename);
	db->pkgcache = alam_list_add_sorted(db->pkgcache, newpkg, _alam_pkg_cmp);

	_alam_db_free_grpcache(db);

	return(0);
}

int _alam_db_remove_pkgfromcache(amdb_t *db, ampkg_t *pkg)
{
	void *vdata;
	ampkg_t *data;

	ALAM_LOG_FUNC;

	if(db == NULL || !db->pkgcache_loaded || pkg == NULL) {
		return(-1);
	}

	_alam_log(AM_LOG_DEBUG, "removing entry '%s' from '%s' cache\n",
						alam_pkg_get_name(pkg), db->treename);

	db->pkgcache = alam_list_remove(db->pkgcache, pkg, _alam_pkg_cmp, &vdata);
	data = vdata;
	if(data == NULL) {
		/* package not found */
		_alam_log(AM_LOG_DEBUG, "cannot remove entry '%s' from '%s' cache: not found\n",
							alam_pkg_get_name(pkg), db->treename);
		return(-1);
	}

	_alam_pkg_free(data);

	_alam_db_free_grpcache(db);

	return(0);
}

ampkg_t *_alam_db_get_pkgfromcache(amdb_t *db, const char *target)
{
	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	alam_list_t *pkgcache = _alam_db_get_pkgcache(db);
	if(!pkgcache) {
		_alam_log(AM_LOG_DEBUG, "warning: failed to get '%s' from NULL pkgcache\n",
				target);
		return(NULL);
	}

	return(_alam_pkg_find(pkgcache, target));
}

/* Returns a new group cache from db.
 */
int _alam_db_load_grpcache(amdb_t *db)
{
	alam_list_t *lp;

	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(-1);
	}

	_alam_log(AM_LOG_DEBUG, "loading group cache for repository '%s'\n",
			db->treename);

	for(lp = _alam_db_get_pkgcache(db); lp; lp = lp->next) {
		const alam_list_t *i;
		ampkg_t *pkg = lp->data;

		for(i = alam_pkg_get_groups(pkg); i; i = i->next) {
			const char *grpname = i->data;
			alam_list_t *j;
			amgrp_t *grp = NULL;
			int found = 0;

			/* first look through the group cache for a group with this name */
			for(j = db->grpcache; j; j = j->next) {
				grp = j->data;

				if(strcmp(grp->name, grpname) == 0
						&& !alam_list_find_ptr(grp->packages, pkg)) {
					grp->packages = alam_list_add(grp->packages, pkg);
					found = 1;
					break;
				}
			}
			if(found) {
				continue;
			}
			/* we didn't find the group, so create a new one with this name */
			grp = _alam_grp_new(grpname);
			grp->packages = alam_list_add(grp->packages, pkg);
			db->grpcache = alam_list_add(db->grpcache, grp);
		}
	}

	db->grpcache_loaded = 1;
	return(0);
}

void _alam_db_free_grpcache(amdb_t *db)
{
	alam_list_t *lg;

	ALAM_LOG_FUNC;

	if(db == NULL || !db->grpcache_loaded) {
		return;
	}

	_alam_log(AM_LOG_DEBUG, "freeing group cache for repository '%s'\n",
	                        db->treename);

	for(lg = db->grpcache; lg; lg = lg->next) {
		_alam_grp_free(lg->data);
		lg->data = NULL;
	}
	FREELIST(db->grpcache);
	db->grpcache_loaded = 0;
}

alam_list_t *_alam_db_get_grpcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(NULL);
	}

	if(!db->grpcache_loaded) {
		_alam_db_load_grpcache(db);
	}

	return(db->grpcache);
}

amgrp_t *_alam_db_get_grpfromcache(amdb_t *db, const char *target)
{
	alam_list_t *i;

	ALAM_LOG_FUNC;

	if(db == NULL || target == NULL || strlen(target) == 0) {
		return(NULL);
	}

	for(i = _alam_db_get_grpcache(db); i; i = i->next) {
		amgrp_t *info = i->data;

		if(strcmp(info->name, target) == 0) {
			return(info);
		}
	}

	return(NULL);
}

/* vim: set ts=2 sw=2 noet: */
