/*
 *  util.h
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
#ifndef _ALAM_UTIL_H
#define _ALAM_UTIL_H

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h> /* struct stat */
#include <archive.h> /* struct archive */

#ifdef ENABLE_NLS
#include <libintl.h> /* here so it doesn't need to be included elsewhere */
/* define _() as shortcut for gettext() */
#define _(str) dgettext ("libalam", str)
#else
#define _(s) s
#endif

#define ALLOC_FAIL(s) do { _alam_log(AM_LOG_ERROR, _("alloc failure: could not allocate %zd bytes\n"), s); } while(0)

#define MALLOC(p, s, action) do { p = calloc(1, s); if(p == NULL) { ALLOC_FAIL(s); action; } } while(0)
#define CALLOC(p, l, s, action) do { p = calloc(l, s); if(p == NULL) { ALLOC_FAIL(s); action; } } while(0)
/* This strdup macro is NULL safe- copying NULL will yield NULL */
#define STRDUP(r, s, action) do { if(s != NULL) { r = strdup(s); if(r == NULL) { ALLOC_FAIL(strlen(s)); action; } } else { r = NULL; } } while(0)

#define FREE(p) do { free(p); p = NULL; } while(0)

#define ASSERT(cond, action) do { if(!(cond)) { action; } } while(0)

#define RET_ERR(err, ret) do { am_errno = (err); \
	_alam_log(AM_LOG_DEBUG, "returning error %d from %s : %s\n", err, __func__, alam_strerrorlast()); \
	return(ret); } while(0)

int _alam_makepath(const char *path);
int _alam_makepath_mode(const char *path, mode_t mode);
int _alam_copyfile(const char *src, const char *dest);
char *_alam_strtrim(char *str);
int _alam_lckmk();
int _alam_lckrm();
int _alam_rmrf(const char *path);
int _alam_logaction(unsigned short usesyslog, FILE *f, const char *fmt, va_list args);
int _alam_run_chroot(const char *root, const char *cmd);
int _alam_ldconfig(const char *root);
int _alam_str_cmp(const void *s1, const void *s2);
char *_alam_filecache_find(const char *filename);
const char *_alam_filecache_setup(void);
int _alam_lstat(const char *path, struct stat *buf);
int _alam_test_md5sum(const char *filepath, const char *md5sum);
char *_alam_archive_fgets(char *line, size_t size, struct archive *a);

#ifndef HAVE_STRSEP
char *strsep(char **, const char *);
#endif

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))
#define SYMHIDDEN __attribute__((visibility("internal")))

/* max percent of package size to download deltas */
#define MAX_DELTA_RATIO 0.7

#endif /* _ALAM_UTIL_H */

/* vim: set ts=2 sw=2 noet: */
