/*
 *  remove.c
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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

/* libalam */
#include "remove.h"
#include "alam_list.h"
#include "trans.h"
#include "util.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "handle.h"
#include "alam.h"

int _alam_remove_loadtarget(amtrans_t *trans, amdb_t *db, char *name)
{
	ampkg_t *info;
	const char *targ;

	ALAM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	targ = strchr(name, '/');
	if(targ && strncmp(name, "local", 5) == 0) {
		targ++;
	} else {
		targ = name;
	}

	if(_alam_pkg_find(trans->remove, targ)) {
		RET_ERR(AM_ERR_TRANS_DUP_TARGET, -1);
	}

	if((info = _alam_db_get_pkgfromcache(db, targ)) == NULL) {
		_alam_log(AM_LOG_DEBUG, "could not find %s in database\n", targ);
		RET_ERR(AM_ERR_PKG_NOT_FOUND, -1);
	}

	_alam_log(AM_LOG_DEBUG, "adding %s in the targets list\n", info->name);
	trans->remove = alam_list_add(trans->remove, _alam_pkg_dup(info));

	return(0);
}

static void remove_prepare_cascade(amtrans_t *trans, amdb_t *db,
		alam_list_t *lp)
{
	ALAM_LOG_FUNC;

	while(lp) {
		alam_list_t *i;
		for(i = lp; i; i = i->next) {
			amdepmissing_t *miss = (amdepmissing_t *)i->data;
			ampkg_t *info = _alam_db_get_pkgfromcache(db, miss->target);
			if(info) {
				if(!_alam_pkg_find(trans->remove, alam_pkg_get_name(info))) {
					_alam_log(AM_LOG_DEBUG, "pulling %s in the targets list\n",
							alam_pkg_get_name(info));
					trans->remove = alam_list_add(trans->remove, _alam_pkg_dup(info));
				}
			} else {
				_alam_log(AM_LOG_ERROR, _("could not find %s in database -- skipping\n"),
									miss->target);
			}
		}
		alam_list_free_inner(lp, (alam_list_fn_free)_alam_depmiss_free);
		alam_list_free(lp);
		lp = alam_checkdeps(_alam_db_get_pkgcache(db), 1, trans->remove, NULL);
	}
}

static void remove_prepare_keep_needed(amtrans_t *trans, amdb_t *db,
		alam_list_t *lp)
{
	ALAM_LOG_FUNC;

	/* Remove needed packages (which break dependencies) from the target list */
	while(lp != NULL) {
		alam_list_t *i;
		for(i = lp; i; i = i->next) {
			amdepmissing_t *miss = (amdepmissing_t *)i->data;
			void *vpkg;
			ampkg_t *pkg = _alam_pkg_find(trans->remove, miss->causingpkg);
			if(pkg == NULL) {
				continue;
			}
			trans->remove = alam_list_remove(trans->remove, pkg, _alam_pkg_cmp,
					&vpkg);
			pkg = vpkg;
			if(pkg) {
				_alam_log(AM_LOG_WARNING, "removing %s from the target-list\n",
						alam_pkg_get_name(pkg));
				_alam_pkg_free(pkg);
			}
		}
		alam_list_free_inner(lp, (alam_list_fn_free)_alam_depmiss_free);
		alam_list_free(lp);
		lp = alam_checkdeps(_alam_db_get_pkgcache(db), 1, trans->remove, NULL);
	}
}

