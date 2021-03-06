/*
 *  add.c
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
#include <errno.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h> /* int64_t */
#include <stdint.h> /* intmax_t */

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalam */
#include "add.h"
#include "alam_list.h"
#include "trans.h"
#include "util.h"
#include "cache.h"
#include "log.h"
#include "backup.h"
#include "package.h"
#include "db.h"
#include "conflict.h"
#include "deps.h"
#include "remove.h"
#include "handle.h"

int _alam_add_loadtarget(amtrans_t *trans, amdb_t *db, char *name)
{
	ampkg_t *pkg = NULL;
	const char *pkgname, *pkgver;
	alam_list_t *i;

	ALAM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));
	ASSERT(name != NULL && strlen(name) != 0, RET_ERR(AM_ERR_WRONG_ARGS, -1));

	_alam_log(AM_LOG_DEBUG, "loading target '%s'\n", name);

	if(alam_pkg_load(name, 1, &pkg) != 0) {
		goto error;
	}
	pkgname = alam_pkg_get_name(pkg);
	pkgver = alam_pkg_get_version(pkg);

	/* check if an older version of said package is already in transaction
	 * packages.  if so, replace it in the list */
	for(i = trans->add; i; i = i->next) {
		ampkg_t *transpkg = i->data;
		if(strcmp(transpkg->name, pkgname) == 0) {
			if(alam_pkg_vercmp(transpkg->version, pkgver) < 0) {
				_alam_log(AM_LOG_WARNING, _("replacing older version %s-%s by %s in target list\n"),
				          transpkg->name, transpkg->version, pkgver);
				_alam_pkg_free(i->data);
				i->data = pkg;
			} else {
				_alam_log(AM_LOG_WARNING, _("skipping %s-%s because newer version %s is in the target list\n"),
				          pkgname, pkgver, transpkg->version);
				_alam_pkg_free(pkg);
			}
			return(0);
		}
	}

	/* add the package to the transaction */
	trans->add = alam_list_add(trans->add, pkg);

	return(0);

error:
	_alam_pkg_free(pkg);
	return(-1);
}

