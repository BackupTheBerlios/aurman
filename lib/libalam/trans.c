/*
 *  trans.c
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>

/* libalam */
#include "trans.h"
#include "alam_list.h"
#include "package.h"
#include "util.h"
#include "log.h"
#include "handle.h"
#include "add.h"
#include "remove.h"
#include "sync.h"
#include "alam.h"
#include "deps.h"
#include "cache.h"

/** \addtogroup alam_trans Transaction Functions
 * @brief Functions to manipulate libalam transactions
 * @{
 */

/** Initialize the transaction.
 * @param flags flags of the transaction (like nodeps, etc)
 * @param event event callback function pointer
 * @param conv question callback function pointer
 * @param progress progress callback function pointer
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_init(amtransflag_t flags,
                              alam_trans_cb_event event, alam_trans_cb_conv conv,
                              alam_trans_cb_progress progress)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	ASSERT(handle->trans == NULL, RET_ERR(AM_ERR_TRANS_NOT_NULL, -1));

	/* lock db */
	if(!(flags & AM_TRANS_FLAG_NOLOCK)) {
		handle->lckfd = _alam_lckmk();
		if(handle->lckfd == -1) {
			RET_ERR(AM_ERR_HANDLE_LOCK, -1);
		}
	}

	trans = _alam_trans_new();
	if(trans == NULL) {
		RET_ERR(AM_ERR_MEMORY, -1);
	}

	trans->flags = flags;
	trans->cb_event = event;
	trans->cb_conv = conv;
	trans->cb_progress = progress;
	trans->state = STATE_INITIALIZED;

	handle->trans = trans;

	return(0);
}

/** Search for packages to upgrade and add them to the transaction.
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_sysupgrade(int enable_downgrade)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	return(_alam_sync_sysupgrade(trans, handle->db_local, handle->dbs_sync, enable_downgrade));
}

/** Add a file target to the transaction.
 * @param target the name of the file target to add
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_add(char *target)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	if(_alam_add_loadtarget(trans, handle->db_local, target) == -1) {
		/* am_errno is set by _alam_add_loadtarget() */
		return(-1);
	}

	return(0);
}

/** Add a sync target to the transaction.
 * @param target the name of the sync target to add
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_sync(char *target)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	if(_alam_sync_addtarget(trans, handle->db_local, handle->dbs_sync, target) == -1) {
		/* am_errno is set by _alam_sync_loadtarget() */
		return(-1);
	}

	return(0);
}

int SYMEXPORT alam_trans_remove(char *target)
{
	ALAM_LOG_FUNC;

	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(target != NULL && strlen(target) != 0, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	if(_alam_remove_loadtarget(trans, handle->db_local, target) == -1) {
		/* am_errno is set by _alam_remove_loadtarget() */
		return(-1);
	}

	return(0);
}

static alam_list_t *check_arch(alam_list_t *pkgs)
{
	alam_list_t *i;
	alam_list_t *invalid = NULL;

	const char *arch = alam_option_get_arch();
	if(!arch) {
		return(NULL);
	}
	for(i = pkgs; i; i = i->next) {
		ampkg_t *pkg = i->data;
		const char *pkgarch = alam_pkg_get_arch(pkg);
		if(strcmp(pkgarch,arch) && strcmp(pkgarch,"any")) {
			char *string;
			const char *pkgname = alam_pkg_get_name(pkg);
			const char *pkgver = alam_pkg_get_version(pkg);
			size_t len = strlen(pkgname) + strlen(pkgver) + strlen(pkgarch) + 3;
			MALLOC(string, len, RET_ERR(AM_ERR_MEMORY, invalid));
			sprintf(string, "%s-%s-%s", pkgname, pkgver, pkgarch);
			invalid = alam_list_add(invalid, string);
		}
	}
	return(invalid);
}

/** Prepare a transaction.
 * @param data the address of an alam_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_prepare(alam_list_t **data)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(data != NULL, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return(0);
	}

	alam_list_t *invalid = check_arch(trans->add);
	if(invalid) {
		if(data) {
			*data = invalid;
		}
		RET_ERR(AM_ERR_PKG_INVALID_ARCH, -1);
	}

	if(trans->add == NULL) {
		if(_alam_remove_prepare(trans, handle->db_local, data) == -1) {
			/* am_errno is set by _alam_remove_prepare() */
			return(-1);
		}
	}	else {
		if(_alam_sync_prepare(trans, handle->db_local, handle->dbs_sync, data) == -1) {
			/* am_errno is set by _alam_sync_prepare() */
			return(-1);
		}
	}

	trans->state = STATE_PREPARED;

	return(0);
}

/** Commit a transaction.
 * @param data the address of an alam_list where detailed description
 * of an error can be dumped (ie. list of conflicting files)
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_commit(alam_list_t **data)
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;

	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_PREPARED, RET_ERR(AM_ERR_TRANS_NOT_PREPARED, -1));

	ASSERT(!(trans->flags & AM_TRANS_FLAG_NOLOCK), RET_ERR(AM_ERR_TRANS_NOT_LOCKED, -1));

	/* If there's nothing to do, return without complaining */
	if(trans->add == NULL && trans->remove == NULL) {
		return(0);
	}

	trans->state = STATE_COMMITING;

	if(trans->add == NULL) {
		if(_alam_remove_packages(trans, handle->db_local) == -1) {
			/* am_errno is set by _alam_remove_commit() */
			return(-1);
		}
	} else {
		if(_alam_sync_commit(trans, handle->db_local, data) == -1) {
			/* am_errno is set by _alam_sync_commit() */
			return(-1);
		}
	}

	trans->state = STATE_COMMITED;

	return(0);
}

