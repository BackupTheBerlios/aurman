/*
 * alam.h
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
#ifndef _ALAM_H
#define _ALAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h> /* for off_t */
#include <time.h> /* for time_t */
#include <stdarg.h> /* for va_list */

#include "alam_list.h"

#define DEPRECATED __attribute__((deprecated))

/*
 * Arch Linux Package Management library
 */

/*
 * Constants
 */
#define AM_USERNAME_MAX 32
	
/*
 * Structures
 */

typedef struct __amdb_t amdb_t;
typedef struct __ampkg_t ampkg_t;
typedef struct __amdelta_t amdelta_t;
typedef struct __amgrp_t amgrp_t;
typedef struct __amtrans_t amtrans_t;
typedef struct __amdepend_t amdepend_t;
typedef struct __amdepmissing_t amdepmissing_t;
typedef struct __amconflict_t amconflict_t;
typedef struct __amfileconflict_t amfileconflict_t;

/*
 * Library
 */

int alam_initialize(void);
int alam_release(void);
const char *alam_version(void);

/*
 * Logging facilities
 */

/* Levels */
typedef enum _amloglevel_t {
	AM_LOG_ERROR    = 0x01,
	AM_LOG_WARNING  = 0x02,
	AM_LOG_DEBUG    = 0x04,
	AM_LOG_FUNCTION = 0x08
} amloglevel_t;

typedef void (*alam_cb_log)(amloglevel_t, char *, va_list);
int alam_logaction(char *fmt, ...);

/*
 * Downloading
 */

typedef void (*alam_cb_download)(const char *filename,
		off_t xfered, off_t total);
typedef void (*alam_cb_totaldl)(off_t total);
/** A callback for downloading files
 * @param url the URL of the file to be downloaded
 * @param localpath the directory to which the file should be downloaded
 * @param mtimeold the modification time of the file previously downloaded
 * @param mtimenew the modification time of the newly downloaded file.
 * This should be set by the callback.
 * @return 0 on success, 1 if the modification times are identical, -1 on
 * error.
 */
typedef int (*alam_cb_fetch)(const char *url, const char *localpath,
		time_t mtimeold, time_t *mtimenew);

/*
 * Options
 */

alam_cb_log alam_option_get_logcb();
void alam_option_set_logcb(alam_cb_log cb);

alam_cb_download alam_option_get_dlcb();
void alam_option_set_dlcb(alam_cb_download cb);

alam_cb_fetch alam_option_get_fetchcb();
void alam_option_set_fetchcb(alam_cb_fetch cb);

alam_cb_totaldl alam_option_get_totaldlcb();
void alam_option_set_totaldlcb(alam_cb_totaldl cb);

const char *alam_option_get_root();
int alam_option_set_root(const char *root);

const char *alam_option_get_dbpath();
int alam_option_set_dbpath(const char *dbpath);

alam_list_t *alam_option_get_cachedirs();
int alam_option_add_cachedir(const char *cachedir);
void alam_option_set_cachedirs(alam_list_t *cachedirs);
int alam_option_remove_cachedir(const char *cachedir);

const char *alam_option_get_logfile();
int alam_option_set_logfile(const char *logfile);

const char *alam_option_get_lockfile();
/* no set_lockfile, path is determined from dbpath */

unsigned short alam_option_get_usesyslog();
void alam_option_set_usesyslog(unsigned short usesyslog);

alam_list_t *alam_option_get_noupgrades();
void alam_option_add_noupgrade(const char *pkg);
void alam_option_set_noupgrades(alam_list_t *noupgrade);
int alam_option_remove_noupgrade(const char *pkg);

alam_list_t *alam_option_get_noextracts();
void alam_option_add_noextract(const char *pkg);
void alam_option_set_noextracts(alam_list_t *noextract);
int alam_option_remove_noextract(const char *pkg);

alam_list_t *alam_option_get_ignorepkgs();
void alam_option_add_ignorepkg(const char *pkg);
void alam_option_set_ignorepkgs(alam_list_t *ignorepkgs);
int alam_option_remove_ignorepkg(const char *pkg);

alam_list_t *alam_option_get_ignoregrps();
void alam_option_add_ignoregrp(const char *grp);
void alam_option_set_ignoregrps(alam_list_t *ignoregrps);
int alam_option_remove_ignoregrp(const char *grp);

