/*
 *  conflict.h
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
#ifndef _ALAM_CONFLICT_H
#define _ALAM_CONFLICT_H

#include "alam.h"
#include "db.h"
#include "package.h"

struct __amconflict_t {
	char *package1;
	char *package2;
	char *reason;
};

struct __amfileconflict_t {
	char *target;
	amfileconflicttype_t type;
	char *file;
	char *ctarget;
};

amconflict_t *_alam_conflict_new(const char *package1, const char *package2, const char *reason);
amconflict_t *_alam_conflict_dup(const amconflict_t *conflict);
void _alam_conflict_free(amconflict_t *conflict);
int _alam_conflict_isin(amconflict_t *needle, alam_list_t *haystack);
alam_list_t *_alam_innerconflicts(alam_list_t *packages);
alam_list_t *_alam_outerconflicts(amdb_t *db, alam_list_t *packages);
alam_list_t *_alam_db_find_fileconflicts(amdb_t *db, amtrans_t *trans,
					 alam_list_t *upgrade, alam_list_t *remove);

void _alam_fileconflict_free(amfileconflict_t *conflict);

#endif /* _ALAM_CONFLICT_H */

/* vim: set ts=2 sw=2 noet: */
