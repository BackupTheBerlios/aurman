/*
 *  alam.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
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

#include "config.h"

/* libalam */
#include "alam.h"
#include "alam_list.h"
#include "handle.h"
#include "util.h"

/* Globals */
enum _amerrno_t am_errno SYMEXPORT;

/** \addtogroup alam_interface Interface Functions
 * @brief Functions to initialize and release libalam
 * @{
 */

/** Initializes the library.  This must be called before any other
 * functions are called.
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_initialize(void)
{
	ASSERT(handle == NULL, RET_ERR(AM_ERR_HANDLE_NOT_NULL, -1));

	handle = _alam_handle_new();
	if(handle == NULL) {
		RET_ERR(AM_ERR_MEMORY, -1);
	}

#ifdef ENABLE_NLS
	bindtextdomain("libalam", LOCALEDIR);
#endif

	return(0);
}

/** Release the library.  This should be the last alam call you make.
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_release(void)
{
	ALAM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	if(alam_db_unregister_all() == -1) {
		return(-1);
	}

	_alam_handle_free(handle);

	return(0);
}

/** @} */

/** @defgroup alam_misc Miscellaneous Functions
 * @brief Various libalam functions
 */

/* Get the version of library */
const char SYMEXPORT *alam_version(void) {
	return(LIB_VERSION);
}

/* vim: set ts=2 sw=2 noet: */
