/*
 *  db.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by David Kimpe <dnaku@frugalware.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <regex.h>
#include <time.h>

/* libalam */
#include "db.h"
#include "alam_list.h"
#include "log.h"
#include "util.h"
#include "handle.h"
#include "cache.h"
#include "alam.h"

/** \addtogroup alam_databases Database Functions
 * @brief Functions to query and manipulate the database of libalam
 * @{
 */

/** Register a sync database of packages.
 * @param treename the name of the sync repository
 * @return a amdb_t* on success (the value), NULL on error
 */
amdb_t SYMEXPORT *alam_db_register_sync(const char *treename)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, NULL));
	ASSERT(treename != NULL && strlen(treename) != 0, RET_ERR(AM_ERR_WRONG_ARGS, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(AM_ERR_TRANS_NOT_NULL, NULL));

	return(_alam_db_register_sync(treename));
}

/** Register the local package database.
 * @return a amdb_t* representing the local database, or NULL on error
 */
amdb_t SYMEXPORT *alam_db_register_local(void)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, NULL));
	/* Do not register a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(AM_ERR_TRANS_NOT_NULL, NULL));

	return(_alam_db_register_local());
}

/* Helper function for alam_db_unregister{_all} */
static void _alam_db_unregister(amdb_t *db)
{
	if(db == NULL) {
		return;
	}

	_alam_log(AM_LOG_DEBUG, "unregistering database '%s'\n", db->treename);
	_alam_db_free(db);
}

/** Unregister all package databases
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_db_unregister_all(void)
{
	alam_list_t *i;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	/* Do not unregister a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(AM_ERR_TRANS_NOT_NULL, -1));

	/* close local database */
	_alam_db_unregister(handle->db_local);
	handle->db_local = NULL;

	/* and also sync ones */
	for(i = handle->dbs_sync; i; i = i->next) {
		amdb_t *db = i->data;
		_alam_db_unregister(db);
		i->data = NULL;
	}
	FREELIST(handle->dbs_sync);
	return(0);
}

/** Unregister a package database
 * @param db pointer to the package database to unregister
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_db_unregister(amdb_t *db)
{
	int found = 0;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL, RET_ERR(AM_ERR_WRONG_ARGS, -1));
	/* Do not unregister a database if a transaction is on-going */
	ASSERT(handle->trans == NULL, RET_ERR(AM_ERR_TRANS_NOT_NULL, -1));

	if(db == handle->db_local) {
		handle->db_local = NULL;
		found = 1;
	} else {
		/* Warning : this function shouldn't be used to unregister all sync
		 * databases by walking through the list returned by
		 * alam_option_get_syncdbs, because the db is removed from that list here.
		 */
		void *data;
		handle->dbs_sync = alam_list_remove(handle->dbs_sync,
				db, _alam_db_cmp, &data);
		if(data) {
			found = 1;
		}
	}

	if(!found) {
		RET_ERR(AM_ERR_DB_NOT_FOUND, -1);
	}

	_alam_db_unregister(db);
	return(0);
}

/** Set the serverlist of a database.
 * @param db database pointer
 * @param url url of the server
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_db_setserver(amdb_t *db, const char *url)
{
	alam_list_t *i;
	int found = 0;
	char *newurl;
	int len = 0;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));

	for(i = handle->dbs_sync; i && !found; i = i->next) {
		amdb_t *sdb = i->data;
		if(strcmp(db->treename, sdb->treename) == 0) {
			found = 1;
		}
	}
	if(!found) {
		RET_ERR(AM_ERR_DB_NOT_FOUND, -1);
	}

	if(url) {
		len = strlen(url);
	}
	if(len) {
		newurl = strdup(url);
		/* strip the trailing slash if one exists */
		if(newurl[len - 1] == '/') {
			newurl[len - 1] = '\0';
		}
		db->servers = alam_list_add(db->servers, newurl);
		_alam_log(AM_LOG_DEBUG, "adding new server URL to database '%s': %s\n",
				db->treename, newurl);
	} else {
		FREELIST(db->servers);
		_alam_log(AM_LOG_DEBUG, "serverlist flushed for '%s'\n", db->treename);
	}

	return(0);
}