/** Interrupt a transaction.
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_interrupt()
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state == STATE_COMMITING || trans->state == STATE_INTERRUPTED,
			RET_ERR(AM_ERR_TRANS_TYPE, -1));

	trans->state = STATE_INTERRUPTED;

	return(0);
}

/** Release a transaction.
 * @return 0 on success, -1 on error (am_errno is set accordingly)
 */
int SYMEXPORT alam_trans_release()
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));

	trans = handle->trans;
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(trans->state != STATE_IDLE, RET_ERR(AM_ERR_TRANS_NULL, -1));

	unsigned int nolock_flag = trans->flags & AM_TRANS_FLAG_NOLOCK;

	_alam_trans_free(trans);
	handle->trans = NULL;

	/* unlock db */
	if(!nolock_flag) {
		if(handle->lckfd != -1) {
			while(close(handle->lckfd) == -1 && errno == EINTR);
			handle->lckfd = -1;
		}
		if(_alam_lckrm()) {
			_alam_log(AM_LOG_WARNING, _("could not remove lock file %s\n"),
					alam_option_get_lockfile());
			alam_logaction("warning: could not remove lock file %s\n",
					alam_option_get_lockfile());
		}
	}

	return(0);
}

/** @} */

amtrans_t *_alam_trans_new()
{
	amtrans_t *trans;

	ALAM_LOG_FUNC;

	CALLOC(trans, 1, sizeof(amtrans_t), RET_ERR(AM_ERR_MEMORY, NULL));
	trans->state = STATE_IDLE;

	return(trans);
}

void _alam_trans_free(amtrans_t *trans)
{
	ALAM_LOG_FUNC;

	if(trans == NULL) {
		return;
	}

	alam_list_free_inner(trans->add, (alam_list_fn_free)_alam_pkg_free_trans);
	alam_list_free(trans->add);
	alam_list_free_inner(trans->remove, (alam_list_fn_free)_alam_pkg_free);
	alam_list_free(trans->remove);

	FREELIST(trans->skip_add);
	FREELIST(trans->skip_remove);

	FREE(trans);
}

/* A cheap grep for text files, returns 1 if a substring
 * was found in the text file fn, 0 if it wasn't
 */
static int grep(const char *fn, const char *needle)
{
	FILE *fp;

	if((fp = fopen(fn, "r")) == NULL) {
		return(0);
	}
	while(!feof(fp)) {
		char line[1024];
		fgets(line, 1024, fp);
		if(feof(fp)) {
			continue;
		}
		if(strstr(line, needle)) {
			fclose(fp);
			return(1);
		}
	}
	fclose(fp);
	return(0);
}

int _alam_runscriptlet(const char *root, const char *installfn,
											 const char *script, const char *ver,
											 const char *oldver, amtrans_t *trans)
{
	char scriptfn[PATH_MAX];
	char cmdline[PATH_MAX];
	char tmpdir[PATH_MAX];
	char *scriptpath;
	int clean_tmpdir = 0;
	int retval = 0;

	ALAM_LOG_FUNC;

	if(access(installfn, R_OK)) {
		/* not found */
		_alam_log(AM_LOG_DEBUG, "scriptlet '%s' not found\n", installfn);
		return(0);
	}

	/* creates a directory in $root/tmp/ for copying/extracting the scriptlet */
	snprintf(tmpdir, PATH_MAX, "%stmp/", root);
	if(access(tmpdir, F_OK) != 0) {
		alam_makepath_mode(tmpdir, 01777);
	}
	snprintf(tmpdir, PATH_MAX, "%stmp/alam_XXXXXX", root);
	if(mkdtemp(tmpdir) == NULL) {
		_alam_log(AM_LOG_ERROR, _("could not create temp directory\n"));
		return(1);
	} else {
		clean_tmpdir = 1;
	}

	/* either extract or copy the scriptlet */
	snprintf(scriptfn, PATH_MAX, "%s/.INSTALL", tmpdir);
	if(!strcmp(script, "pre_upgrade") || !strcmp(script, "pre_install")) {
		if(alam_unpack(installfn, tmpdir, ".INSTALL")) {
			retval = 1;
		}
	} else {
		if(_alam_copyfile(installfn, scriptfn)) {
			_alam_log(AM_LOG_ERROR, _("could not copy tempfile to %s (%s)\n"), scriptfn, strerror(errno));
			retval = 1;
		}
	}
	if(retval == 1) {
		goto cleanup;
	}

	/* chop off the root so we can find the tmpdir in the chroot */
	scriptpath = scriptfn + strlen(root) - 1;

	if(!grep(scriptfn, script)) {
		/* script not found in scriptlet file */
		goto cleanup;
	}

	if(oldver) {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s %s",
				scriptpath, script, ver, oldver);
	} else {
		snprintf(cmdline, PATH_MAX, ". %s; %s %s",
				scriptpath, script, ver);
	}

	retval = _alam_run_chroot(root, cmdline);

cleanup:
	if(clean_tmpdir && _alam_rmrf(tmpdir)) {
		_alam_log(AM_LOG_WARNING, _("could not remove tmpdir %s\n"), tmpdir);
	}

	return(retval);
}

unsigned int SYMEXPORT alam_trans_get_flags()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(-1));
	ASSERT(handle->trans != NULL, return(-1));

	return handle->trans->flags;
}

alam_list_t SYMEXPORT * alam_trans_get_add()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->add;
}

alam_list_t SYMEXPORT * alam_trans_get_remove()
{
	/* Sanity checks */
	ASSERT(handle != NULL, return(NULL));
	ASSERT(handle->trans != NULL, return(NULL));

	return handle->trans->remove;
}
/* vim: set ts=2 sw=2 noet: */