static int extract_single_file(struct archive *archive,
		struct archive_entry *entry, ampkg_t *newpkg, ampkg_t *oldpkg,
		amtrans_t *trans, amdb_t *db)
{
	const char *entryname;
	mode_t entrymode;
	char filename[PATH_MAX]; /* the actual file we're extracting */
	int needbackup = 0, notouch = 0;
	char *hash_orig = NULL;
	char *entryname_orig = NULL;
	const int archive_flags = ARCHIVE_EXTRACT_OWNER |
	                          ARCHIVE_EXTRACT_PERM |
	                          ARCHIVE_EXTRACT_TIME;
	int errors = 0;

	entryname = archive_entry_pathname(entry);
	entrymode = archive_entry_mode(entry);

	memset(filename, 0, PATH_MAX); /* just to be sure */

	if(strcmp(entryname, ".INSTALL") == 0) {
		/* the install script goes inside the db */
		snprintf(filename, PATH_MAX, "%s%s-%s/install", db->path,
				newpkg->name, newpkg->version);
		archive_entry_set_perm(entry, 0644);
	} else if(strcmp(entryname, ".CHANGELOG") == 0) {
		/* the changelog goes inside the db */
		snprintf(filename, PATH_MAX, "%s%s-%s/changelog", db->path,
				newpkg->name, newpkg->version);
		archive_entry_set_perm(entry, 0644);
	} else if(*entryname == '.') {
		/* for now, ignore all files starting with '.' that haven't
		 * already been handled (for future possibilities) */
		_alam_log(AM_LOG_DEBUG, "skipping extraction of '%s'\n", entryname);
		archive_read_data_skip(archive);
		return(0);
	} else {
		/* build the new entryname relative to handle->root */
		snprintf(filename, PATH_MAX, "%s%s", handle->root, entryname);
	}

	/* if a file is in NoExtract then we never extract it */
	if(alam_list_find_str(handle->noextract, entryname)) {
		_alam_log(AM_LOG_DEBUG, "%s is in NoExtract, skipping extraction\n",
				entryname);
		alam_logaction("note: %s is in NoExtract, skipping extraction\n",
				entryname);
		archive_read_data_skip(archive);
		return(0);
	}

	/* if a file is in the add skiplist we never extract it */
	if(alam_list_find_str(trans->skip_add, filename)) {
		_alam_log(AM_LOG_DEBUG, "%s is in trans->skip_add, skipping extraction\n",
				entryname);
		archive_read_data_skip(archive);
		return(0);
	}

	/* Check for file existence. This is one of the more crucial parts
	 * to get 'right'. Here are the possibilities, with the filesystem
	 * on the left and the package on the top:
	 * (F=file, N=node, S=symlink, D=dir)
	 *               |  F/N  |  S  |  D
	 *  non-existent |   1   |  2  |  3
	 *  F/N          |   4   |  5  |  6
	 *  S            |   7   |  8  |  9
	 *  D            |   10  |  11 |  12
	 *
	 *  1,2,3- extract, no magic necessary. lstat (_alam_lstat) will fail here.
	 *  4,5,6,7,8- conflict checks should have caught this. either overwrite
	 *      or backup the file.
	 *  9- follow the symlink, hopefully it is a directory, check it.
	 *  10- file replacing directory- don't allow it.
	 *  11- don't extract symlink- a dir exists here. we don't want links to
	 *      links, etc.
	 *  12- skip extraction, dir already exists.
	 */

	/* do both a lstat and a stat, so we can see what symlinks point to */
	struct stat lsbuf, sbuf;
	if(_alam_lstat(filename, &lsbuf) != 0 || stat(filename, &sbuf) != 0) {
		/* cases 1,2,3: couldn't stat an existing file, skip all backup checks */
	} else {
		if(S_ISDIR(lsbuf.st_mode)) {
			if(S_ISDIR(entrymode)) {
				/* case 12: existing dir, ignore it */
				if(lsbuf.st_mode != entrymode) {
					/* if filesystem perms are different than pkg perms, warn user */
					int mask = 07777;
					_alam_log(AM_LOG_WARNING, _("directory permissions differ on %s\n"
								"filesystem: %o  package: %o\n"), entryname, lsbuf.st_mode & mask,
							entrymode & mask);
					alam_logaction("warning: directory permissions differ on %s\n"
							"filesystem: %o  package: %o\n", entryname, lsbuf.st_mode & mask,
							entrymode & mask);
				}
				_alam_log(AM_LOG_DEBUG, "extract: skipping dir extraction of %s\n",
						entryname);
				archive_read_data_skip(archive);
				return(0);
			} else {
				/* case 10/11: trying to overwrite dir with file/symlink, don't allow it */
				_alam_log(AM_LOG_ERROR, _("extract: not overwriting dir with file %s\n"),
						entryname);
				archive_read_data_skip(archive);
				return(1);
			}
		} else if(S_ISLNK(lsbuf.st_mode) && S_ISDIR(entrymode)) {
			/* case 9: existing symlink, dir in package */
			if(S_ISDIR(sbuf.st_mode)) {
				/* the symlink on FS is to a directory, so we'll use it */
				_alam_log(AM_LOG_DEBUG, "extract: skipping symlink overwrite of %s\n",
						entryname);
				archive_read_data_skip(archive);
				return(0);
			} else {
				/* this is BAD. symlink was not to a directory */
				_alam_log(AM_LOG_ERROR, _("extract: symlink %s does not point to dir\n"),
						entryname);
				archive_read_data_skip(archive);
				return(1);
			}
		} else if(S_ISREG(lsbuf.st_mode) && S_ISDIR(entrymode)) {
			/* case 6: trying to overwrite file with dir */
			_alam_log(AM_LOG_DEBUG, "extract: overwriting file with dir %s\n",
					entryname);
		} else if(S_ISREG(entrymode)) {
			/* case 4,7: */
			/* if file is in NoUpgrade, don't touch it */
			if(alam_list_find_str(handle->noupgrade, entryname)) {
				notouch = 1;
			} else {
				/* go to the backup array and see if our conflict is there */
				/* check newpkg first, so that adding backup files is retroactive */
				if(alam_list_find_str(alam_pkg_get_backup(newpkg), entryname) != NULL) {
					needbackup = 1;
				}

				/* check oldpkg for a backup entry, store the hash if available */
				if(oldpkg) {
					hash_orig = _alam_needbackup(entryname, alam_pkg_get_backup(oldpkg));
					if(hash_orig) {
						needbackup = 1;
					}
				}

				/* if we force hash_orig to be non-NULL retroactive backup works */
				if(needbackup && !hash_orig) {
					STRDUP(hash_orig, "", RET_ERR(AM_ERR_MEMORY, -1));
				}
			}
		}
		/* else if(S_ISLNK(entrymode)) */
		/* case 5,8: don't need to do anything special */
	}

	/* we need access to the original entryname later after calls to
	 * archive_entry_set_pathname(), so we need to dupe it and free() later */
	STRDUP(entryname_orig, entryname, RET_ERR(AM_ERR_MEMORY, -1));

	if(needbackup) {
		char checkfile[PATH_MAX];
		char *hash_local = NULL, *hash_pkg = NULL;
		int ret;

		snprintf(checkfile, PATH_MAX, "%s.paccheck", filename);
		archive_entry_set_pathname(entry, checkfile);

		ret = archive_read_extract(archive, entry, archive_flags);
		if(ret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			_alam_log(AM_LOG_DEBUG, "warning extracting %s (%s)\n",
					entryname_orig, archive_error_string(archive));
		} else if(ret != ARCHIVE_OK) {
			_alam_log(AM_LOG_ERROR, _("could not extract %s (%s)\n"),
					entryname_orig, archive_error_string(archive));
			alam_logaction("error: could not extract %s (%s)\n",
					entryname_orig, archive_error_string(archive));
			FREE(hash_orig);
			FREE(entryname_orig);
			return(1);
		}

		hash_local = alam_compute_md5sum(filename);
		hash_pkg = alam_compute_md5sum(checkfile);

		/* append the new md5 hash to it's respective entry
		 * in newpkg's backup (it will be the new orginal) */
		alam_list_t *backups;
		for(backups = alam_pkg_get_backup(newpkg); backups;
				backups = alam_list_next(backups)) {
			char *oldbackup = alam_list_getdata(backups);
			if(!oldbackup || strcmp(oldbackup, entryname_orig) != 0) {
				continue;
			}
			char *backup = NULL;
			/* length is tab char, null byte and MD5 (32 char) */
			size_t backup_len = strlen(oldbackup) + 34;
			MALLOC(backup, backup_len, RET_ERR(AM_ERR_MEMORY, -1));

			sprintf(backup, "%s\t%s", oldbackup, hash_pkg);
			backup[backup_len-1] = '\0';
			FREE(oldbackup);
			backups->data = backup;
		}

		_alam_log(AM_LOG_DEBUG, "checking hashes for %s\n", entryname_orig);
		_alam_log(AM_LOG_DEBUG, "current:  %s\n", hash_local);
		_alam_log(AM_LOG_DEBUG, "new:      %s\n", hash_pkg);
		_alam_log(AM_LOG_DEBUG, "original: %s\n", hash_orig);

		if(!oldpkg) {
			if(strcmp(hash_local, hash_pkg) != 0) {
				/* looks like we have a local file that has a different hash as the
				 * file in the package, move it to a .pacorig */
				char newpath[PATH_MAX];
				snprintf(newpath, PATH_MAX, "%s.pacorig", filename);

				/* move the existing file to the "pacorig" */
				if(rename(filename, newpath)) {
					_alam_log(AM_LOG_ERROR, _("could not rename %s to %s (%s)\n"),
							filename, newpath, strerror(errno));
					alam_logaction("error: could not rename %s to %s (%s)\n",
							filename, newpath, strerror(errno));
					errors++;
				} else {
					/* rename the file we extracted to the real name */
					if(rename(checkfile, filename)) {
						_alam_log(AM_LOG_ERROR, _("could not rename %s to %s (%s)\n"),
								checkfile, filename, strerror(errno));
						alam_logaction("error: could not rename %s to %s (%s)\n",
								checkfile, filename, strerror(errno));
						errors++;
					} else {
						_alam_log(AM_LOG_WARNING, _("%s saved as %s\n"), filename, newpath);
						alam_logaction("warning: %s saved as %s\n", filename, newpath);
					}
				}
			} else {
				/* local file is identical to pkg one, so just remove pkg one */
				unlink(checkfile);
			}
		} else if(hash_orig) {
			/* the fun part */

			if(strcmp(hash_orig, hash_local) == 0) {
				/* installed file has NOT been changed by user */
				if(strcmp(hash_orig, hash_pkg) != 0) {
					_alam_log(AM_LOG_DEBUG, "action: installing new file: %s\n",
							entryname_orig);

					if(rename(checkfile, filename)) {
						_alam_log(AM_LOG_ERROR, _("could not rename %s to %s (%s)\n"),
								checkfile, filename, strerror(errno));
						alam_logaction("error: could not rename %s to %s (%s)\n",
								checkfile, filename, strerror(errno));
						errors++;
					}
				} else {
					/* there's no sense in installing the same file twice, install
					 * ONLY is the original and package hashes differ */
					_alam_log(AM_LOG_DEBUG, "action: leaving existing file in place\n");
					unlink(checkfile);
				}
			} else if(strcmp(hash_orig, hash_pkg) == 0) {
				/* originally installed file and new file are the same - this
				 * implies the case above failed - i.e. the file was changed by a
				 * user */
				_alam_log(AM_LOG_DEBUG, "action: leaving existing file in place\n");
				unlink(checkfile);
			} else if(strcmp(hash_local, hash_pkg) == 0) {
				/* this would be magical.  The above two cases failed, but the
				 * user changes just so happened to make the new file exactly the
				 * same as the one in the package... skip it */
				_alam_log(AM_LOG_DEBUG, "action: leaving existing file in place\n");
				unlink(checkfile);
			} else {
				char newpath[PATH_MAX];
				_alam_log(AM_LOG_DEBUG, "action: keeping current file and installing"
						" new one with .pacnew ending\n");
				snprintf(newpath, PATH_MAX, "%s.pacnew", filename);
				if(rename(checkfile, newpath)) {
					_alam_log(AM_LOG_ERROR, _("could not install %s as %s (%s)\n"),
							filename, newpath, strerror(errno));
					alam_logaction("error: could not install %s as %s (%s)\n",
							filename, newpath, strerror(errno));
				} else {
					_alam_log(AM_LOG_WARNING, _("%s installed as %s\n"),
							filename, newpath);
					alam_logaction("warning: %s installed as %s\n",
							filename, newpath);
				}
			}
		}

		FREE(hash_local);
		FREE(hash_pkg);
		FREE(hash_orig);
	} else {
		int ret;

		/* we didn't need a backup */
		if(notouch) {
			/* change the path to a .pacnew extension */
			_alam_log(AM_LOG_DEBUG, "%s is in NoUpgrade -- skipping\n", filename);
			_alam_log(AM_LOG_WARNING, _("extracting %s as %s.pacnew\n"), filename, filename);
			alam_logaction("warning: extracting %s as %s.pacnew\n", filename, filename);
			strncat(filename, ".pacnew", PATH_MAX - strlen(filename));
		} else {
			_alam_log(AM_LOG_DEBUG, "extracting %s\n", filename);
		}

		if(trans->flags & AM_TRANS_FLAG_FORCE) {
			/* if FORCE was used, unlink() each file (whether it's there
			 * or not) before extracting. This prevents the old "Text file busy"
			 * error that crops up if forcing a glibc or pacman upgrade. */
			unlink(filename);
		}

		archive_entry_set_pathname(entry, filename);

		ret = archive_read_extract(archive, entry, archive_flags);
		if(ret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			_alam_log(AM_LOG_DEBUG, "warning extracting %s (%s)\n",
					entryname_orig, archive_error_string(archive));
		} else if(ret != ARCHIVE_OK) {
			_alam_log(AM_LOG_ERROR, _("could not extract %s (%s)\n"),
					entryname_orig, archive_error_string(archive));
			alam_logaction("error: could not extract %s (%s)\n",
					entryname_orig, archive_error_string(archive));
			FREE(entryname_orig);
			return(1);
		}

		/* calculate an hash if this is in newpkg's backup */
		alam_list_t *b;
		for(b = alam_pkg_get_backup(newpkg); b; b = b->next) {
			char *backup = NULL, *hash = NULL;
			char *oldbackup = alam_list_getdata(b);
			/* length is tab char, null byte and MD5 (32 char) */
			size_t backup_len = strlen(oldbackup) + 34;

			if(!oldbackup || strcmp(oldbackup, entryname_orig) != 0) {
				continue;
			}
			_alam_log(AM_LOG_DEBUG, "appending backup entry for %s\n", filename);

			hash = alam_compute_md5sum(filename);
			MALLOC(backup, backup_len, RET_ERR(AM_ERR_MEMORY, -1));

			sprintf(backup, "%s\t%s", oldbackup, hash);
			backup[backup_len-1] = '\0';
			FREE(hash);
			FREE(oldbackup);
			b->data = backup;
		}
	}
	FREE(entryname_orig);
	return(errors);
}

