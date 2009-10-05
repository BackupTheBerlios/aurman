/*
 *  dload.h
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
#ifndef _ALAM_DLOAD_H
#define _ALAM_DLOAD_H

#include "alam_list.h"
#include "alam.h"

#include <time.h>

#define AM_DLBUF_LEN (1024 * 10)

int _alam_download_single_file(const char *filename,
		alam_list_t *servers, const char *localpath,
		time_t mtimeold, time_t *mtimenew);

int _alam_download_files(alam_list_t *files,
		alam_list_t *servers, const char *localpath);

#endif /* _ALAM_DLOAD_H */

/* vim: set ts=2 sw=2 noet: */
