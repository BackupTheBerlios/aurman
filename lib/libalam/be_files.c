/*
 *  be_files.c
 *
 *  Copyright (c) 2006 by Christian Hamar <krics@linuxforum.hu>
 *  Copyright (c) 2006 by Miklos Vajna <vmiklos@frugalware.org>
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h> /* uintmax_t, intmax_t */
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include <limits.h> /* PATH_MAX */
#include <locale.h> /* setlocale */

/* libalam */
#include "db.h"
#include "alam_list.h"
#include "cache.h"
#include "log.h"
#include "util.h"
#include "alam.h"
#include "handle.h"
#include "package.h"
#include "delta.h"
#include "deps.h"
#include "dload.h"


/*
 * Return the last update time as number of seconds from the epoch.
 * Returns 0 if the value is unknown or can't be read.
 */
static time_t getlastupdate(const amdb_t *db)
{
	FILE *fp;
	char *file;
	time_t ret = 0;

	ALAM_LOG_FUNC;

	if(db == NULL) {
		return(ret);
	}

	/* db->path + '.lastupdate' + NULL */
	MALLOC(file, strlen(db->path) + 12, RET_ERR(AM_ERR_MEMORY, ret));
	sprintf(file, "%s.lastupdate", db->path);

	/* get the last update time, if it's there */
	if((fp = fopen(file, "r")) == NULL) {
		free(file);
		return(ret);
	} else {
		char line[64];
		if(fgets(line, sizeof(line), fp)) {
			ret = atol(line);
		}
	}
	fclose(fp);
	free(file);
	return(ret);
}

/*
 * writes the dbpath/.lastupdate file with the value in time
 */
static int setlastupdate(const amdb_t *db, time_t time)
{
	FILE *fp;
	char *file;
	int ret = 0;

	ALAM_LOG_FUNC;

	if(db == NULL || time == 0) {
		return(-1);
	}

	/* db->path + '.lastupdate' + NULL */
	MALLOC(file, strlen(db->path) + 12, RET_ERR(AM_ERR_MEMORY, ret));
	sprintf(file, "%s.lastupdate", db->path);

	if((fp = fopen(file, "w")) == NULL) {
		free(file);
		return(-1);
	}
	if(fprintf(fp, "%ju", (uintmax_t)time) <= 0) {
		ret = -1;
	}
	fclose(fp);
	free(file);
	return(ret);
}

static int checkdbdir(amdb_t *db)
{
	struct stat buf;
	char *path = db->path;

	if(stat(path, &buf) != 0) {
		_alam_log(AM_LOG_DEBUG, "database dir '%s' does not exist, creating it\n",
				path);
		if(alam_makepath(path) != 0) {
			RET_ERR(AM_ERR_SYSTEM, -1);
		}
	} else if(!S_ISDIR(buf.st_mode)) {
		_alam_log(AM_LOG_WARNING, "removing bogus database: %s\n", path);
		if(unlink(path) != 0 || alam_makepath(path) != 0) {
			RET_ERR(AM_ERR_SYSTEM, -1);
		}
	}
	return(0);
}

/** Update a package database
 *
 * An update of the package database \a db will be attempted. Unless
 * \a force is true, the update will only be performed if the remote
 * database was modified since the last update.
 *
 * A transaction is necessary for this operation, in order to obtain a
 * database lock. During this transaction the front-end will be informed
 * of the download progress of the database via the download callback.
 *
 * Example:
 * @code
 * amdb_t *db;
 * int result;
 * db = alam_list_getdata(alam_option_get_syncdbs());
 * if(alam_trans_init(0, NULL, NULL, NULL) == 0) {
 *     result = alam_db_update(0, db);
 *     alam_trans_release();
 *
 *     if(result > 0) {
 *	       printf("Unable to update database: %s\n", alam_strerrorlast());
 *     } else if(result < 0) {
 *         printf("Database already up to date\n");
 *     } else {
 *         printf("Database updated\n");
 *     }
 * }
 * @endcode
 *
 * @ingroup alam_databases
 * @note After a successful update, the \link alam_db_get_pkgcache()
 * package cache \endlink will be invalidated
 * @param force if true, then forces the update, otherwise update only in case
 * the database isn't up to date
 * @param db pointer to the package database to update
 * @return 0 on success, > 0 on error (am_errno is set accordingly), < 0 if up
 * to date
 */