int _alam_remove_prepare(amtrans_t *trans, amdb_t *db, alam_list_t **data)
{
	alam_list_t *lp;

	ALAM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));

	if((trans->flags & AM_TRANS_FLAG_RECURSE) && !(trans->flags & AM_TRANS_FLAG_CASCADE)) {
		_alam_log(AM_LOG_DEBUG, "finding removable dependencies\n");
		_alam_recursedeps(db, trans->remove, trans->flags & AM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & AM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, AM_TRANS_EVT_CHECKDEPS_START, NULL, NULL);

		_alam_log(AM_LOG_DEBUG, "looking for unsatisfied dependencies\n");
		lp = alam_checkdeps(_alam_db_get_pkgcache(db), 1, trans->remove, NULL);
		if(lp != NULL) {

			if(trans->flags & AM_TRANS_FLAG_CASCADE) {
				remove_prepare_cascade(trans, db, lp);
			} else if (trans->flags & AM_TRANS_FLAG_UNNEEDED) {
				/* Remove needed packages (which would break dependencies)
				 * from the target list */
				remove_prepare_keep_needed(trans, db, lp);
			} else {
				if(data) {
					*data = lp;
				} else {
					alam_list_free_inner(lp, (alam_list_fn_free)_alam_depmiss_free);
					alam_list_free(lp);
				}
				RET_ERR(AM_ERR_UNSATISFIED_DEPS, -1);
			}
		}
	}

	/* re-order w.r.t. dependencies */
	_alam_log(AM_LOG_DEBUG, "sorting by dependencies\n");
	lp = _alam_sortbydeps(trans->remove, 1);
	/* free the old alltargs */
	alam_list_free(trans->remove);
	trans->remove = lp;

	/* -Rcs == -Rc then -Rs */
	if((trans->flags & AM_TRANS_FLAG_CASCADE) && (trans->flags & AM_TRANS_FLAG_RECURSE)) {
		_alam_log(AM_LOG_DEBUG, "finding removable dependencies\n");
		_alam_recursedeps(db, trans->remove, trans->flags & AM_TRANS_FLAG_RECURSEALL);
	}

	if(!(trans->flags & AM_TRANS_FLAG_NODEPS)) {
		EVENT(trans, AM_TRANS_EVT_CHECKDEPS_DONE, NULL, NULL);
	}

	return(0);
}

static int can_remove_file(const char *path, alam_list_t *skip)
{
	char file[PATH_MAX+1];

	snprintf(file, PATH_MAX, "%s%s", handle->root, path);

	if(alam_list_find_str(skip, file)) {
		/* return success because we will never actually remove this file */
		return(1);
	}
	/* If we fail write permissions due to a read-only filesystem, abort.
	 * Assume all other possible failures are covered somewhere else */
	if(access(file, W_OK) == -1) {
		if(errno != EACCES && errno != ETXTBSY && access(file, F_OK) == 0) {
			/* only return failure if the file ACTUALLY exists and we can't write to
			 * it - ignore "chmod -w" simple permission failures */
			_alam_log(AM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
			          file, strerror(errno));
			return(0);
		}
	}

	return(1);
}

/* Helper function for iterating through a package's file and deleting them
 * Used by _alam_remove_commit. */
