/*
 *  handle.c
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

/* libalam */
#include "handle.h"
#include "alam_list.h"
#include "util.h"
#include "log.h"
#include "trans.h"
#include "alam.h"

/* global var for handle (private to libalam) */
amhandle_t *handle = NULL;

amhandle_t *_alam_handle_new()
{
	amhandle_t *handle;

	ALAM_LOG_FUNC;

	CALLOC(handle, 1, sizeof(amhandle_t), RET_ERR(AM_ERR_MEMORY, NULL));
	handle->lckfd = -1;

	return(handle);
}

void _alam_handle_free(amhandle_t *handle)
{
	ALAM_LOG_FUNC;

	if(handle == NULL) {
		return;
	}

	/* close logfile */
	if(handle->logstream) {
		fclose(handle->logstream);
		handle->logstream= NULL;
	}
	if(handle->usesyslog) {
		handle->usesyslog = 0;
		closelog();
	}

	/* free memory */
	_alam_trans_free(handle->trans);
	FREE(handle->root);
	FREE(handle->dbpath);
	FREELIST(handle->cachedirs);
	FREE(handle->logfile);
	FREE(handle->lockfile);
	FREE(handle->arch);
	FREELIST(handle->dbs_sync);
	FREELIST(handle->noupgrade);
	FREELIST(handle->noextract);
	FREELIST(handle->ignorepkg);
	FREELIST(handle->ignoregrp);
	FREE(handle);
}

alam_cb_log SYMEXPORT alam_option_get_logcb()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->logcb;
}

alam_cb_download SYMEXPORT alam_option_get_dlcb()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dlcb;
}

alam_cb_fetch SYMEXPORT alam_option_get_fetchcb()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->fetchcb;
}

alam_cb_totaldl SYMEXPORT alam_option_get_totaldlcb()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->totaldlcb;
}

const char SYMEXPORT *alam_option_get_root()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->root;
}

const char SYMEXPORT *alam_option_get_dbpath()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dbpath;
}

alam_list_t SYMEXPORT *alam_option_get_cachedirs()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->cachedirs;
}

const char SYMEXPORT *alam_option_get_logfile()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->logfile;
}

const char SYMEXPORT *alam_option_get_lockfile()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->lockfile;
}

unsigned short SYMEXPORT alam_option_get_usesyslog()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return -1;
	}
	return handle->usesyslog;
}

alam_list_t SYMEXPORT *alam_option_get_noupgrades()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->noupgrade;
}

alam_list_t SYMEXPORT *alam_option_get_noextracts()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->noextract;
}

alam_list_t SYMEXPORT *alam_option_get_ignorepkgs()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->ignorepkg;
}

alam_list_t SYMEXPORT *alam_option_get_ignoregrps()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->ignoregrp;
}

const char SYMEXPORT *alam_option_get_arch()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->arch;
}

amdb_t SYMEXPORT *alam_option_get_localdb()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->db_local;
}

alam_list_t SYMEXPORT *alam_option_get_syncdbs()
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return NULL;
	}
	return handle->dbs_sync;
}

void SYMEXPORT alam_option_set_logcb(alam_cb_log cb)
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return;
	}
	handle->logcb = cb;
}

void SYMEXPORT alam_option_set_dlcb(alam_cb_download cb)
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return;
	}
	handle->dlcb = cb;
}

void SYMEXPORT alam_option_set_fetchcb(alam_cb_fetch cb)
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return;
	}
	handle->fetchcb = cb;
}

void SYMEXPORT alam_option_set_totaldlcb(alam_cb_totaldl cb)
{
	if (handle == NULL) {
		am_errno = AM_ERR_HANDLE_NULL;
		return;
	}
	handle->totaldlcb = cb;
}