int SYMEXPORT alam_db_update(int force, amdb_t *db)
{
	char *dbfile, *dbfilepath;
	time_t newmtime = 0, lastupdate = 0;
	const char *dbpath;
	size_t len;

	int ret;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(handle != NULL, RET_ERR(AM_ERR_HANDLE_NULL, -1));
	ASSERT(db != NULL && db != handle->db_local, RET_ERR(AM_ERR_WRONG_ARGS, -1));
	/* Verify we are in a transaction.  This is done _mainly_ because we need a DB
	 * lock - if we update without a db lock, we may kludge some other pacman
	 * process that _has_ a lock.
	 */
	ASSERT(handle->trans != NULL, RET_ERR(AM_ERR_TRANS_NULL, -1));
	ASSERT(handle->trans->state == STATE_INITIALIZED, RET_ERR(AM_ERR_TRANS_NOT_INITIALIZED, -1));

	if(!alam_list_find_ptr(handle->dbs_sync, db)) {
		RET_ERR(AM_ERR_DB_NOT_FOUND, -1);
	}

	if(!force) {
		/* get the lastupdate time */
		lastupdate = getlastupdate(db);
		if(lastupdate == 0) {
			_alam_log(AM_LOG_DEBUG, "failed to get lastupdate time for %s\n",
					db->treename);
		}
	}

	len = strlen(db->treename) + strlen(DBEXT) + 1;
	MALLOC(dbfile, len, RET_ERR(AM_ERR_MEMORY, -1));
	sprintf(dbfile, "%s" DBEXT, db->treename);

	dbpath = alam_option_get_dbpath();

	ret = _alam_download_single_file(dbfile, db->servers, dbpath,
			lastupdate, &newmtime);
	free(dbfile);

	if(ret == 1) {
		/* mtimes match, do nothing */
		am_errno = 0;
		return(1);
	} else if(ret == -1) {
		/* am_errno was set by the download code */
		_alam_log(AM_LOG_DEBUG, "failed to sync db: %s\n", alam_strerrorlast());
		return(-1);
	} else {
		/* remove the old dir */
		if(_alam_rmrf(db->path) != 0) {
			_alam_log(AM_LOG_ERROR, _("could not remove database %s\n"), db->treename);
			RET_ERR(AM_ERR_DB_REMOVE, -1);
		}

		/* Cache needs to be rebuilt */
		_alam_db_free_pkgcache(db);

		/* form the path to the db location */
		len = strlen(dbpath) + strlen(db->treename) + strlen(DBEXT) + 1;
		MALLOC(dbfilepath, len, RET_ERR(AM_ERR_MEMORY, -1));
		sprintf(dbfilepath, "%s%s" DBEXT, dbpath, db->treename);

		/* uncompress the sync database */
		checkdbdir(db);
		ret = alam_unpack(dbfilepath, db->path, NULL);
		if(ret) {
			free(dbfilepath);
			RET_ERR(AM_ERR_SYSTEM, -1);
		}
		unlink(dbfilepath);
		free(dbfilepath);

		/* if we have a new mtime, set the DB last update value */
		if(newmtime) {
			_alam_log(AM_LOG_DEBUG, "sync: new mtime for %s: %ju\n",
					db->treename, (uintmax_t)newmtime);
			setlastupdate(db, newmtime);
		}
	}

	return(0);
}