static void unlink_file(ampkg_t *info, char *filename, alam_list_t *skip_remove, int nosave)
{
	struct stat buf;
	char file[PATH_MAX+1];

	ALAM_LOG_FUNC;

	snprintf(file, PATH_MAX, "%s%s", handle->root, filename);

	/* check the remove skip list before removing the file.
	 * see the big comment block in db_find_fileconflicts() for an
	 * explanation. */
	if(alam_list_find_str(skip_remove, filename)) {
		_alam_log(AM_LOG_DEBUG, "%s is in skip_remove, skipping removal\n",
				file);
		return;
	}

	/* we want to do a lstat here, and not a _alam_lstat.
	 * if a directory in the package is actually a directory symlink on the
	 * filesystem, we want to work with the linked directory instead of the
	 * actual symlink */
	if(lstat(file, &buf)) {
		_alam_log(AM_LOG_DEBUG, "file %s does not exist\n", file);
		return;
	}

	if(S_ISDIR(buf.st_mode)) {
		if(rmdir(file)) {
			/* this is okay, other packages are probably using it (like /usr) */
			_alam_log(AM_LOG_DEBUG, "keeping directory %s\n", file);
		} else {
			_alam_log(AM_LOG_DEBUG, "removing directory %s\n", file);
		}
	} else {
		/* if the file needs backup and has been modified, back it up to .pacsave */
		char *pkghash = _alam_needbackup(filename, alam_pkg_get_backup(info));
		if(pkghash) {
			if(nosave) {
				_alam_log(AM_LOG_DEBUG, "transaction is set to NOSAVE, not backing up '%s'\n", file);
				FREE(pkghash);
			} else {
				char *filehash = alam_compute_md5sum(file);
				int cmp = strcmp(filehash,pkghash);
				FREE(filehash);
				FREE(pkghash);
				if(cmp != 0) {
					char newpath[PATH_MAX];
					snprintf(newpath, PATH_MAX, "%s.pacsave", file);
					rename(file, newpath);
					_alam_log(AM_LOG_WARNING, _("%s saved as %s\n"), file, newpath);
					alam_logaction("warning: %s saved as %s\n", file, newpath);
					return;
				}
			}
		}

		_alam_log(AM_LOG_DEBUG, "unlinking %s\n", file);

		if(unlink(file) == -1) {
			_alam_log(AM_LOG_ERROR, _("cannot remove file '%s': %s\n"),
								filename, strerror(errno));
		}
	}
}