int SYMEXPORT alam_option_set_root(const char *root)
{
	struct stat st;
	char *realroot;
	size_t rootlen;

	ALAM_LOG_FUNC;

	if(!root) {
		am_errno = AM_ERR_WRONG_ARGS;
		return(-1);
	}
	if(stat(root, &st) == -1 || !S_ISDIR(st.st_mode)) {
		am_errno = AM_ERR_NOT_A_DIR;
		return(-1);
	}

	realroot = calloc(PATH_MAX+1, sizeof(char));
	if(!realpath(root, realroot)) {
		FREE(realroot);
		am_errno = AM_ERR_NOT_A_DIR;
		return(-1);
	}

	/* verify root ends in a '/' */
	rootlen = strlen(realroot);
	if(realroot[rootlen-1] != '/') {
		rootlen += 1;
	}
	if(handle->root) {
		FREE(handle->root);
	}
	handle->root = calloc(rootlen + 1, sizeof(char));
	strncpy(handle->root, realroot, rootlen);
	handle->root[rootlen-1] = '/';
	FREE(realroot);
	_alam_log(AM_LOG_DEBUG, "option 'root' = %s\n", handle->root);
	return(0);
}

int SYMEXPORT alam_option_set_dbpath(const char *dbpath)
{
	struct stat st;
	size_t dbpathlen, lockfilelen;
	const char *lf = "db.lck";

	ALAM_LOG_FUNC;

	if(!dbpath) {
		am_errno = AM_ERR_WRONG_ARGS;
		return(-1);
	}
	if(stat(dbpath, &st) == -1 || !S_ISDIR(st.st_mode)) {
		am_errno = AM_ERR_NOT_A_DIR;
		return(-1);
	}
	/* verify dbpath ends in a '/' */
	dbpathlen = strlen(dbpath);
	if(dbpath[dbpathlen-1] != '/') {
		dbpathlen += 1;
	}
	if(handle->dbpath) {
		FREE(handle->dbpath);
	}
	handle->dbpath = calloc(dbpathlen+1, sizeof(char));
	strncpy(handle->dbpath, dbpath, dbpathlen);
	handle->dbpath[dbpathlen-1] = '/';
	_alam_log(AM_LOG_DEBUG, "option 'dbpath' = %s\n", handle->dbpath);

	if(handle->lockfile) {
		FREE(handle->lockfile);
	}
	lockfilelen = strlen(handle->dbpath) + strlen(lf) + 1;
	handle->lockfile = calloc(lockfilelen, sizeof(char));
	snprintf(handle->lockfile, lockfilelen, "%s%s", handle->dbpath, lf);
	_alam_log(AM_LOG_DEBUG, "option 'lockfile' = %s\n", handle->lockfile);
	return(0);
}

int SYMEXPORT alam_option_add_cachedir(const char *cachedir)
{
	char *newcachedir;
	size_t cachedirlen;

	ALAM_LOG_FUNC;

	if(!cachedir) {
		am_errno = AM_ERR_WRONG_ARGS;
		return(-1);
	}
	/* don't stat the cachedir yet, as it may not even be needed. we can
	 * fail later if it is needed and the path is invalid. */

	/* verify cachedir ends in a '/' */
	cachedirlen = strlen(cachedir);
	if(cachedir[cachedirlen-1] != '/') {
		cachedirlen += 1;
	}
	newcachedir = calloc(cachedirlen + 1, sizeof(char));
	strncpy(newcachedir, cachedir, cachedirlen);
	newcachedir[cachedirlen-1] = '/';
	handle->cachedirs = alam_list_add(handle->cachedirs, newcachedir);
	_alam_log(AM_LOG_DEBUG, "option 'cachedir' = %s\n", newcachedir);
	return(0);
}

void SYMEXPORT alam_option_set_cachedirs(alam_list_t *cachedirs)
{
	if(handle->cachedirs) FREELIST(handle->cachedirs);
	if(cachedirs) handle->cachedirs = cachedirs;
}