static int splitname(const char *target, ampkg_t *pkg)
{
	/* the format of a db entry is as follows:
	 *    package-version-rel/
	 * package name can contain hyphens, so parse from the back- go back
	 * two hyphens and we have split the version from the name.
	 */
	char *tmp, *p, *q;

	if(target == NULL || pkg == NULL) {
		return(-1);
	}
	STRDUP(tmp, target, RET_ERR(AM_ERR_MEMORY, -1));
	p = tmp + strlen(tmp);

	/* do the magic parsing- find the beginning of the version string
	 * by doing two iterations of same loop to lop off two hyphens */
	for(q = --p; *q && *q != '-'; q--);
	for(p = --q; *p && *p != '-'; p--);
	if(*p != '-' || p == tmp) {
		return(-1);
	}

	/* copy into fields and return */
	if(pkg->version) {
		FREE(pkg->version);
	}
	STRDUP(pkg->version, p+1, RET_ERR(AM_ERR_MEMORY, -1));
	/* insert a terminator at the end of the name (on hyphen)- then copy it */
	*p = '\0';
	if(pkg->name) {
		FREE(pkg->name);
	}
	STRDUP(pkg->name, tmp, RET_ERR(AM_ERR_MEMORY, -1));

	free(tmp);
	return(0);
}

int _alam_db_populate(amdb_t *db)
{
	int count = 0;
	struct dirent *ent = NULL;
	struct stat sbuf;
	char path[PATH_MAX];
	DIR *dbdir;

	ALAM_LOG_FUNC;

	ASSERT(db != NULL, RET_ERR(AM_ERR_DB_NULL, -1));

	dbdir = opendir(db->path);
	if(dbdir == NULL) {
		return(0);
	}
	while((ent = readdir(dbdir)) != NULL) {
		const char *name = ent->d_name;
		ampkg_t *pkg;

		if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
			continue;
		}
		/* stat the entry, make sure it's a directory */
		snprintf(path, PATH_MAX, "%s%s", db->path, name);
		if(stat(path, &sbuf) != 0 || !S_ISDIR(sbuf.st_mode)) {
			continue;
		}

		pkg = _alam_pkg_new();
		if(pkg == NULL) {
			closedir(dbdir);
			return(-1);
		}
		/* split the db entry name */
		if(splitname(name, pkg) != 0) {
			_alam_log(AM_LOG_ERROR, _("invalid name for database entry '%s'\n"),
					name);
			_alam_pkg_free(pkg);
			continue;
		}

		/* explicitly read with only 'BASE' data, accessors will handle the rest */
		if(_alam_db_read(db, pkg, INFRQ_BASE) == -1) {
			_alam_log(AM_LOG_ERROR, _("corrupted database entry '%s'\n"), name);
			_alam_pkg_free(pkg);
			continue;
		}
		pkg->origin = PKG_FROM_CACHE;
		pkg->origin_data.db = db;
		/* add to the collection */
		_alam_log(AM_LOG_FUNCTION, "adding '%s' to package cache for db '%s'\n",
				pkg->name, db->treename);
		db->pkgcache = alam_list_add(db->pkgcache, pkg);
		count++;
	}

	closedir(dbdir);
	db->pkgcache = alam_list_msort(db->pkgcache, count, _alam_pkg_cmp);
	return(count);
}

/* Note: the return value must be freed by the caller */
static char *get_pkgpath(amdb_t *db, ampkg_t *info)
{
	size_t len;
	char *pkgpath;

	len = strlen(db->path) + strlen(info->name) + strlen(info->version) + 3;
	MALLOC(pkgpath, len, RET_ERR(AM_ERR_MEMORY, NULL));
	sprintf(pkgpath, "%s%s-%s/", db->path, info->name, info->version);
	return(pkgpath);
}

