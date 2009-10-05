/*
 *  group.h
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
#ifndef _ALAM_GROUP_H
#define _ALAM_GROUP_H

#include "alam.h"

struct __amgrp_t {
	/** group name */
	char *name;
	/** list of ampkg_t packages */
	alam_list_t *packages;
};

amgrp_t *_alam_grp_new(const char *name);
void _alam_grp_free(amgrp_t *grp);

#endif /* _ALAM_GROUP_H */

/* vim: set ts=2 sw=2 noet: */