/** Get the name of a package database
 * @param db pointer to the package database
 * @return the name of the package database, NULL on error
 */
const char SYMEXPORT *alam_db_get_name(const amdb_t *db)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return db->treename;
}

/** Get a download URL for the package database
 * @param db pointer to the package database
 * @return a fully-specified download URL, NULL on error
 */
const char SYMEXPORT *alam_db_get_url(const amdb_t *db)
{
	char *url;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(db->servers != NULL, return(NULL));

	url = (char*)db->servers->data;

	return(url);
}


/** Get a package entry from a package database
 * @param db pointer to the package database to get the package from
 * @param name of the package
 * @return the package entry on success, NULL on error
 */
ampkg_t SYMEXPORT *alam_db_get_pkg(amdb_t *db, const char *name)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(_alam_db_get_pkgfromcache(db, name));
}

/** Get the package cache of a package database
 * @param db pointer to the package database to get the package from
 * @return the list of packages on success, NULL on error
 */
alam_list_t SYMEXPORT *alam_db_get_pkgcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alam_db_get_pkgcache(db));
}

/** Get a group entry from a package database
 * @param db pointer to the package database to get the group from
 * @param name of the group
 * @return the groups entry on success, NULL on error
 */
amgrp_t SYMEXPORT *alam_db_readgrp(amdb_t *db, const char *name)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));
	ASSERT(name != NULL && strlen(name) != 0, return(NULL));

	return(_alam_db_get_grpfromcache(db, name));
}

/** Get the group cache of a package database
 * @param db pointer to the package database to get the group from
 * @return the list of groups on success, NULL on error
 */
alam_list_t SYMEXPORT *alam_db_get_grpcache(amdb_t *db)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alam_db_get_grpcache(db));
}

/** Searches a database
 * @param db pointer to the package database to search in
 * @param needles the list of strings to search for
 * @return the list of packages on success, NULL on error
 */
alam_list_t SYMEXPORT *alam_db_search(amdb_t *db, const alam_list_t* needles)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(db != NULL, return(NULL));

	return(_alam_db_search(db, needles));
}

/** @} */

amdb_t *_alam_db_new(const char *dbpath, const char *treename)
{
	amdb_t *db;
	const size_t pathsize = strlen(dbpath) + strlen(treename) + 2;

	ALAM_LOG_FUNC;

	CALLOC(db, 1, sizeof(amdb_t), RET_ERR(AM_ERR_MEMORY, NULL));
	CALLOC(db->path, 1, pathsize, RET_ERR(AM_ERR_MEMORY, NULL));

	sprintf(db->path, "%s%s/", dbpath, treename);
	STRDUP(db->treename, treename, RET_ERR(AM_ERR_MEMORY, NULL));

	return(db);
}

void _alam_db_free(amdb_t *db)
{
	ALAM_LOG_FUNC;

	/* cleanup pkgcache */
	_alam_db_free_pkgcache(db);
	/* cleanup server list */
	FREELIST(db->servers);
	FREE(db->path);
	FREE(db->treename);
	FREE(db);

	return;
}

int _alam_db_cmp(const void *d1, const void *d2)
{
	amdb_t *db1 = (amdb_t *)d1;
	amdb_t *db2 = (amdb_t *)d2;
	return(strcmp(db1->treename, db2->treename));
}

