/*
 *  sync.c
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

#include <sys/types.h> /* off_t */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h> /* intmax_t */
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/* libalam */
#include "sync.h"
#include "alam_list.h"
#include "log.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "deps.h"
#include "conflict.h"
#include "trans.h"
#include "add.h"
#include "util.h"
#include "handle.h"
#include "alam.h"
#include "dload.h"
#include "delta.h"
#include "remove.h"

/** Check for new version of pkg in sync repos
 * (only the first occurrence is considered in sync)
 */
ampkg_t SYMEXPORT *alam_sync_newversion(ampkg_t *pkg, alam_list_t *dbs_sync)
{
	ASSERT(pkg != NULL, return(NULL));

	alam_list_t *i;
	ampkg_t *spkg = NULL;

	for(i = dbs_sync; !spkg && i; i = i->next) {
		spkg = _alam_db_get_pkgfromcache(i->data, alam_pkg_get_name(pkg));
	}

	if(spkg == NULL) {
		_alam_log(AM_LOG_DEBUG, "'%s' not found in sync db => no upgrade\n",
				alam_pkg_get_name(pkg));
		return(NULL);
	}

	/* compare versions and see if spkg is an upgrade */
	if(_alam_pkg_compare_versions(spkg, pkg) > 0) {
		_alam_log(AM_LOG_DEBUG, "new version of '%s' found (%s => %s)\n",
					alam_pkg_get_name(pkg), alam_pkg_get_version(pkg),
					alam_pkg_get_version(spkg));
		return(spkg);
	}
	/* spkg is not an upgrade */
	return(NULL);
}

int _alam_sync_sysupgrade(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, int enable_downgrade)
{
	alam_list_t *i, *j, *k;

	ALAM_LOG_FUNC;

	_alam_log(AM_LOG_DEBUG, "checking for package upgrades\n");
	for(i = _alam_db_get_pkgcache(db_local); i; i = i->next) {
		ampkg_t *lpkg = i->data;

		if(_alam_pkg_find(trans->add, lpkg->name)) {
			_alam_log(AM_LOG_DEBUG, "%s is already in the target list -- skipping\n", lpkg->name);
			continue;
		}

		/* Search for literal then replacers in each sync database.
		 * If found, don't check other databases */
		for(j = dbs_sync; j; j = j->next) {
			amdb_t *sdb = j->data;
			/* Check sdb */
			ampkg_t *spkg = _alam_db_get_pkgfromcache(sdb, lpkg->name);
			if(spkg) { /* 1. literal was found in sdb */
				int cmp = _alam_pkg_compare_versions(spkg, lpkg);
				if(cmp > 0) {
					_alam_log(AM_LOG_DEBUG, "new version of '%s' found (%s => %s)\n",
								lpkg->name, lpkg->version, spkg->version);
					/* check IgnorePkg/IgnoreGroup */
					if(_alam_pkg_should_ignore(spkg) || _alam_pkg_should_ignore(lpkg)) {
						_alam_log(AM_LOG_WARNING, _("%s: ignoring package upgrade (%s => %s)\n"),
										lpkg->name, lpkg->version, spkg->version);
					} else {
						_alam_log(AM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
												spkg->name, spkg->version);
						trans->add = alam_list_add(trans->add, spkg);
					}
				} else if(cmp < 0) {
					if(enable_downgrade) {
						/* check IgnorePkg/IgnoreGroup */
						if(_alam_pkg_should_ignore(spkg) || _alam_pkg_should_ignore(lpkg)) {
							_alam_log(AM_LOG_WARNING, _("%s: ignoring package downgrade (%s => %s)\n"),
											lpkg->name, lpkg->version, spkg->version);
						} else {
							_alam_log(AM_LOG_WARNING, _("%s: downgrading from version %s to version %s\n"),
											lpkg->name, lpkg->version, spkg->version);
							trans->add = alam_list_add(trans->add, spkg);
						}
					} else {
						_alam_log(AM_LOG_WARNING, _("%s: local (%s) is newer than %s (%s)\n"),
								lpkg->name, lpkg->version, sdb->treename, spkg->version);
					}
				}
				break; /* jump to next local package */
			} else { /* 2. search for replacers in sdb */
				int found = 0;
				for(k = _alam_db_get_pkgcache(sdb); k; k = k->next) {
					spkg = k->data;
					if(alam_list_find_str(alam_pkg_get_replaces(spkg), lpkg->name)) {
						found = 1;
						/* check IgnorePkg/IgnoreGroup */
						if(_alam_pkg_should_ignore(spkg) || _alam_pkg_should_ignore(lpkg)) {
							_alam_log(AM_LOG_WARNING, _("ignoring package replacement (%s-%s => %s-%s)\n"),
										lpkg->name, lpkg->version, spkg->name, spkg->version);
							continue;
						}

						int doreplace = 0;
						QUESTION(trans, AM_TRANS_CONV_REPLACE_PKG, lpkg, spkg, sdb->treename, &doreplace);
						if(!doreplace) {
							continue;
						}

						/* If spkg is already in the target list, we append lpkg to spkg's removes list */
						ampkg_t *tpkg = _alam_pkg_find(trans->add, spkg->name);
						if(tpkg) {
							/* sanity check, multiple repos can contain spkg->name */
							if(tpkg->origin_data.db != sdb) {
								_alam_log(AM_LOG_WARNING, _("cannot replace %s by %s\n"),
													lpkg->name, spkg->name);
								continue;
							}
							_alam_log(AM_LOG_DEBUG, "appending %s to the removes list of %s\n",
												lpkg->name, tpkg->name);
							tpkg->removes = alam_list_add(tpkg->removes, lpkg);
							/* check the to-be-replaced package's reason field */
							if(alam_pkg_get_reason(lpkg) == AM_PKG_REASON_EXPLICIT) {
								tpkg->reason = AM_PKG_REASON_EXPLICIT;
							}
						} else { /* add spkg to the target list */
							/* copy over reason */
							spkg->reason = alam_pkg_get_reason(lpkg);
							spkg->removes = alam_list_add(NULL, lpkg);
							_alam_log(AM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
													spkg->name, spkg->version);
							trans->add = alam_list_add(trans->add, spkg);
						}
					}
				}
				if(found) {
					break; /* jump to next local package */
				}
			}
		}
	}

	return(0);
}