const char *alam_option_get_arch();
void alam_option_set_arch(const char *arch);

void alam_option_set_usedelta(unsigned short usedelta);

amdb_t *alam_option_get_localdb();
alam_list_t *alam_option_get_syncdbs();

/*
 * Databases
 */

/* Preferred interfaces db_register_local and db_register_sync */
amdb_t *alam_db_register_local(void);
amdb_t *alam_db_register_sync(const char *treename);
int alam_db_unregister(amdb_t *db);
int alam_db_unregister_all(void);

const char *alam_db_get_name(const amdb_t *db);
const char *alam_db_get_url(const amdb_t *db);

int alam_db_setserver(amdb_t *db, const char *url);

int alam_db_update(int level, amdb_t *db);

ampkg_t *alam_db_get_pkg(amdb_t *db, const char *name);
alam_list_t *alam_db_get_pkgcache(amdb_t *db);

amgrp_t *alam_db_readgrp(amdb_t *db, const char *name);
alam_list_t *alam_db_get_grpcache(amdb_t *db);
alam_list_t *alam_db_search(amdb_t *db, const alam_list_t* needles);

/*
 * Packages
 */

/* Info parameters */

/* reasons -- ie, why the package was installed */
typedef enum _ampkgreason_t {
	AM_PKG_REASON_EXPLICIT = 0,  /* explicitly requested by the user */
	AM_PKG_REASON_DEPEND = 1  /* installed as a dependency for another package */
} ampkgreason_t;

int alam_pkg_load(const char *filename, unsigned short full, ampkg_t **pkg);
int alam_pkg_free(ampkg_t *pkg);
int alam_pkg_checkmd5sum(ampkg_t *pkg);
char *alam_fetch_pkgurl(const char *url);
int alam_pkg_vercmp(const char *a, const char *b);
alam_list_t *alam_pkg_compute_requiredby(ampkg_t *pkg);

const char *alam_pkg_get_filename(ampkg_t *pkg);
const char *alam_pkg_get_name(ampkg_t *pkg);
const char *alam_pkg_get_version(ampkg_t *pkg);
const char *alam_pkg_get_desc(ampkg_t *pkg);
const char *alam_pkg_get_url(ampkg_t *pkg);
time_t alam_pkg_get_builddate(ampkg_t *pkg);
time_t alam_pkg_get_installdate(ampkg_t *pkg);
const char *alam_pkg_get_packager(ampkg_t *pkg);
const char *alam_pkg_get_md5sum(ampkg_t *pkg);
const char *alam_pkg_get_arch(ampkg_t *pkg);
off_t alam_pkg_get_size(ampkg_t *pkg);
off_t alam_pkg_get_isize(ampkg_t *pkg);
ampkgreason_t alam_pkg_get_reason(ampkg_t *pkg);
alam_list_t *alam_pkg_get_licenses(ampkg_t *pkg);
alam_list_t *alam_pkg_get_groups(ampkg_t *pkg);
alam_list_t *alam_pkg_get_depends(ampkg_t *pkg);
alam_list_t *alam_pkg_get_optdepends(ampkg_t *pkg);
alam_list_t *alam_pkg_get_conflicts(ampkg_t *pkg);
alam_list_t *alam_pkg_get_provides(ampkg_t *pkg);
alam_list_t *alam_pkg_get_deltas(ampkg_t *pkg);
alam_list_t *alam_pkg_get_replaces(ampkg_t *pkg);
alam_list_t *alam_pkg_get_files(ampkg_t *pkg);
alam_list_t *alam_pkg_get_backup(ampkg_t *pkg);
amdb_t *alam_pkg_get_db(ampkg_t *pkg);
void *alam_pkg_changelog_open(ampkg_t *pkg);
size_t alam_pkg_changelog_read(void *ptr, size_t size,
		const ampkg_t *pkg, const void *fp);
int alam_pkg_changelog_feof(const ampkg_t *pkg, void *fp);
int alam_pkg_changelog_close(const ampkg_t *pkg, void *fp);
unsigned short alam_pkg_has_scriptlet(ampkg_t *pkg);
unsigned short alam_pkg_has_force(ampkg_t *pkg);