static int commit_single_pkg(ampkg_t *newpkg, int pkg_current, int pkg_count,
		amtrans_t *trans, amdb_t *db)
{
	int i, ret = 0, errors = 0;
	char scriptlet[PATH_MAX+1];
	int is_upgrade = 0;
	ampkg_t *oldpkg = NULL;

	ALAM_LOG_FUNC;

	snprintf(scriptlet, PATH_MAX, "%s%s-%s/install", db->path,
			alam_pkg_get_name(newpkg), alam_pkg_get_version(newpkg));

	/* see if this is an upgrade. if so, remove the old package first */
	ampkg_t *local = _alam_db_get_pkgfromcache(db, newpkg->name);
	if(local) {
		is_upgrade = 1;

		/* we'll need to save some record for backup checks later */
		oldpkg = _alam_pkg_dup(local);
		/* make sure all infos are loaded because the database entry
		 * will be removed soon */
		_alam_db_read(oldpkg->origin_data.db, oldpkg, INFRQ_ALL);

		EVENT(trans, AM_TRANS_EVT_UPGRADE_START, newpkg, oldpkg);
		_alam_log(AM_LOG_DEBUG, "upgrading package %s-%s\n",
				newpkg->name, newpkg->version);

		/* copy over the install reason */
		newpkg->reason = alam_pkg_get_reason(oldpkg);

		/* pre_upgrade scriptlet */
		if(alam_pkg_has_scriptlet(newpkg) && !(trans->flags & AM_TRANS_FLAG_NOSCRIPTLET)) {
			_alam_runscriptlet(handle->root, newpkg->origin_data.file,
					"pre_upgrade", newpkg->version, oldpkg->version, trans);
		}
	} else {
		is_upgrade = 0;

		EVENT(trans, AM_TRANS_EVT_ADD_START, newpkg, NULL);
		_alam_log(AM_LOG_DEBUG, "adding package %s-%s\n",
				newpkg->name, newpkg->version);

		/* pre_install scriptlet */
		if(alam_pkg_has_scriptlet(newpkg) && !(trans->flags & AM_TRANS_FLAG_NOSCRIPTLET)) {
			_alam_runscriptlet(handle->root, newpkg->origin_data.file,
					"pre_install", newpkg->version, NULL, trans);
		}
	}

	/* we override any pre-set reason if we have alldeps or allexplicit set */
	if(trans->flags & AM_TRANS_FLAG_ALLDEPS) {
		newpkg->reason = AM_PKG_REASON_DEPEND;
	} else if(trans->flags & AM_TRANS_FLAG_ALLEXPLICIT) {
		newpkg->reason = AM_PKG_REASON_EXPLICIT;
	}

	if(oldpkg) {
		/* set up fake remove transaction */
		if(_alam_upgraderemove_package(oldpkg, newpkg, trans) == -1) {
			am_errno = AM_ERR_TRANS_ABORT;
			ret = -1;
			goto cleanup;
		}
	}

	/* prepare directory for database entries so permission are correct after
	   changelog/install script installation (FS#12263) */
	if(_alam_db_prepare(db, newpkg)) {
		alam_logaction("error: could not create database entry %s-%s\n",
				alam_pkg_get_name(newpkg), alam_pkg_get_version(newpkg));
		am_errno = AM_ERR_DB_WRITE;
		ret = -1;
		goto cleanup;
	}

	if(!(trans->flags & AM_TRANS_FLAG_DBONLY)) {
		struct archive *archive;
		struct archive_entry *entry;
		char cwd[PATH_MAX] = "";

		_alam_log(AM_LOG_DEBUG, "extracting files\n");

		if ((archive = archive_read_new()) == NULL) {
			am_errno = AM_ERR_LIBARCHIVE;
			ret = -1;
			goto cleanup;
		}

		archive_read_support_compression_all(archive);
		archive_read_support_format_all(archive);

		_alam_log(AM_LOG_DEBUG, "archive: %s\n", newpkg->origin_data.file);
		if(archive_read_open_filename(archive, newpkg->origin_data.file,
					ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
			am_errno = AM_ERR_PKG_OPEN;
			ret = -1;
			goto cleanup;
		}

		/* save the cwd so we can restore it later */
		if(getcwd(cwd, PATH_MAX) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not get current working directory\n"));
			cwd[0] = 0;
		}

		/* libarchive requires this for extracting hard links */
		chdir(handle->root);

		/* call PROGRESS once with 0 percent, as we sort-of skip that here */
		if(is_upgrade) {
			PROGRESS(trans, AM_TRANS_PROGRESS_UPGRADE_START,
					alam_pkg_get_name(newpkg), 0, pkg_count, pkg_current);
		} else {
			PROGRESS(trans, AM_TRANS_PROGRESS_ADD_START,
					alam_pkg_get_name(newpkg), 0, pkg_count, pkg_current);
		}

		for(i = 0; archive_read_next_header(archive, &entry) == ARCHIVE_OK; i++) {
			double percent;

			if(newpkg->size != 0) {
				/* Using compressed size for calculations here, as newpkg->isize is not
				 * exact when it comes to comparing to the ACTUAL uncompressed size
				 * (missing metadata sizes) */
				int64_t pos = archive_position_compressed(archive);
				percent = (double)pos / (double)newpkg->size;
				_alam_log(AM_LOG_DEBUG, "decompression progress: "
						"%f%% (%"PRId64" / %jd)\n",
						percent*100.0, pos, (intmax_t)newpkg->size);
				if(percent >= 1.0) {
					percent = 1.0;
				}
			} else {
				percent = 0.0;
			}

			if(is_upgrade) {
				PROGRESS(trans, AM_TRANS_PROGRESS_UPGRADE_START,
						alam_pkg_get_name(newpkg), (int)(percent * 100), pkg_count,
						pkg_current);
			} else {
				PROGRESS(trans, AM_TRANS_PROGRESS_ADD_START,
						alam_pkg_get_name(newpkg), (int)(percent * 100), pkg_count,
						pkg_current);
			}

			/* extract the next file from the archive */
			errors += extract_single_file(archive, entry, newpkg, oldpkg,
					trans, db);
		}
		archive_read_finish(archive);

		/* restore the old cwd is we have it */
		if(strlen(cwd)) {
			chdir(cwd);
		}

		if(errors) {
			ret = -1;
			if(is_upgrade) {
				_alam_log(AM_LOG_ERROR, _("problem occurred while upgrading %s\n"),
						newpkg->name);
				alam_logaction("error: problem occurred while upgrading %s\n",
						newpkg->name);
			} else {
				_alam_log(AM_LOG_ERROR, _("problem occurred while installing %s\n"),
						newpkg->name);
				alam_logaction("error: problem occurred while installing %s\n",
						newpkg->name);
			}
		}
	}

	/* make an install date (in UTC) */
	newpkg->installdate = time(NULL);

	_alam_log(AM_LOG_DEBUG, "updating database\n");
	_alam_log(AM_LOG_DEBUG, "adding database entry '%s'\n", newpkg->name);

	if(_alam_db_write(db, newpkg, INFRQ_ALL)) {
		_alam_log(AM_LOG_ERROR, _("could not update database entry %s-%s\n"),
				alam_pkg_get_name(newpkg), alam_pkg_get_version(newpkg));
		alam_logaction("error: could not update database entry %s-%s\n",
				alam_pkg_get_name(newpkg), alam_pkg_get_version(newpkg));
		am_errno = AM_ERR_DB_WRITE;
		ret = -1;
		goto cleanup;
	}

	if(_alam_db_add_pkgincache(db, newpkg) == -1) {
		_alam_log(AM_LOG_ERROR, _("could not add entry '%s' in cache\n"),
				alam_pkg_get_name(newpkg));
	}

	if(is_upgrade) {
		PROGRESS(trans, AM_TRANS_PROGRESS_UPGRADE_START,
				alam_pkg_get_name(newpkg), 100, pkg_count, pkg_current);
	} else {
		PROGRESS(trans, AM_TRANS_PROGRESS_ADD_START,
				alam_pkg_get_name(newpkg), 100, pkg_count, pkg_current);
	}

	/* run the post-install script if it exists  */
	if(alam_pkg_has_scriptlet(newpkg)
			&& !(trans->flags & AM_TRANS_FLAG_NOSCRIPTLET)) {
		if(is_upgrade) {
			_alam_runscriptlet(handle->root, scriptlet, "post_upgrade",
					alam_pkg_get_version(newpkg),
					oldpkg ? alam_pkg_get_version(oldpkg) : NULL, trans);
		} else {
			_alam_runscriptlet(handle->root, scriptlet, "post_install",
					alam_pkg_get_version(newpkg), NULL, trans);
		}
	}

	if(is_upgrade) {
		EVENT(trans, AM_TRANS_EVT_UPGRADE_DONE, newpkg, oldpkg);
	} else {
		EVENT(trans, AM_TRANS_EVT_ADD_DONE, newpkg, oldpkg);
	}

cleanup:
	_alam_pkg_free(oldpkg);
	return(ret);
}

int _alam_upgrade_packages(amtrans_t *trans, amdb_t *db)
{
	int pkg_count, pkg_current;
	alam_list_t *targ;

	ALAM_LOG_FUNC;

	ASSERT(trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));

	if(trans->add == NULL) {
		return(0);
	}

	pkg_count = alam_list_count(trans->add);
	pkg_current = 1;

	/* loop through our package list adding/upgrading one at a time */
	for(targ = trans->add; targ; targ = targ->next) {
		if(handle->trans->state == STATE_INTERRUPTED) {
			return(0);
		}

		ampkg_t *newpkg = (ampkg_t *)targ->data;
		commit_single_pkg(newpkg, pkg_current, pkg_count, trans, db);
		pkg_current++;
	}

	/* run ldconfig if it exists */
	_alam_ldconfig(handle->root);

	return(0);
}

/* vim: set ts=2 sw=2 noet: */