int SYMEXPORT alam_option_remove_cachedir(const char *cachedir)
{
	char *vdata = NULL;
	char *newcachedir;
	size_t cachedirlen;
	/* verify cachedir ends in a '/' */
	cachedirlen = strlen(cachedir);
	if(cachedir[cachedirlen-1] != '/') {
		cachedirlen += 1;
	}
	newcachedir = calloc(cachedirlen + 1, sizeof(char));
	strncpy(newcachedir, cachedir, cachedirlen);
	newcachedir[cachedirlen-1] = '/';
	handle->cachedirs = alam_list_remove_str(handle->cachedirs, newcachedir, &vdata);
	FREE(newcachedir);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

int SYMEXPORT alam_option_set_logfile(const char *logfile)
{
	char *oldlogfile = handle->logfile;

	ALAM_LOG_FUNC;

	if(!logfile) {
		am_errno = AM_ERR_WRONG_ARGS;
		return(-1);
	}

	handle->logfile = strdup(logfile);

	/* free the old logfile path string, and close the stream so logaction
	 * will reopen a new stream on the new logfile */
	if(oldlogfile) {
		FREE(oldlogfile);
	}
	if(handle->logstream) {
		fclose(handle->logstream);
		handle->logstream = NULL;
	}
	_alam_log(AM_LOG_DEBUG, "option 'logfile' = %s\n", handle->logfile);
	return(0);
}

void SYMEXPORT alam_option_set_usesyslog(unsigned short usesyslog)
{
	handle->usesyslog = usesyslog;
}

void SYMEXPORT alam_option_add_noupgrade(const char *pkg)
{
	handle->noupgrade = alam_list_add(handle->noupgrade, strdup(pkg));
}

void SYMEXPORT alam_option_set_noupgrades(alam_list_t *noupgrade)
{
	if(handle->noupgrade) FREELIST(handle->noupgrade);
	if(noupgrade) handle->noupgrade = noupgrade;
}

int SYMEXPORT alam_option_remove_noupgrade(const char *pkg)
{
	char *vdata = NULL;
	handle->noupgrade = alam_list_remove_str(handle->noupgrade, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alam_option_add_noextract(const char *pkg)
{
	handle->noextract = alam_list_add(handle->noextract, strdup(pkg));
}

void SYMEXPORT alam_option_set_noextracts(alam_list_t *noextract)
{
	if(handle->noextract) FREELIST(handle->noextract);
	if(noextract) handle->noextract = noextract;
}

int SYMEXPORT alam_option_remove_noextract(const char *pkg)
{
	char *vdata = NULL;
	handle->noextract = alam_list_remove_str(handle->noextract, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alam_option_add_ignorepkg(const char *pkg)
{
	handle->ignorepkg = alam_list_add(handle->ignorepkg, strdup(pkg));
}

void SYMEXPORT alam_option_set_ignorepkgs(alam_list_t *ignorepkgs)
{
	if(handle->ignorepkg) FREELIST(handle->ignorepkg);
	if(ignorepkgs) handle->ignorepkg = ignorepkgs;
}

int SYMEXPORT alam_option_remove_ignorepkg(const char *pkg)
{
	char *vdata = NULL;
	handle->ignorepkg = alam_list_remove_str(handle->ignorepkg, pkg, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alam_option_add_ignoregrp(const char *grp)
{
	handle->ignoregrp = alam_list_add(handle->ignoregrp, strdup(grp));
}

void SYMEXPORT alam_option_set_ignoregrps(alam_list_t *ignoregrps)
{
	if(handle->ignoregrp) FREELIST(handle->ignoregrp);
	if(ignoregrps) handle->ignoregrp = ignoregrps;
}

int SYMEXPORT alam_option_remove_ignoregrp(const char *grp)
{
	char *vdata = NULL;
	handle->ignoregrp = alam_list_remove_str(handle->ignoregrp, grp, &vdata);
	if(vdata != NULL) {
		FREE(vdata);
		return(1);
	}
	return(0);
}

void SYMEXPORT alam_option_set_arch(const char *arch)
{
	if(handle->arch) FREE(handle->arch);
	if(arch) handle->arch = strdup(arch);
}

void SYMEXPORT alam_option_set_usedelta(unsigned short usedelta)
{
	handle->usedelta = usedelta;
}

/* vim: set ts=2 sw=2 noet: */