off_t alam_pkg_download_size(ampkg_t *newpkg);

/*
 * Deltas
 */

const char *alam_delta_get_from(amdelta_t *delta);
const char *alam_delta_get_to(amdelta_t *delta);
const char *alam_delta_get_filename(amdelta_t *delta);
const char *alam_delta_get_md5sum(amdelta_t *delta);
off_t alam_delta_get_size(amdelta_t *delta);

/*
 * Groups
 */
const char *alam_grp_get_name(const amgrp_t *grp);
alam_list_t *alam_grp_get_pkgs(const amgrp_t *grp);

/*
 * Sync
 */

ampkg_t *alam_sync_newversion(ampkg_t *pkg, alam_list_t *dbs_sync);

/*
 * Transactions
 */


/* Flags */
typedef enum _amtransflag_t {
	AM_TRANS_FLAG_NODEPS = 0x01,
	AM_TRANS_FLAG_FORCE = 0x02,
	AM_TRANS_FLAG_NOSAVE = 0x04,
	/* 0x08 flag can go here */
	AM_TRANS_FLAG_CASCADE = 0x10,
	AM_TRANS_FLAG_RECURSE = 0x20,
	AM_TRANS_FLAG_DBONLY = 0x40,
	/* 0x80 flag can go here */
	AM_TRANS_FLAG_ALLDEPS = 0x100,
	AM_TRANS_FLAG_DOWNLOADONLY = 0x200,
	AM_TRANS_FLAG_NOSCRIPTLET = 0x400,
	AM_TRANS_FLAG_NOCONFLICTS = 0x800,
	/* 0x1000 flag can go here */
	AM_TRANS_FLAG_NEEDED = 0x2000,
	AM_TRANS_FLAG_ALLEXPLICIT = 0x4000,
	AM_TRANS_FLAG_UNNEEDED = 0x8000,
	AM_TRANS_FLAG_RECURSEALL = 0x10000,
	AM_TRANS_FLAG_NOLOCK = 0x20000
} amtransflag_t;

/**
 * @addtogroup alam_trans
 * @{
 */
/**
 * @brief Transaction events.
 * NULL parameters are passed to in all events unless specified otherwise.
 */
typedef enum _amtransevt_t {
	/** Dependencies will be computed for a package. */
	AM_TRANS_EVT_CHECKDEPS_START = 1,
	/** Dependencies were computed for a package. */
	AM_TRANS_EVT_CHECKDEPS_DONE,
	/** File conflicts will be computed for a package. */
	AM_TRANS_EVT_FILECONFLICTS_START,
	/** File conflicts were computed for a package. */
	AM_TRANS_EVT_FILECONFLICTS_DONE,
	/** Dependencies will be resolved for target package. */
	AM_TRANS_EVT_RESOLVEDEPS_START,
	/** Dependencies were resolved for target package. */
	AM_TRANS_EVT_RESOLVEDEPS_DONE,
	/** Inter-conflicts will be checked for target package. */
	AM_TRANS_EVT_INTERCONFLICTS_START,
	/** Inter-conflicts were checked for target package. */
	AM_TRANS_EVT_INTERCONFLICTS_DONE,
	/** Package will be installed.
	 * A pointer to the target package is passed to the callback.
	 */
	AM_TRANS_EVT_ADD_START,
	/** Package was installed.
	 * A pointer to the new package is passed to the callback.
	 */
	AM_TRANS_EVT_ADD_DONE,
	/** Package will be removed.
	 * A pointer to the target package is passed to the callback.
	 */
	AM_TRANS_EVT_REMOVE_START,
	/** Package was removed.
	 * A pointer to the removed package is passed to the callback.
	 */
	AM_TRANS_EVT_REMOVE_DONE,
	/** Package will be upgraded.
	 * A pointer to the upgraded package is passed to the callback.
	 */
	AM_TRANS_EVT_UPGRADE_START,
	/** Package was upgraded.
	 * A pointer to the new package, and a pointer to the old package is passed
	 * to the callback, respectively.
	 */
	AM_TRANS_EVT_UPGRADE_DONE,
	/** Target package's integrity will be checked. */
	AM_TRANS_EVT_INTEGRITY_START,
	/** Target package's integrity was checked. */
	AM_TRANS_EVT_INTEGRITY_DONE,
	/** Target deltas's integrity will be checked. */
	AM_TRANS_EVT_DELTA_INTEGRITY_START,
	/** Target delta's integrity was checked. */
	AM_TRANS_EVT_DELTA_INTEGRITY_DONE,
	/** Deltas will be applied to packages. */
	AM_TRANS_EVT_DELTA_PATCHES_START,
	/** Deltas were applied to packages. */
	AM_TRANS_EVT_DELTA_PATCHES_DONE,
	/** Delta patch will be applied to target package.
	 * The filename of the package and the filename of the patch is passed to the
	 * callback.
	 */
	AM_TRANS_EVT_DELTA_PATCH_START,
	/** Delta patch was applied to target package. */
	AM_TRANS_EVT_DELTA_PATCH_DONE,
	/** Delta patch failed to apply to target package. */
	AM_TRANS_EVT_DELTA_PATCH_FAILED,
	/** Scriptlet has printed information.
	 * A line of text is passed to the callback.
	 */
	AM_TRANS_EVT_SCRIPTLET_INFO,
	/** Files will be downloaded from a repository.
	 * The repository's tree name is passed to the callback.
	 */
	AM_TRANS_EVT_RETRIEVE_START,
} amtransevt_t;
/*@}*/

