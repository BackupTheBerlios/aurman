/*
 *  sync.h
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#ifndef _ALAM_SYNC_H
#define _ALAM_SYNC_H

#include "alam.h"

int _alam_sync_sysupgrade(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, int enable_downgrade);

int _alam_sync_addtarget(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, char *name);
int _alam_sync_prepare(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, alam_list_t **data);
int _alam_sync_commit(amtrans_t *trans, amdb_t *db_local, alam_list_t **data);

#endif /* _ALAM_SYNC_H */

/* vim: set ts=2 sw=2 noet: */