alam_list_t *_alam_db_search(amdb_t *db, const alam_list_t *needles)
{
	const alam_list_t *i, *j, *k;
	alam_list_t *ret = NULL;
	/* copy the pkgcache- we will free the list var after each needle */
	alam_list_t *list = alam_list_copy(_alam_db_get_pkgcache(db));

	ALAM_LOG_FUNC;

	for(i = needles; i; i = i->next) {
		char *targ;
		regex_t reg;

		if(i->data == NULL) {
			continue;
		}
		ret = NULL;
		targ = i->data;
		_alam_log(AM_LOG_DEBUG, "searching for target '%s'\n", targ);

		if(regcomp(&reg, targ, REG_EXTENDED | REG_NOSUB | REG_ICASE | REG_NEWLINE) != 0) {
			RET_ERR(AM_ERR_INVALID_REGEX, NULL);
		}

		for(j = list; j; j = j->next) {
			ampkg_t *pkg = j->data;
			const char *matched = NULL;
			const char *name = alam_pkg_get_name(pkg);
			const char *desc = alam_pkg_get_desc(pkg);

			/* check name as regex AND as plain text */
			if(name && (regexec(&reg, name, 0, 0, 0) == 0 || strstr(name, targ))) {
				matched = name;
			}
			/* check desc */
			else if (desc && regexec(&reg, desc, 0, 0, 0) == 0) {
				matched = desc;
			}
			/* TODO: should we be doing this, and should we print something
			 * differently when we do match it since it isn't currently printed? */
			if(!matched) {
				/* check provides */
				for(k = alam_pkg_get_provides(pkg); k; k = k->next) {
					if (regexec(&reg, k->data, 0, 0, 0) == 0) {
						matched = k->data;
						break;
					}
				}
			}
			if(!matched) {
				/* check groups */
				for(k = alam_pkg_get_groups(pkg); k; k = k->next) {
					if (regexec(&reg, k->data, 0, 0, 0) == 0) {
						matched = k->data;
						break;
					}
				}
			}

			if(matched != NULL) {
				_alam_log(AM_LOG_DEBUG, "    search target '%s' matched '%s'\n",
				          targ, matched);
				ret = alam_list_add(ret, pkg);
			}
		}

		/* Free the existing search list, and use the returned list for the
		 * next needle. This allows for AND-based package searching. */
		alam_list_free(list);
		list = ret;
		regfree(&reg);
	}

	return(ret);
}

amdb_t *_alam_db_register_local(void)
{
	amdb_t *db;
	const char *dbpath;

	ALAM_LOG_FUNC;

	if(handle->db_local != NULL) {
		_alam_log(AM_LOG_WARNING, _("attempt to re-register the 'local' DB\n"));
		RET_ERR(AM_ERR_DB_NOT_NULL, NULL);
	}

	_alam_log(AM_LOG_DEBUG, "registering local database\n");

	dbpath = alam_option_get_dbpath();
	if(!dbpath) {
		_alam_log(AM_LOG_ERROR, _("database path is undefined\n"));
			RET_ERR(AM_ERR_DB_OPEN, NULL);
	}

	db = _alam_db_new(dbpath, "local");
	if(db == NULL) {
		RET_ERR(AM_ERR_DB_CREATE, NULL);
	}

	handle->db_local = db;
	return(db);
}

amdb_t *_alam_db_register_sync(const char *treename)
{
	amdb_t *db;
	const char *dbpath;
	char path[PATH_MAX];
	alam_list_t *i;

	ALAM_LOG_FUNC;

	for(i = handle->dbs_sync; i; i = i->next) {
		amdb_t *sdb = i->data;
		if(strcmp(treename, sdb->treename) == 0) {
			_alam_log(AM_LOG_DEBUG, "attempt to re-register the '%s' database, using existing\n", sdb->treename);
			return sdb;
		}
	}

	_alam_log(AM_LOG_DEBUG, "registering sync database '%s'\n", treename);

	dbpath = alam_option_get_dbpath();
	if(!dbpath) {
		_alam_log(AM_LOG_ERROR, _("database path is undefined\n"));
			RET_ERR(AM_ERR_DB_OPEN, NULL);
	}
	/* all sync DBs now reside in the sync/ subdir of the dbpath */
	snprintf(path, PATH_MAX, "%ssync/", dbpath);

	db = _alam_db_new(path, treename);
	if(db == NULL) {
		RET_ERR(AM_ERR_DB_CREATE, NULL);
	}

	handle->dbs_sync = alam_list_add(handle->dbs_sync, db);
	return(db);
}

/* vim: set ts=2 sw=2 noet: */