/* Transaction Conversations (ie, questions) */
typedef enum _amtransconv_t {
	AM_TRANS_CONV_INSTALL_IGNOREPKG = 0x01,
	AM_TRANS_CONV_REPLACE_PKG = 0x02,
	AM_TRANS_CONV_CONFLICT_PKG = 0x04,
	AM_TRANS_CONV_CORRUPTED_PKG = 0x08,
	AM_TRANS_CONV_LOCAL_NEWER = 0x10,
	AM_TRANS_CONV_REMOVE_PKGS = 0x20,
} amtransconv_t;

/* Transaction Progress */
typedef enum _amtransprog_t {
	AM_TRANS_PROGRESS_ADD_START,
	AM_TRANS_PROGRESS_UPGRADE_START,
	AM_TRANS_PROGRESS_REMOVE_START,
	AM_TRANS_PROGRESS_CONFLICTS_START
} amtransprog_t;

/* Transaction Event callback */
typedef void (*alam_trans_cb_event)(amtransevt_t, void *, void *);

/* Transaction Conversation callback */
typedef void (*alam_trans_cb_conv)(amtransconv_t, void *, void *,
																	 void *, int *);

/* Transaction Progress callback */
typedef void (*alam_trans_cb_progress)(amtransprog_t, const char *, int, int, int);

unsigned int alam_trans_get_flags();
alam_list_t * alam_trans_get_add();
alam_list_t * alam_trans_get_remove();
int alam_trans_init(amtransflag_t flags,
										alam_trans_cb_event cb_event, alam_trans_cb_conv conv,
										alam_trans_cb_progress cb_progress);
int alam_trans_prepare(alam_list_t **data);
int alam_trans_commit(alam_list_t **data);
int alam_trans_interrupt(void);
int alam_trans_release(void);

int alam_sync_sysupgrade(int enable_downgrade);
int alam_sync_target(char *target);
int alam_sync_dbtarget(char *db, char *target);
int alam_add_target(char *target);
int alam_remove_target(char *target);

/*
 * Dependencies and conflicts
 */

typedef enum _amdepmod_t {
	AM_DEP_MOD_ANY = 1,
	AM_DEP_MOD_EQ,
	AM_DEP_MOD_GE,
	AM_DEP_MOD_LE,
	AM_DEP_MOD_GT,
	AM_DEP_MOD_LT
} amdepmod_t;

int alam_depcmp(ampkg_t *pkg, amdepend_t *dep);
alam_list_t *alam_checkdeps(alam_list_t *pkglist, int reversedeps,
		alam_list_t *remove, alam_list_t *upgrade);
alam_list_t *alam_deptest(amdb_t *db, alam_list_t *targets);

const char *alam_miss_get_target(const amdepmissing_t *miss);
amdepend_t *alam_miss_get_dep(amdepmissing_t *miss);
const char *alam_miss_get_causingpkg(const amdepmissing_t *miss);

