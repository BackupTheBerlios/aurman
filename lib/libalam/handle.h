/*
 *  handle.h
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
#ifndef _ALAM_HANDLE_H
#define _ALAM_HANDLE_H

#include <stdio.h>
#include <sys/types.h>

#include "alam_list.h"
#include "db.h"
#include "log.h"
#include "alam.h"
#include "trans.h"

typedef struct _amhandle_t {
	/* internal usage */
	amdb_t *db_local;       /* local db pointer */
	alam_list_t *dbs_sync;  /* List of (pmdb_t *) */
	FILE *logstream;        /* log file stream pointer */
	int lckfd;              /* lock file descriptor if one exists */
	amtrans_t *trans;

	/* callback functions */
	alam_cb_log logcb;      /* Log callback function */
	alam_cb_download dlcb;  /* Download callback function */
	alam_cb_totaldl totaldlcb;  /* Total download callback function */
	alam_cb_fetch fetchcb; /* Download file callback function */

	/* filesystem paths */
	char *root;              /* Root path, default '/' */
	char *dbpath;            /* Base path to pacman's DBs */
	char *logfile;           /* Name of the log file */
	char *lockfile;          /* Name of the lock file */
	alam_list_t *cachedirs;  /* Paths to pacman cache directories */

	/* package lists */
	alam_list_t *noupgrade;   /* List of packages NOT to be upgraded */
	alam_list_t *noextract;   /* List of files NOT to extract */
	alam_list_t *ignorepkg;   /* List of packages to ignore */
	alam_list_t *ignoregrp;   /* List of groups to ignore */

	/* options */
	unsigned short usesyslog;    /* Use syslog instead of logfile? */ /* TODO move to frontend */
	char *arch;       /* Architecture of packages we should allow */
	unsigned short usedelta;     /* Download deltas if possible */
} amhandle_t;

/* global handle variable */
extern amhandle_t *handle;

amhandle_t *_alam_handle_new();
void _alam_handle_free(amhandle_t *handle);

#endif /* _ALAM_HANDLE_H */

/* vim: set ts=2 sw=2 noet: */
