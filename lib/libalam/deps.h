/*
 *  deps.h
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALAM_DEPS_H
#define _ALAM_DEPS_H

#include "db.h"
#include "sync.h"
#include "package.h"
#include "alam.h"

/* Dependency */
struct __amdepend_t {
	amdepmod_t mod;
	char *name;
	char *version;
};

/* Missing dependency */
struct __amdepmissing_t {
	char *target;
	amdepend_t *depend;
	char *causingpkg; /* this is used in case of remove dependency error only */
};

void _alam_dep_free(amdepend_t *dep);
amdepend_t *_alam_dep_dup(const amdepend_t *dep);
amdepmissing_t *_alam_depmiss_new(const char *target, amdepend_t *dep,
		const char *causinpkg);
void _alam_depmiss_free(amdepmissing_t *miss);
alam_list_t *_alam_sortbydeps(alam_list_t *targets, int reverse);
void _alam_recursedeps(amdb_t *db, alam_list_t *targs, int include_explicit);
ampkg_t *_alam_resolvedep(amdepend_t *dep, alam_list_t *dbs, alam_list_t *excluding, int prompt);
int _alam_resolvedeps(amdb_t *local, alam_list_t *dbs_sync, ampkg_t *pkg,
		alam_list_t *preferred, alam_list_t **packages, alam_list_t *remove,
		alam_list_t **data);
int _alam_dep_edge(ampkg_t *pkg1, ampkg_t *pkg2);
amdepend_t *_alam_splitdep(const char *depstring);
ampkg_t *_alam_find_dep_satisfier(alam_list_t *pkgs, amdepend_t *dep);

#endif /* _ALAM_DEPS_H */

/* vim: set ts=2 sw=2 noet: */