int _alam_upgraderemove_package(ampkg_t *oldpkg, ampkg_t *newpkg, amtrans_t *trans)
{
	alam_list_t *skip_remove, *b;
	alam_list_t *newfiles, *lp;
	alam_list_t *files = alam_pkg_get_files(oldpkg);
	const char *pkgname = alam_pkg_get_name(oldpkg);

	ALAM_LOG_FUNC;

	_alam_log(AM_LOG_DEBUG, "removing old package first (%s-%s)\n",
			oldpkg->name, oldpkg->version);

	/* copy the remove skiplist over */
	skip_remove =
		alam_list_join(alam_list_strdup(trans->skip_remove),alam_list_strdup(handle->noupgrade));
	/* Add files in the NEW backup array to the skip_remove array
	 * so this removal operation doesn't kill them */
	/* old package backup list */
	alam_list_t *filelist = alam_pkg_get_files(newpkg);
	for(b = alam_pkg_get_backup(newpkg); b; b = b->next) {
		char *backup = _alam_backup_file(b->data);
		/* safety check (fix the upgrade026 pactest) */
		if(!alam_list_find_str(filelist, backup)) {
			FREE(backup);
			continue;
		}
		_alam_log(AM_LOG_DEBUG, "adding %s to the skip_remove array\n", backup);
		skip_remove = alam_list_add(skip_remove, backup);
	}

	for(lp = files; lp; lp = lp->next) {
		if(!can_remove_file(lp->data, skip_remove)) {
			_alam_log(AM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					pkgname);
			RET_ERR(AM_ERR_PKG_CANT_REMOVE, -1);
		}
	}

	/* iterate through the list backwards, unlinking files */
	newfiles = alam_list_reverse(files);
	for(lp = newfiles; lp; lp = alam_list_next(lp)) {
		unlink_file(oldpkg, lp->data, skip_remove, 0);
	}
	alam_list_free(newfiles);
	FREELIST(skip_remove);

	/* remove the package from the database */
	_alam_log(AM_LOG_DEBUG, "updating database\n");
	_alam_log(AM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
	if(_alam_db_remove(handle->db_local, oldpkg) == -1) {
		_alam_log(AM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
				pkgname, alam_pkg_get_version(oldpkg));
	}
	/* remove the package from the cache */
	if(_alam_db_remove_pkgfromcache(handle->db_local, oldpkg) == -1) {
		_alam_log(AM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
				pkgname);
	}

	return(0);
}

int _alam_remove_packages(amtrans_t *trans, amdb_t *db)
{
	ampkg_t *info;
	alam_list_t *targ, *lp;
	int pkg_count;

	ALAM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));

	pkg_count = alam_list_count(trans->remove);

	for(targ = trans->remove; targ; targ = targ->next) {
		int position = 0;
		char scriptlet[PATH_MAX];
		info = (ampkg_t*)targ->data;
		const char *pkgname = NULL;

		if(handle->trans->state == STATE_INTERRUPTED) {
			return(0);
		}

		/* get the name now so we can use it after package is removed */
		pkgname = alam_pkg_get_name(info);
		snprintf(scriptlet, PATH_MAX, "%s%s-%s/install", db->path,
						 pkgname, alam_pkg_get_version(info));

		EVENT(trans, AM_TRANS_EVT_REMOVE_START, info, NULL);
		_alam_log(AM_LOG_DEBUG, "removing package %s-%s\n",
				pkgname, alam_pkg_get_version(info));

		/* run the pre-remove scriptlet if it exists  */
		if(alam_pkg_has_scriptlet(info) && !(trans->flags & AM_TRANS_FLAG_NOSCRIPTLET)) {
			_alam_runscriptlet(handle->root, scriptlet, "pre_remove",
					alam_pkg_get_version(info), NULL, trans);
		}


		if(!(trans->flags & AM_TRANS_FLAG_DBONLY)) {
			alam_list_t *files = alam_pkg_get_files(info);
			for(lp = files; lp; lp = lp->next) {
				if(!can_remove_file(lp->data, NULL)) {
					_alam_log(AM_LOG_DEBUG, "not removing package '%s', can't remove all files\n",
					          pkgname);
					RET_ERR(AM_ERR_PKG_CANT_REMOVE, -1);
				}
			}

			int filenum = alam_list_count(files);
			double percent = 0.0;
			alam_list_t *newfiles;
			_alam_log(AM_LOG_DEBUG, "removing %d files\n", filenum);

			/* iterate through the list backwards, unlinking files */
			newfiles = alam_list_reverse(files);
			for(lp = newfiles; lp; lp = alam_list_next(lp)) {
				unlink_file(info, lp->data, NULL, trans->flags & AM_TRANS_FLAG_NOSAVE);

				/* update progress bar after each file */
				percent = (double)position / (double)filenum;
				PROGRESS(trans, AM_TRANS_PROGRESS_REMOVE_START, info->name,
						(double)(percent * 100), pkg_count,
						(pkg_count - alam_list_count(targ) + 1));
				position++;
			}
			alam_list_free(newfiles);
		}

		/* set progress to 100% after we finish unlinking files */
		PROGRESS(trans, AM_TRANS_PROGRESS_REMOVE_START, pkgname, 100,
		         pkg_count, (pkg_count - alam_list_count(targ) + 1));

		/* run the post-remove script if it exists  */
		if(alam_pkg_has_scriptlet(info) && !(trans->flags & AM_TRANS_FLAG_NOSCRIPTLET)) {
			_alam_runscriptlet(handle->root, scriptlet, "post_remove",
					alam_pkg_get_version(info), NULL, trans);
		}

		/* remove the package from the database */
		_alam_log(AM_LOG_DEBUG, "updating database\n");
		_alam_log(AM_LOG_DEBUG, "removing database entry '%s'\n", pkgname);
		if(_alam_db_remove(db, info) == -1) {
			_alam_log(AM_LOG_ERROR, _("could not remove database entry %s-%s\n"),
			          pkgname, alam_pkg_get_version(info));
		}
		/* remove the package from the cache */
		if(_alam_db_remove_pkgfromcache(db, info) == -1) {
			_alam_log(AM_LOG_ERROR, _("could not remove entry '%s' from cache\n"),
			          pkgname);
		}

		EVENT(trans, AM_TRANS_EVT_REMOVE_DONE, info, NULL);
	}

	/* run ldconfig if it exists */
	_alam_ldconfig(handle->root);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