alam_list_t *alam_checkconflicts(alam_list_t *pkglist);

const char *alam_conflict_get_package1(amconflict_t *conflict);
const char *alam_conflict_get_package2(amconflict_t *conflict);
const char *alam_conflict_get_reason(amconflict_t *conflict);

amdepmod_t alam_dep_get_mod(const amdepend_t *dep);
const char *alam_dep_get_name(const amdepend_t *dep);
const char *alam_dep_get_version(const amdepend_t *dep);
char *alam_dep_compute_string(const amdepend_t *dep);

/*
 * File conflicts
 */

typedef enum _amfileconflicttype_t {
	AM_FILECONFLICT_TARGET = 1,
	AM_FILECONFLICT_FILESYSTEM
} amfileconflicttype_t;

const char *alam_fileconflict_get_target(amfileconflict_t *conflict);
amfileconflicttype_t alam_fileconflict_get_type(amfileconflict_t *conflict);
const char *alam_fileconflict_get_file(amfileconflict_t *conflict);
const char *alam_fileconflict_get_ctarget(amfileconflict_t *conflict);

/*
 * Helpers
 */

/* checksums */
char *alam_compute_md5sum(const char *name);

/* compress */
int alam_pack(const char *archive, const char *prefix, const char **fn);
/* uncompress */
int alam_unpack(const char *archive, const char *prefix, const char *fn);

/*
 * Errors
 */
enum _amerrno_t {
	AM_ERR_MEMORY = 1,
	AM_ERR_SYSTEM,
	AM_ERR_BADPERMS,
	AM_ERR_NOT_A_FILE,
	AM_ERR_NOT_A_DIR,
	AM_ERR_WRONG_ARGS,
	/* Interface */
	AM_ERR_HANDLE_NULL,
	AM_ERR_HANDLE_NOT_NULL,
	AM_ERR_HANDLE_LOCK,
	/* Databases */
	AM_ERR_DB_OPEN,
	AM_ERR_DB_CREATE,
	AM_ERR_DB_NULL,
	AM_ERR_DB_NOT_NULL,
	AM_ERR_DB_NOT_FOUND,
	AM_ERR_DB_WRITE,
	AM_ERR_DB_REMOVE,
	/* Servers */
	AM_ERR_SERVER_BAD_URL,
	AM_ERR_SERVER_NONE,
	/* Transactions */
	AM_ERR_TRANS_NOT_NULL,
	AM_ERR_TRANS_NULL,
	AM_ERR_TRANS_DUP_TARGET,
	AM_ERR_TRANS_NOT_INITIALIZED,
	AM_ERR_TRANS_NOT_PREPARED,
	AM_ERR_TRANS_ABORT,
	AM_ERR_TRANS_TYPE,
	AM_ERR_TRANS_NOT_LOCKED,
	/* Packages */
	AM_ERR_PKG_NOT_FOUND,
	AM_ERR_PKG_IGNORED,
	AM_ERR_PKG_INVALID,
	AM_ERR_PKG_OPEN,
	AM_ERR_PKG_CANT_REMOVE,
	AM_ERR_PKG_INVALID_NAME,
	AM_ERR_PKG_INVALID_ARCH,
	AM_ERR_PKG_REPO_NOT_FOUND,
	/* Deltas */
	AM_ERR_DLT_INVALID,
	AM_ERR_DLT_PATCHFAILED,
	/* Dependencies */
	AM_ERR_UNSATISFIED_DEPS,
	AM_ERR_CONFLICTING_DEPS,
	AM_ERR_FILE_CONFLICTS,
	/* Misc */
	AM_ERR_RETRIEVE,
	AM_ERR_INVALID_REGEX,
	/* External library errors */
	AM_ERR_LIBARCHIVE,
	AM_ERR_LIBFETCH,
	AM_ERR_EXTERNAL_DOWNLOAD
};

extern enum _amerrno_t am_errno;

const char *alam_strerror(int err);
const char *alam_strerrorlast(void);

#ifdef __cplusplus
}
#endif
#endif /* _ALAM_H */

/* vim: set ts=2 sw=2 noet: */