int _alam_db_read(amdb_t *db, ampkg_t *info, amdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	char line[513];
	char *pkgpath = NULL;

	ALAM_LOG_FUNC;

	if(db == NULL) {
		RET_ERR(AM_ERR_DB_NULL, -1);
	}

	if(info == NULL || info->name == NULL || info->version == NULL) {
		_alam_log(AM_LOG_DEBUG, "invalid package entry provided to _alam_db_read, skipping\n");
		return(-1);
	}

	if(info->origin == PKG_FROM_FILE) {
		_alam_log(AM_LOG_DEBUG, "request to read database info for a file-based package '%s', skipping...\n", info->name);
		return(-1);
	}

	/* bitmask logic here:
	 * infolevel: 00001111
	 * inforeq:   00010100
	 * & result:  00000100
	 * == to inforeq? nope, we need to load more info. */
	if((info->infolevel & inforeq) == inforeq) {
		/* already loaded this info, do nothing */
		return(0);
	}
	_alam_log(AM_LOG_FUNCTION, "loading package data for %s : level=0x%x\n",
			info->name, inforeq);

	/* clear out 'line', to be certain - and to make valgrind happy */
	memset(line, 0, 513);

	pkgpath = get_pkgpath(db, info);

	if(access(pkgpath, F_OK)) {
		/* directory doesn't exist or can't be opened */
		_alam_log(AM_LOG_DEBUG, "cannot find '%s-%s' in db '%s'\n",
				info->name, info->version, db->treename);
		goto error;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		snprintf(path, PATH_MAX, "%sdesc", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			if(fgets(line, 256, fp) == NULL) {
				break;
			}
			_alam_strtrim(line);
			if(strcmp(line, "%NAME%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				if(strcmp(_alam_strtrim(line), info->name) != 0) {
					_alam_log(AM_LOG_ERROR, _("%s database is inconsistent: name "
								"mismatch on package %s\n"), db->treename, info->name);
				}
			} else if(strcmp(line, "%VERSION%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				if(strcmp(_alam_strtrim(line), info->version) != 0) {
					_alam_log(AM_LOG_ERROR, _("%s database is inconsistent: version "
								"mismatch on package %s\n"), db->treename, info->name);
				}
			} else if(strcmp(line, "%FILENAME%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->filename, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%DESC%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->desc, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%GROUPS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->groups = alam_list_add(info->groups, linedup);
				}
			} else if(strcmp(line, "%URL%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->url, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%LICENSE%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->licenses = alam_list_add(info->licenses, linedup);
				}
			} else if(strcmp(line, "%ARCH%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->arch, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%BUILDDATE%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				_alam_strtrim(line);

				char first = tolower(line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; /* initialize to null in case of failure */
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->builddate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->builddate = atol(line);
				}
			} else if(strcmp(line, "%INSTALLDATE%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				_alam_strtrim(line);

				char first = tolower(line[0]);
				if(first > 'a' && first < 'z') {
					struct tm tmp_tm = {0}; /* initialize to null in case of failure */
					setlocale(LC_TIME, "C");
					strptime(line, "%a %b %e %H:%M:%S %Y", &tmp_tm);
					info->installdate = mktime(&tmp_tm);
					setlocale(LC_TIME, "");
				} else {
					info->installdate = atol(line);
				}
			} else if(strcmp(line, "%PACKAGER%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->packager, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%REASON%") == 0) {
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->reason = (ampkgreason_t)atol(_alam_strtrim(line));
			} else if(strcmp(line, "%SIZE%") == 0 || strcmp(line, "%CSIZE%") == 0) {
				/* NOTE: the CSIZE and SIZE fields both share the "size" field
				 *       in the pkginfo_t struct.  This can be done b/c CSIZE
				 *       is currently only used in sync databases, and SIZE is
				 *       only used in local databases.
				 */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->size = atol(_alam_strtrim(line));
				/* also store this value to isize if isize is unset */
				if(info->isize == 0) {
					info->isize = info->size;
				}
			} else if(strcmp(line, "%ISIZE%") == 0) {
				/* ISIZE (installed size) tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				info->isize = atol(_alam_strtrim(line));
			} else if(strcmp(line, "%MD5SUM%") == 0) {
				/* MD5SUM tag only appears in sync repositories,
				 * not the local one. */
				if(fgets(line, 512, fp) == NULL) {
					goto error;
				}
				STRDUP(info->md5sum, _alam_strtrim(line), goto error);
			} else if(strcmp(line, "%REPLACES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->replaces = alam_list_add(info->replaces, linedup);
				}
			} else if(strcmp(line, "%FORCE%") == 0) {
				info->force = 1;
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(inforeq & INFRQ_FILES) {
		snprintf(path, PATH_MAX, "%sfiles", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(fgets(line, 256, fp)) {
			_alam_strtrim(line);
			if(strcmp(line, "%FILES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->files = alam_list_add(info->files, linedup);
				}
			} else if(strcmp(line, "%BACKUP%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->backup = alam_list_add(info->backup, linedup);
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		snprintf(path, PATH_MAX, "%sdepends", pkgpath);
		if((fp = fopen(path, "r")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			goto error;
		}
		while(!feof(fp)) {
			fgets(line, 255, fp);
			_alam_strtrim(line);
			if(strcmp(line, "%DEPENDS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					amdepend_t *dep = _alam_splitdep(_alam_strtrim(line));
					info->depends = alam_list_add(info->depends, dep);
				}
			} else if(strcmp(line, "%OPTDEPENDS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->optdepends = alam_list_add(info->optdepends, linedup);
				}
			} else if(strcmp(line, "%CONFLICTS%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->conflicts = alam_list_add(info->conflicts, linedup);
				}
			} else if(strcmp(line, "%PROVIDES%") == 0) {
				while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
					char *linedup;
					STRDUP(linedup, _alam_strtrim(line), goto error);
					info->provides = alam_list_add(info->provides, linedup);
				}
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* DELTAS */
	if(inforeq & INFRQ_DELTAS) {
		snprintf(path, PATH_MAX, "%sdeltas", pkgpath);
		if((fp = fopen(path, "r"))) {
			while(!feof(fp)) {
				fgets(line, 255, fp);
				_alam_strtrim(line);
				if(strcmp(line, "%DELTAS%") == 0) {
					while(fgets(line, 512, fp) && strlen(_alam_strtrim(line))) {
						amdelta_t *delta = _alam_delta_parse(line);
						if(delta) {
							info->deltas = alam_list_add(info->deltas, delta);
						}
					}
				}
			}
			fclose(fp);
			fp = NULL;
		}
	}

	/* INSTALL */
	if(inforeq & INFRQ_SCRIPTLET) {
		snprintf(path, PATH_MAX, "%sinstall", pkgpath);
		if(access(path, F_OK) == 0) {
			info->scriptlet = 1;
		}
	}

	/* internal */
	info->infolevel |= inforeq;

	free(pkgpath);
	return(0);

error:
	free(pkgpath);
	if(fp) {
		fclose(fp);
	}
	return(-1);
}

int _alam_db_prepare(amdb_t *db, ampkg_t *info)
{
	mode_t oldmask;
	int retval = 0;
	char *pkgpath = NULL;

	if(checkdbdir(db) != 0) {
		return(-1);
	}

	oldmask = umask(0000);
	pkgpath = get_pkgpath(db, info);

	if((retval = mkdir(pkgpath, 0755)) != 0) {
		_alam_log(AM_LOG_ERROR, _("could not create directory %s: %s\n"),
				pkgpath, strerror(errno));
	}

	free(pkgpath);
	umask(oldmask);

	return(retval);
}

int _alam_db_write(amdb_t *db, ampkg_t *info, amdbinfrq_t inforeq)
{
	FILE *fp = NULL;
	char path[PATH_MAX];
	mode_t oldmask;
	alam_list_t *lp = NULL;
	int retval = 0;
	int local = 0;
	char *pkgpath = NULL;

	ALAM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		return(-1);
	}

	pkgpath = get_pkgpath(db, info);

	/* make sure we have a sane umask */
	oldmask = umask(0022);

	if(strcmp(db->treename, "local") == 0) {
		local = 1;
	}

	/* DESC */
	if(inforeq & INFRQ_DESC) {
		_alam_log(AM_LOG_DEBUG, "writing %s-%s DESC information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%sdesc", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		fprintf(fp, "%%NAME%%\n%s\n\n"
						"%%VERSION%%\n%s\n\n", info->name, info->version);
		if(info->desc) {
			fprintf(fp, "%%DESC%%\n"
							"%s\n\n", info->desc);
		}
		if(info->groups) {
			fputs("%GROUPS%\n", fp);
			for(lp = info->groups; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->replaces) {
			fputs("%REPLACES%\n", fp);
			for(lp = info->replaces; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->force) {
			fprintf(fp, "%%FORCE%%\n\n");
		}
		if(local) {
			if(info->url) {
				fprintf(fp, "%%URL%%\n"
								"%s\n\n", info->url);
			}
			if(info->licenses) {
				fputs("%LICENSE%\n", fp);
				for(lp = info->licenses; lp; lp = lp->next) {
					fprintf(fp, "%s\n", (char *)lp->data);
				}
				fprintf(fp, "\n");
			}
			if(info->arch) {
				fprintf(fp, "%%ARCH%%\n"
								"%s\n\n", info->arch);
			}
			if(info->builddate) {
				fprintf(fp, "%%BUILDDATE%%\n"
								"%ju\n\n", (uintmax_t)info->builddate);
			}
			if(info->installdate) {
				fprintf(fp, "%%INSTALLDATE%%\n"
								"%ju\n\n", (uintmax_t)info->installdate);
			}
			if(info->packager) {
				fprintf(fp, "%%PACKAGER%%\n"
								"%s\n\n", info->packager);
			}
			if(info->isize) {
				/* only write installed size, csize is irrelevant once installed */
				fprintf(fp, "%%SIZE%%\n"
								"%jd\n\n", (intmax_t)info->isize);
			}
			if(info->reason) {
				fprintf(fp, "%%REASON%%\n"
								"%u\n\n", info->reason);
			}
		} else {
			if(info->size) {
				fprintf(fp, "%%CSIZE%%\n"
								"%jd\n\n", (intmax_t)info->size);
			}
			if(info->isize) {
				fprintf(fp, "%%ISIZE%%\n"
								"%jd\n\n", (intmax_t)info->isize);
			}
			if(info->md5sum) {
				fprintf(fp, "%%MD5SUM%%\n"
								"%s\n\n", info->md5sum);
			}
		}
		fclose(fp);
		fp = NULL;
	}

	/* FILES */
	if(local && (inforeq & INFRQ_FILES)) {
		_alam_log(AM_LOG_DEBUG, "writing %s-%s FILES information back to db\n",
				info->name, info->version);
		snprintf(path, PATH_MAX, "%sfiles", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		if(info->files) {
			fprintf(fp, "%%FILES%%\n");
			for(lp = info->files; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->backup) {
			fprintf(fp, "%%BACKUP%%\n");
			for(lp = info->backup; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
		fp = NULL;
	}

	/* DEPENDS */
	if(inforeq & INFRQ_DEPENDS) {
		_alam_log(AM_LOG_DEBUG, "writing %s-%s DEPENDS information back to db\n",
			info->name, info->version);
		snprintf(path, PATH_MAX, "%sdepends", pkgpath);
		if((fp = fopen(path, "w")) == NULL) {
			_alam_log(AM_LOG_ERROR, _("could not open file %s: %s\n"), path, strerror(errno));
			retval = -1;
			goto cleanup;
		}
		if(info->depends) {
			fputs("%DEPENDS%\n", fp);
			for(lp = info->depends; lp; lp = lp->next) {
				char *depstring = alam_dep_compute_string(lp->data);
				fprintf(fp, "%s\n", depstring);
				free(depstring);
			}
			fprintf(fp, "\n");
		}
		if(info->optdepends) {
			fputs("%OPTDEPENDS%\n", fp);
			for(lp = info->optdepends; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->conflicts) {
			fputs("%CONFLICTS%\n", fp);
			for(lp = info->conflicts; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		if(info->provides) {
			fputs("%PROVIDES%\n", fp);
			for(lp = info->provides; lp; lp = lp->next) {
				fprintf(fp, "%s\n", (char *)lp->data);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
		fp = NULL;
	}

	/* INSTALL */
	/* nothing needed here (script is automatically extracted) */

cleanup:
	umask(oldmask);
	free(pkgpath);

	if(fp) {
		fclose(fp);
	}

	return(retval);
}

int _alam_db_remove(amdb_t *db, ampkg_t *info)
{
	int ret = 0;
	char *pkgpath = NULL;

	ALAM_LOG_FUNC;

	if(db == NULL || info == NULL) {
		RET_ERR(AM_ERR_DB_NULL, -1);
	}

	pkgpath = get_pkgpath(db, info);

	ret = _alam_rmrf(pkgpath);
	free(pkgpath);
	if(ret != 0) {
		ret = -1;
	}
	return(ret);
}

/* vim: set ts=2 sw=2 noet: */
