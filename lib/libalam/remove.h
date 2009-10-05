/*
 *  remove.h
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
#ifndef _ALAM_REMOVE_H
#define _ALAM_REMOVE_H

#include "db.h"
#include "alam_list.h"
#include "trans.h"

int _alam_remove_loadtarget(amtrans_t *trans, amdb_t *db, char *name);
int _alam_remove_prepare(amtrans_t *trans, amdb_t *db, alam_list_t **data);
int _alam_remove_packages(amtrans_t *trans, amdb_t *db);

int _alam_upgraderemove_package(ampkg_t *oldpkg, ampkg_t *newpkg, amtrans_t *trans);


#endif /* _ALAM_REMOVE_H */

/* vim: set ts=2 sw=2 noet: */
