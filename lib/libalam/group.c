/*
 *  group.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* libalam */
#include "group.h"
#include "alam_list.h"
#include "util.h"
#include "log.h"
#include "alam.h"

amgrp_t *_alam_grp_new(const char *name)
{
	amgrp_t* grp;

	ALAM_LOG_FUNC;

	CALLOC(grp, 1, sizeof(amgrp_t), RET_ERR(AM_ERR_MEMORY, NULL));
	STRDUP(grp->name, name, RET_ERR(AM_ERR_MEMORY, NULL));

	return(grp);
}

void _alam_grp_free(amgrp_t *grp)
{
	ALAM_LOG_FUNC;

	if(grp == NULL) {
		return;
	}

	FREE(grp->name);
	/* do NOT free the contents of the list, just the nodes */
	alam_list_free(grp->packages);
	FREE(grp);
}

const char SYMEXPORT *alam_grp_get_name(const amgrp_t *grp)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->name;
}

alam_list_t SYMEXPORT *alam_grp_get_pkgs(const amgrp_t *grp)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(grp != NULL, return(NULL));

	return grp->packages;
}
/* vim: set ts=2 sw=2 noet: */
