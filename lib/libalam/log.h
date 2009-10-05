/*
 *  log.h
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
#ifndef _ALAM_LOG_H
#define _ALAM_LOG_H

#include "alam.h"

#ifdef PACMAN_DEBUG
/* Log funtion entry points if debugging is enabled */
#define ALAM_LOG_FUNC _alam_log(AM_LOG_FUNCTION, "Enter %s\n", __func__)
#else
#define ALAM_LOG_FUNC
#endif

void _alam_log(amloglevel_t flag, char *fmt, ...) __attribute__((format(printf,2,3)));

#endif /* _ALAM_LOG_H */

/* vim: set ts=2 sw=2 noet: */