int _alam_sync_addtarget(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, char *name)
{
	char *targline;
	char *targ;
	alam_list_t *j;
	ampkg_t *local, *spkg;
	amdepend_t *dep; /* provisions and dependencies are also allowed */

	ALAM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(name != NULL, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	STRDUP(targline, name, RET_ERR(AM_ERR_MEMORY, -1));
	targ = strchr(targline, '/');
	if(targ) {
		/* we are looking for a package in a specific database */
		alam_list_t *dbs = NULL;
		*targ = '\0';
		targ++;
		_alam_log(AM_LOG_DEBUG, "searching for target '%s' in repo '%s'\n", targ, targline);
		for(j = dbs_sync; j; j = j->next) {
			amdb_t *db = j->data;
			if(strcmp(db->treename, targline) == 0) {
				dbs = alam_list_add(NULL, db);
				break;
			}
		}
		if(dbs == NULL) {
			_alam_log(AM_LOG_ERROR, _("repository '%s' not found\n"), targline);
			FREE(targline);
			RET_ERR(AM_ERR_PKG_REPO_NOT_FOUND, -1);
		}
		dep = _alam_splitdep(targ);
		spkg = _alam_resolvedep(dep, dbs, NULL, 1);
		_alam_dep_free(dep);
		alam_list_free(dbs);
	} else {
		dep = _alam_splitdep(targline);
		spkg = _alam_resolvedep(dep, dbs_sync, NULL, 1);
		_alam_dep_free(dep);
	}
	FREE(targline);

	if(spkg == NULL) {
		/* am_errno is set by _alam_resolvedep */
		return(-1);
	}

	if(_alam_pkg_find(trans->add, alam_pkg_get_name(spkg))) {
		RET_ERR(AM_ERR_TRANS_DUP_TARGET, -1);
	}

	local = _alam_db_get_pkgfromcache(db_local, alam_pkg_get_name(spkg));
	if(local) {
		int cmp = _alam_pkg_compare_versions(spkg, local);
		if(cmp == 0) {
			if(trans->flags & AM_TRANS_FLAG_NEEDED) {
				/* with the NEEDED flag, packages up to date are not reinstalled */
				_alam_log(AM_LOG_WARNING, _("%s-%s is up to date -- skipping\n"),
						alam_pkg_get_name(local), alam_pkg_get_version(local));
				return(0);
			} else {
				_alam_log(AM_LOG_WARNING, _("%s-%s is up to date -- reinstalling\n"),
						alam_pkg_get_name(local), alam_pkg_get_version(local));

			}
		} else if(cmp < 0) {
			/* local version is newer */
			_alam_log(AM_LOG_WARNING, _("downgrading package %s (%s => %s)\n"),
					alam_pkg_get_name(local), alam_pkg_get_version(local),
					alam_pkg_get_version(spkg));
		}
	}

	/* add the package to the transaction */
	spkg->reason = AM_PKG_REASON_EXPLICIT;
	_alam_log(AM_LOG_DEBUG, "adding package %s-%s to the transaction targets\n",
						alam_pkg_get_name(spkg), alam_pkg_get_version(spkg));
	trans->add = alam_list_add(trans->add, spkg);

	return(0);
}

/** Compute the size of the files that will be downloaded to install a
 * package.
 * @param newpkg the new package to upgrade to
 */
static int compute_download_size(ampkg_t *newpkg)
{
	const char *fname;
	char *fpath;
	off_t size = 0;

	if(newpkg->origin == PKG_FROM_FILE) {
		newpkg->download_size = 0;
		return(0);
	}

	fname = alam_pkg_get_filename(newpkg);
	ASSERT(fname != NULL, RET_ERR(AM_ERR_PKG_INVALID_NAME, -1));
	fpath = _alam_filecache_find(fname);

	if(fpath) {
		FREE(fpath);
		size = 0;
	} else if(handle->usedelta) {
		off_t dltsize;
		off_t pkgsize = alam_pkg_get_size(newpkg);

		dltsize = _alam_shortest_delta_path(
			alam_pkg_get_deltas(newpkg),
			alam_pkg_get_filename(newpkg),
			&newpkg->delta_path);

		if(newpkg->delta_path && (dltsize < pkgsize * MAX_DELTA_RATIO)) {
			_alam_log(AM_LOG_DEBUG, "using delta size\n");
			size = dltsize;
		} else {
			_alam_log(AM_LOG_DEBUG, "using package size\n");
			size = alam_pkg_get_size(newpkg);
			alam_list_free(newpkg->delta_path);
			newpkg->delta_path = NULL;
		}
	} else {
		size = alam_pkg_get_size(newpkg);
	}

	_alam_log(AM_LOG_DEBUG, "setting download size %jd for pkg %s\n",
			(intmax_t)size, alam_pkg_get_name(newpkg));

	newpkg->download_size = size;
	return(0);
}

int _alam_sync_prepare(amtrans_t *trans, amdb_t *db_local, alam_list_t *dbs_sync, alam_list_t **data)
{
	alam_list_t *deps = NULL;
	alam_list_t *preferred = NULL;
	alam_list_t *unresolvable = NULL;
	alam_list_t *i, *j;
	alam_list_t *remove = NULL;
	int ret = 0;

	ALAM_LOG_FUNC;

	ASSERT(db_local != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));

	if(data) {
		*data = NULL;
	}

	if(!(trans->flags & AM_TRANS_FLAG_NODEPS)) {
		alam_list_t *resolved = NULL; /* target list after resolvedeps */

		/* Build up list by repeatedly resolving each transaction package */
		/* Resolve targets dependencies */
		EVENT(trans, AM_TRANS_EVT_RESOLVEDEPS_START, NULL, NULL);
		_alam_log(AM_LOG_DEBUG, "resolving target's dependencies\n");

		/* build remove list and preferred list for resolvedeps */
		for(i = trans->add; i; i = i->next) {
			ampkg_t *spkg = i->data;
			for(j = spkg->removes; j; j = j->next) {
				remove = alam_list_add(remove, j->data);
			}
			preferred = alam_list_add(preferred, spkg);
		}

		/* Resolve packages in the transaction one at a time, in addtion
		   building up a list of packages which could not be resolved. */
		for(i = trans->add; i; i = i->next) {
			ampkg_t *pkg = i->data;
			if(_alam_resolvedeps(db_local, dbs_sync, pkg, preferred,
						&resolved, remove, data) == -1) {
				unresolvable = alam_list_add(unresolvable, pkg);
			}
			/* Else, [resolved] now additionally contains [pkg] and all of its
			   dependencies not already on the list */
		}
		alam_list_free(preferred);

		/* If there were unresolvable top-level packages, prompt the user to
		   see if they'd like to ignore them rather than failing the sync */
		if(unresolvable != NULL) {
			unresolvable = alam_list_remove_dupes(unresolvable);
			int remove_unresolvable = 0;
			QUESTION(handle->trans, AM_TRANS_CONV_REMOVE_PKGS, unresolvable,
					NULL, NULL, &remove_unresolvable);
			if (remove_unresolvable) {
				/* User wants to remove the unresolvable packages from the
				   transaction. The packages will be removed from the actual
				   transaction when the transaction packages are replaced with a
				   dependency-reordered list below */
				am_errno = 0; /* am_errno was set by resolvedeps */
				if(data) {
					alam_list_free_inner(*data, (alam_list_fn_free)_alam_depmiss_free);
					alam_list_free(*data);
					*data = NULL;
				}
			} else {
				/* am_errno is set by resolvedeps */
				alam_list_free(resolved);
				ret = -1;
				goto cleanup;
			}
		}

		/* Set DEPEND reason for pulled packages */
		for(i = resolved; i; i = i->next) {
			ampkg_t *pkg = i->data;
			if(!_alam_pkg_find(trans->add, pkg->name)) {
				pkg->reason = AM_PKG_REASON_DEPEND;
			}
		}

		/* Unresolvable packages will be removed from the target list, so
		   we free the transaction specific fields */
		alam_list_free_inner(unresolvable, (alam_list_fn_free)_alam_pkg_free_trans);

		/* re-order w.r.t. dependencies */
		alam_list_free(trans->add);
		trans->add = _alam_sortbydeps(resolved, 0);
		alam_list_free(resolved);

		EVENT(trans, AM_TRANS_EVT_RESOLVEDEPS_DONE, NULL, NULL);
	}

	if(!(trans->flags & AM_TRANS_FLAG_NOCONFLICTS)) {
		/* check for inter-conflicts and whatnot */
		EVENT(trans, AM_TRANS_EVT_INTERCONFLICTS_START, NULL, NULL);

		_alam_log(AM_LOG_DEBUG, "looking for conflicts\n");

		/* 1. check for conflicts in the target list */
		_alam_log(AM_LOG_DEBUG, "check targets vs targets\n");
		deps = _alam_innerconflicts(trans->add);

		for(i = deps; i; i = i->next) {
			amconflict_t *conflict = i->data;
			ampkg_t *rsync, *sync, *sync1, *sync2;

			/* have we already removed one of the conflicting targets? */
			sync1 = _alam_pkg_find(trans->add, conflict->package1);
			sync2 = _alam_pkg_find(trans->add, conflict->package2);
			if(!sync1 || !sync2) {
				continue;
			}

			_alam_log(AM_LOG_DEBUG, "conflicting packages in the sync list: '%s' <-> '%s'\n",
					conflict->package1, conflict->package2);

			/* if sync1 provides sync2, we remove sync2 from the targets, and vice versa */
			amdepend_t *dep1 = _alam_splitdep(conflict->package1);
			amdepend_t *dep2 = _alam_splitdep(conflict->package2);
			if(alam_depcmp(sync1, dep2)) {
				rsync = sync2;
				sync = sync1;
			} else if(alam_depcmp(sync2, dep1)) {
				rsync = sync1;
				sync = sync2;
			} else {
				_alam_log(AM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
				am_errno = AM_ERR_CONFLICTING_DEPS;
				ret = -1;
				if(data) {
					amconflict_t *newconflict = _alam_conflict_dup(conflict);
					if(newconflict) {
						*data = alam_list_add(*data, newconflict);
					}
				}
				alam_list_free_inner(deps, (alam_list_fn_free)_alam_conflict_free);
				alam_list_free(deps);
				_alam_dep_free(dep1);
				_alam_dep_free(dep2);
				goto cleanup;
			}
			_alam_dep_free(dep1);
			_alam_dep_free(dep2);

			/* Prints warning */
			_alam_log(AM_LOG_WARNING,
					_("removing '%s' from target list because it conflicts with '%s'\n"),
					rsync->name, sync->name);
			trans->add = alam_list_remove(trans->add, rsync, _alam_pkg_cmp, NULL);
			_alam_pkg_free_trans(rsync); /* rsync is not transaction target anymore */
			continue;
		}

		alam_list_free_inner(deps, (alam_list_fn_free)_alam_conflict_free);
		alam_list_free(deps);
		deps = NULL;

		/* 2. we check for target vs db conflicts (and resolve)*/
		_alam_log(AM_LOG_DEBUG, "check targets vs db and db vs targets\n");
		deps = _alam_outerconflicts(db_local, trans->add);

		for(i = deps; i; i = i->next) {
			amconflict_t *conflict = i->data;

			/* if conflict->package2 (the local package) is not elected for removal,
			   we ask the user */
			int found = 0;
			for(j = trans->add; j && !found; j = j->next) {
				ampkg_t *spkg = j->data;
				if(_alam_pkg_find(spkg->removes, conflict->package2)) {
					found = 1;
				}
			}
			if(found) {
				continue;
			}

			_alam_log(AM_LOG_DEBUG, "package '%s' conflicts with '%s'\n",
					conflict->package1, conflict->package2);

			ampkg_t *sync = _alam_pkg_find(trans->add, conflict->package1);
			ampkg_t *local = _alam_db_get_pkgfromcache(db_local, conflict->package2);
			int doremove = 0;
			QUESTION(trans, AM_TRANS_CONV_CONFLICT_PKG, conflict->package1,
							conflict->package2, conflict->reason, &doremove);
			if(doremove) {
				/* append to the removes list */
				_alam_log(AM_LOG_DEBUG, "electing '%s' for removal\n", conflict->package2);
				sync->removes = alam_list_add(sync->removes, local);
			} else { /* abort */
				_alam_log(AM_LOG_ERROR, _("unresolvable package conflicts detected\n"));
				am_errno = AM_ERR_CONFLICTING_DEPS;
				ret = -1;
				if(data) {
					amconflict_t *newconflict = _alam_conflict_dup(conflict);
					if(newconflict) {
						*data = alam_list_add(*data, newconflict);
					}
				}
				alam_list_free_inner(deps, (alam_list_fn_free)_alam_conflict_free);
				alam_list_free(deps);
				goto cleanup;
			}
		}
		EVENT(trans, AM_TRANS_EVT_INTERCONFLICTS_DONE, NULL, NULL);
		alam_list_free_inner(deps, (alam_list_fn_free)_alam_conflict_free);
		alam_list_free(deps);
	}

	if(!(trans->flags & AM_TRANS_FLAG_NODEPS)) {
		/* rebuild remove list */
		alam_list_free(remove);
		trans->remove = NULL;
		for(i = trans->add; i; i = i->next) {
			ampkg_t *spkg = i->data;
			for(j = spkg->removes; j; j = j->next) {
				trans->remove = alam_list_add(trans->remove, _alam_pkg_dup(j->data));
			}
		}

		_alam_log(AM_LOG_DEBUG, "checking dependencies\n");
		deps = alam_checkdeps(_alam_db_get_pkgcache(db_local), 1, trans->remove, trans->add);
		if(deps) {
			am_errno = AM_ERR_UNSATISFIED_DEPS;
			ret = -1;
			if(data) {
				*data = deps;
			} else {
				alam_list_free_inner(deps, (alam_list_fn_free)_alam_depmiss_free);
				alam_list_free(deps);
			}
			goto cleanup;
		}
	}
	for(i = trans->add; i; i = i->next) {
		/* update download size field */
		ampkg_t *spkg = i->data;
		if(compute_download_size(spkg) != 0) {
			ret = -1;
			goto cleanup;
		}
	}

cleanup:
	alam_list_free(unresolvable);

	return(ret);
}

/** Returns the size of the files that will be downloaded to install a
 * package.
 * @param newpkg the new package to upgrade to
 * @return the size of the download
 */
off_t SYMEXPORT alam_pkg_download_size(ampkg_t *newpkg)
{
	return(newpkg->download_size);
}

static int endswith(const char *filename, const char *extension)
{
	const char *s = filename + strlen(filename) - strlen(extension);
	return(strcmp(s, extension) == 0);
}

/** Applies delta files to create an upgraded package file.
 *
 * All intermediate files are deleted, leaving only the starting and
 * ending package files.
 *
 * @param trans the transaction
 *
 * @return 0 if all delta files were able to be applied, 1 otherwise.
 */
static int apply_deltas(amtrans_t *trans)
{
	alam_list_t *i;
	int ret = 0;
	const char *cachedir = _alam_filecache_setup();

	for(i = trans->add; i; i = i->next) {
		ampkg_t *spkg = i->data;
		alam_list_t *delta_path = spkg->delta_path;
		alam_list_t *dlts = NULL;

		if(!delta_path) {
			continue;
		}

		for(dlts = delta_path; dlts; dlts = dlts->next) {
			amdelta_t *d = dlts->data;
			char *delta, *from, *to;
			char command[PATH_MAX];
			int len = 0;

			delta = _alam_filecache_find(d->delta);
			/* the initial package might be in a different cachedir */
			if(dlts == delta_path) {
				from = _alam_filecache_find(d->from);
			} else {
				/* len = cachedir len + from len + '/' + null */
				len = strlen(cachedir) + strlen(d->from) + 2;
				CALLOC(from, len, sizeof(char), RET_ERR(AM_ERR_MEMORY, 1));
				snprintf(from, len, "%s/%s", cachedir, d->from);
			}
			len = strlen(cachedir) + strlen(d->to) + 2;
			CALLOC(to, len, sizeof(char), RET_ERR(AM_ERR_MEMORY, 1));
			snprintf(to, len, "%s/%s", cachedir, d->to);

			/* build the patch command */
			if(endswith(to, ".gz")) {
				/* special handling for gzip : we disable timestamp with -n option */
				snprintf(command, PATH_MAX, "xdelta3 -d -q -R -c -s %s %s | gzip -n > %s", from, delta, to);
			} else {
				snprintf(command, PATH_MAX, "xdelta3 -d -q -s %s %s %s", from, delta, to);
			}

			_alam_log(AM_LOG_DEBUG, "command: %s\n", command);

			EVENT(trans, AM_TRANS_EVT_DELTA_PATCH_START, d->to, d->delta);

			int retval = system(command);
			if(retval == 0) {
				EVENT(trans, AM_TRANS_EVT_DELTA_PATCH_DONE, NULL, NULL);

				/* delete the delta file */
				unlink(delta);

				/* Delete the 'from' package but only if it is an intermediate
				 * package. The starting 'from' package should be kept, just
				 * as if deltas were not used. */
				if(dlts != delta_path) {
					unlink(from);
				}
			}
			FREE(from);
			FREE(to);
			FREE(delta);

			if(retval != 0) {
				/* one delta failed for this package, cancel the remaining ones */
				EVENT(trans, AM_TRANS_EVT_DELTA_PATCH_FAILED, NULL, NULL);
				ret = 1;
				break;
			}
		}
	}

	return(ret);
}

/** Compares the md5sum of a file to the expected value.
 *
 * If the md5sum does not match, the user is asked whether the file
 * should be deleted.
 *
 * @param trans the transaction
 * @param filename the filename of the file to test
 * @param md5sum the expected md5sum of the file
 *
 * @return 0 if the md5sum matched, 1 if not, -1 in case of errors
 */
static int test_md5sum(amtrans_t *trans, const char *filename,
		const char *md5sum)
{
	char *filepath;
	int ret;

	filepath = _alam_filecache_find(filename);

	ret = _alam_test_md5sum(filepath, md5sum);

	if(ret == 1) {
		int doremove = 0;
		QUESTION(trans, AM_TRANS_CONV_CORRUPTED_PKG, (char *)filename,
				NULL, NULL, &doremove);
		if(doremove) {
			unlink(filepath);
		}
	}

	FREE(filepath);

	return(ret);
}

int _alam_sync_commit(amtrans_t *trans, amdb_t *db_local, alam_list_t **data)
{
	alam_list_t *i, *j, *files = NULL;
	alam_list_t *deltas = NULL;
	int replaces = 0;
	int errors = 0;
	const char *cachedir = NULL;
	int ret = -1;

	ALAM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));

	cachedir = _alam_filecache_setup();
	trans->state = STATE_DOWNLOADING;

	/* Total progress - figure out the total download size if required to
	 * pass to the callback. This function is called once, and it is up to the
	 * frontend to compute incremental progress. */
	if(handle->totaldlcb) {
		off_t total_size = (off_t)0;
		/* sum up the download size for each package and store total */
		for(i = trans->add; i; i = i->next) {
			ampkg_t *spkg = i->data;
			total_size += spkg->download_size;
		}
		handle->totaldlcb(total_size);
	}

	/* group sync records by repository and download */
	for(i = handle->dbs_sync; i; i = i->next) {
		amdb_t *current = i->data;

		for(j = trans->add; j; j = j->next) {
			ampkg_t *spkg = j->data;

			if(spkg->origin == PKG_FROM_CACHE && current == spkg->origin_data.db) {
				const char *fname = NULL;

				fname = alam_pkg_get_filename(spkg);
				ASSERT(fname != NULL, RET_ERR(AM_ERR_PKG_INVALID_NAME, -1));
				alam_list_t *delta_path = spkg->delta_path;
				if(delta_path) {
					/* using deltas */
					alam_list_t *dlts = NULL;

					for(dlts = delta_path; dlts; dlts = dlts->next) {
						amdelta_t *d = dlts->data;

						if(d->download_size != 0) {
							/* add the delta filename to the download list if needed */
							files = alam_list_add(files, strdup(d->delta));
						}

						/* keep a list of all the delta files for md5sums */
						deltas = alam_list_add(deltas, d);
					}

				} else {
					/* not using deltas */
					if(spkg->download_size != 0) {
						/* add the filename to the download list if needed */
						files = alam_list_add(files, strdup(fname));
					}
				}

			}
		}

		if(files) {
			EVENT(trans, AM_TRANS_EVT_RETRIEVE_START, current->treename, NULL);
			if(_alam_download_files(files, current->servers, cachedir)) {
				_alam_log(AM_LOG_WARNING, _("failed to retrieve some files from %s\n"),
						current->treename);
				if(am_errno == 0) {
					am_errno = AM_ERR_RETRIEVE;
				}
				goto error;
			}
			FREELIST(files);
		}
	}

	/* clear out value to let callback know we are done */
	if(handle->totaldlcb) {
		handle->totaldlcb(0);
	}

	/* if we have deltas to work with */
	if(handle->usedelta && deltas) {
		int ret = 0;
		errors = 0;
		/* Check integrity of deltas */
		EVENT(trans, AM_TRANS_EVT_DELTA_INTEGRITY_START, NULL, NULL);

		for(i = deltas; i; i = i->next) {
			amdelta_t *d = alam_list_getdata(i);
			const char *filename = alam_delta_get_filename(d);
			const char *md5sum = alam_delta_get_md5sum(d);

			if(test_md5sum(trans, filename, md5sum) != 0) {
				errors++;
				*data = alam_list_add(*data, strdup(filename));
			}
		}
		if(errors) {
			am_errno = AM_ERR_DLT_INVALID;
			goto error;
		}
		EVENT(trans, AM_TRANS_EVT_DELTA_INTEGRITY_DONE, NULL, NULL);

		/* Use the deltas to generate the packages */
		EVENT(trans, AM_TRANS_EVT_DELTA_PATCHES_START, NULL, NULL);
		ret = apply_deltas(trans);
		EVENT(trans, AM_TRANS_EVT_DELTA_PATCHES_DONE, NULL, NULL);

		if(ret) {
			am_errno = AM_ERR_DLT_PATCHFAILED;
			goto error;
		}
	}

	/* Check integrity of packages */
	EVENT(trans, AM_TRANS_EVT_INTEGRITY_START, NULL, NULL);

	errors = 0;
	for(i = trans->add; i; i = i->next) {
		ampkg_t *spkg = i->data;
		if(spkg->origin == PKG_FROM_FILE) {
			continue; /* pkg_load() has been already called, this package is valid */
		}

		const char *filename = alam_pkg_get_filename(spkg);
		const char *md5sum = alam_pkg_get_md5sum(spkg);

		if(test_md5sum(trans, filename, md5sum) != 0) {
			errors++;
			*data = alam_list_add(*data, strdup(filename));
			continue;
		}
		/* load the package file and replace pkgcache entry with it in the target list */
		/* TODO: alam_pkg_get_db() will not work on this target anymore */
		_alam_log(AM_LOG_DEBUG, "replacing pkgcache entry with package file for target %s\n", spkg->name);
		char *filepath = _alam_filecache_find(filename);
		ampkg_t *pkgfile;
		if(alam_pkg_load(filepath, 1, &pkgfile) != 0) {
			_alam_pkg_free(pkgfile);
			errors++;
			*data = alam_list_add(*data, strdup(filename));
			FREE(filepath);
			continue;
		}
		FREE(filepath);
		pkgfile->reason = spkg->reason; /* copy over install reason */
		i->data = pkgfile;
		_alam_pkg_free_trans(spkg); /* spkg has been removed from the target list */
	}
	if(errors) {
		am_errno = AM_ERR_PKG_INVALID;
		goto error;
	}
	EVENT(trans, AM_TRANS_EVT_INTEGRITY_DONE, NULL, NULL);
	if(trans->flags & AM_TRANS_FLAG_DOWNLOADONLY) {
		return(0);
	}

	trans->state = STATE_COMMITING;

	replaces = alam_list_count(trans->remove);

	/* fileconflict check */
	if(!(trans->flags & AM_TRANS_FLAG_FORCE)) {
		EVENT(trans, AM_TRANS_EVT_FILECONFLICTS_START, NULL, NULL);

		_alam_log(AM_LOG_DEBUG, "looking for file conflicts\n");
		alam_list_t *conflict = _alam_db_find_fileconflicts(db_local, trans,
								    trans->add, trans->remove);
		if(conflict) {
			am_errno = AM_ERR_FILE_CONFLICTS;
			if(data) {
				*data = conflict;
			} else {
				alam_list_free_inner(conflict, (alam_list_fn_free)_alam_fileconflict_free);
				alam_list_free(conflict);
			}
			goto error;
		}

		EVENT(trans, AM_TRANS_EVT_FILECONFLICTS_DONE, NULL, NULL);
	}

	/* remove conflicting and to-be-replaced packages */
	if(replaces) {
		_alam_log(AM_LOG_DEBUG, "removing conflicting and to-be-replaced packages\n");
		/* we want the frontend to be aware of commit details */
		if(_alam_remove_packages(trans, handle->db_local) == -1) {
			_alam_log(AM_LOG_ERROR, _("could not commit removal transaction\n"));
			goto error;
		}
	}

	/* install targets */
	_alam_log(AM_LOG_DEBUG, "installing packages\n");
	if(_alam_upgrade_packages(trans, handle->db_local) == -1) {
		_alam_log(AM_LOG_ERROR, _("could not commit transaction\n"));
		goto error;
	}
	ret = 0;

error:
	FREELIST(files);
	alam_list_free(deltas);
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
