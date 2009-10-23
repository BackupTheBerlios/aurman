/*
 *  util.c
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

#ifdef __linux__
#define	_FILE_OFFSET_BITS 64
#endif

#include <sys/stat.h>
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

/* libarchive */
#include <archive.h>
#include <archive_entry.h>

/* libalam */
#include "util.h"
#include "log.h"
#include "package.h"
#include "alam.h"
#include "alam_list.h"
#include "md5.h"
#include "handle.h"

/* #ifndef HAVE_STRSEP */
/* This is a replacement for strsep which is not portable (missing on Solaris).
 * Copyright (c) 2001 by François Gouget <fgouget_at_codeweavers.com> */
/* char* strsep(char** str, const char* delims) */
/* { */
	/* char* token; */

	/* if (*str==NULL) { */
		/* [> No more tokens <] */
		/* return NULL; */
	/* } */

	/* token=*str; */
	/* while (**str!='\0') { */
		/* if (strchr(delims,**str)!=NULL) { */
			/* **str='\0'; */
			/* (*str)++; */
			/* return token; */
		/* } */
		/* (*str)++; */
	/* } */
	/* [> There is no other token <] */
	/* *str=NULL; */
	/* return token; */
/* } */
/* #endif */

int _alam_makepath(const char *path)
{
	return(alam_makepath_mode(path, 0755));
}

/* does the same thing as 'mkdir -p' */
int SYMEXPORT alam_makepath_mode(const char *path, mode_t mode)
{
	/* A bit of pointer hell here. Descriptions:
	 * orig - a copy of path so we can safely butcher it with strsep
	 * str - the current position in the path string (after the delimiter)
	 * ptr - the original position of str after calling strsep
	 * incr - incrementally generated path for use in stat/mkdir call
	 */
	char *orig, *str, *ptr, *incr;
	mode_t oldmask = umask(0000);
	int ret = 0;

	orig = strdup(path);
	incr = calloc(strlen(orig) + 1, sizeof(char));
	str = orig;
	while((ptr = strsep(&str, "/"))) {
		if(strlen(ptr)) {
			/* we have another path component- append the newest component to
			 * existing string and create one more level of dir structure */
			strcat(incr, "/");
			strcat(incr, ptr);
			if(access(incr, F_OK)) {
				if(mkdir(incr, mode)) {
					ret = 1;
					break;
				}
			}
		}
	}
	free(orig);
	free(incr);
	umask(oldmask);
	return(ret);
}

#define CPBUFSIZE 8 * 1024

int _alam_copyfile(const char *src, const char *dest)
{
	FILE *in, *out;
	size_t len;
	char *buf;
	int ret = 0;

	in = fopen(src, "rb");
	if(in == NULL) {
		return(1);
	}
	out = fopen(dest, "wb");
	if(out == NULL) {
		fclose(in);
		return(1);
	}

	CALLOC(buf, (size_t)CPBUFSIZE, (size_t)1, ret = 1; goto cleanup;);

	/* do the actual file copy */
	while((len = fread(buf, 1, CPBUFSIZE, in))) {
		fwrite(buf, 1, len, out);
	}

	/* chmod dest to permissions of src, as long as it is not a symlink */
	struct stat statbuf;
	if(!stat(src, &statbuf)) {
		if(! S_ISLNK(statbuf.st_mode)) {
			fchmod(fileno(out), statbuf.st_mode);
		}
	} else {
		/* stat was unsuccessful */
		ret = 1;
	}

cleanup:
	fclose(in);
	fclose(out);
	FREE(buf);
	return(ret);
}

/* Trim whitespace and newlines from a string
*/
char *_alam_strtrim(char *str)
{
	char *pch = str;

	if(*str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace((int)*pch)) {
		pch++;
	}
	if(pch != str) {
		memmove(str, pch, (strlen(pch) + 1));
	}

	/* check if there wasn't anything but whitespace in the string. */
	if(*str == '\0') {
		return(str);
	}

	pch = (str + (strlen(str) - 1));
	while(isspace((int)*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
}

/* Create a lock file */
int _alam_lckmk()
{
	int fd;
	pid_t pid;
	char *dir, *ptr, *spid = NULL;
	const char *file = alam_option_get_lockfile();

	/* create the dir of the lockfile first */
	dir = strdup(file);
	ptr = strrchr(dir, '/');
	if(ptr) {
		*ptr = '\0';
	}
	_alam_makepath(dir);
	FREE(dir);

	while((fd = open(file, O_WRONLY | O_CREAT | O_EXCL, 0000)) == -1
			&& errno == EINTR);
	if(fd > 0) {
		pid = getpid();
		size_t len = snprintf(spid, 0, "%ld\n", (long)pid) + 1;
		spid = malloc(len);
		snprintf(spid, len, "%ld\n", (long)pid);
		while(write(fd, (void *)spid, len) == -1 && errno == EINTR);
		fsync(fd);
		free(spid);
		return(fd);
	}
	return(-1);
}

/* Remove a lock file */
int _alam_lckrm()
{
	const char *file = alam_option_get_lockfile();
	if(unlink(file) == -1 && errno != ENOENT) {
		return(-1);
	}
	return(0);
}

//------------------------------------------------------------------
struct alam_pack_data {
        const char *name;
        int fd;
};

//------------------------------------------------------------------
int alam_pack_open(struct archive *a, void *client_data)
{
  struct alam_pack_data *alam_pack_data = client_data;
  alam_pack_data->fd = open(alam_pack_data->name, O_WRONLY | O_CREAT, 0644);
  if (alam_pack_data->fd >= 0)
    return (ARCHIVE_OK);
  else {
    return (ARCHIVE_FATAL);
  }
}

//------------------------------------------------------------------
ssize_t alam_pack_write(struct archive *a, void *client_data, const void *buff, size_t n)
{
  struct alam_pack_data *alam_pack_data = client_data;
  return (write(alam_pack_data->fd, buff, n));
}

//------------------------------------------------------------------
int alam_pack_close(struct archive *a, void *client_data)
{
  struct alam_pack_data *alam_pack_data = client_data;
  if (alam_pack_data->fd > 0)
    close(alam_pack_data->fd);
  return (0);
}

//------------------------------------------------------------------
/* Compression function */
/**
 * @brief pack specific files or all files into an archive.
 *
 * @param archive  		the archive filename to create
 * @param prefix   		where to create the archive
 * @param fn			 		the files to pack into the archive or NULL for all
 * @param compress_t 	The resulting archive will be compressed as specified(gzip,
 * bzip2, etc).
 * @param format_t 		The resulting archive will be formatted as specified(ustar,
 * pax, etc).
 * @return 0 on success, 1 on failure
 */
/* int SYMEXPORT alam_pack(const char *archive, const char *prefix, const char **fn, int compress_t, int format_t) */
int SYMEXPORT alam_pack(const char *archive, const char *prefix, const char **fn)
{
  struct alam_pack_data *alam_pack_data = malloc(sizeof(struct alam_pack_data));
  struct archive *_archive;
  struct archive_entry *entry;
  struct stat st;
  char buff[8192];
  int len;
  int fd;

	_archive = archive_write_new();
	alam_pack_data->name = archive;
	archive_write_set_compression_gzip(_archive);
	archive_write_set_format_ustar(_archive);
	archive_write_open(_archive, alam_pack_data, alam_pack_open, alam_pack_write, alam_pack_close);
	while (*fn) {
		stat(*fn, &st);
		entry = archive_entry_new();
		archive_entry_copy_stat(entry, &st);
		archive_entry_set_pathname(entry, *fn);
		archive_clear_error(_archive);
		archive_write_header(_archive, entry);
		fd = open(*fn, O_RDONLY);
		len = read(fd, buff, sizeof(buff));
		while ( len > 0 ) {
			archive_write_data(_archive, buff, len);
			len = read(fd, buff, sizeof(buff));
		}
		archive_entry_free(entry);
		fn++;
	}
	archive_write_finish(_archive);
}

//------------------------------------------------------------------
/* Uncompression function */
/**
 * @brief Unpack a specific file or all files in an archive.
 *
 * @param archive  the archive to unpack
 * @param prefix   where to extract the files
 * @param fn       a file within the archive to unpack or NULL for all
 * @return 0 on success, 1 on failure
 */
int SYMEXPORT alam_unpack(const char *archive, const char *prefix, const char *fn)
{
	int ret = 0;
	mode_t oldmask;
	struct archive *_archive;
	struct archive_entry *entry;
	char cwd[PATH_MAX];
	int restore_cwd = 0;

	/* ALAM_LOG_FUNC; */
    if (fn == NULL)
        printf("ARCHIVE_INSIDE: %s, LOCATION: %s\n", archive, prefix);
	if((_archive = archive_read_new()) == NULL)
    printf("TEST1\n");
		/* RET_ERR(AM_ERR_LIBARCHIVE, 1); */

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		/* _alam_log(AM_LOG_ERROR, _("could not open %s: %s\n"), archive, */
				/* archive_error_string(_archive)); */
		/* RET_ERR(AM_ERR_PKG_OPEN, 1); */
	}

	oldmask = umask(0022);

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		/* _alam_log(AM_LOG_ERROR, _("could not get current working directory\n")); */
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(prefix) != 0) {
		/* _alam_log(AM_LOG_ERROR, _("could not change directory to %s (%s)\n"), prefix, strerror(errno)); */
		ret = 1;
		goto cleanup;
	}

	while(archive_read_next_header(_archive, &entry) == ARCHIVE_OK) {
		const struct stat *st;
		const char *entryname; /* the name of the file in the archive */

		st = archive_entry_stat(entry);
		entryname = archive_entry_pathname(entry);

		if(S_ISREG(st->st_mode)) {
			archive_entry_set_perm(entry, 0644);
		} else if(S_ISDIR(st->st_mode)) {
			archive_entry_set_perm(entry, 0755);
		}

		/* If a specific file was requested skip entries that don't match. */
		if (fn && strcmp(fn, entryname)) {
			/* _alam_log(AM_LOG_DEBUG, "skipping: %s\n", entryname); */
			if (archive_read_data_skip(_archive) != ARCHIVE_OK) {
				ret = 1;
				goto cleanup;
			}
			continue;
		}

		/* Extract the archive entry. */
		int readret = archive_read_extract(_archive, entry, 0);
		if(readret == ARCHIVE_WARN) {
			/* operation succeeded but a non-critical error was encountered */
			/* _alam_log(AM_LOG_DEBUG, "warning extracting %s (%s)\n", */
					/* entryname, archive_error_string(_archive)); */
		} else if(readret != ARCHIVE_OK) {
			/* _alam_log(AM_LOG_ERROR, _("could not extract %s (%s)\n"), */
					/* entryname, archive_error_string(_archive)); */
			ret = 1;
			goto cleanup;
		}

		if(fn) {
			break;
		}
	}

cleanup:
	umask(oldmask);
	archive_read_finish(_archive);
	if(restore_cwd) {
		chdir(cwd);
	}
	return(ret);
}

//----------------------------------------
/* does the same thing as 'rm -rf' */
int _alam_rmrf(const char *path)
{
	int errflag = 0;
	struct dirent *dp;
	DIR *dirp;
	char name[PATH_MAX];
	struct stat st;

	if(_alam_lstat(path, &st) == 0) {
		if(!S_ISDIR(st.st_mode)) {
			if(!unlink(path)) {
				return(0);
			} else {
				if(errno == ENOENT) {
					return(0);
				} else {
					return(1);
				}
			}
		} else {
			if((dirp = opendir(path)) == (DIR *)-1) {
				return(1);
			}
			for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
				if(dp->d_ino) {
					sprintf(name, "%s/%s", path, dp->d_name);
					if(strcmp(dp->d_name, "..") && strcmp(dp->d_name, ".")) {
						errflag += _alam_rmrf(name);
					}
				}
			}
			closedir(dirp);
			if(rmdir(path)) {
				errflag++;
			}
		}
		return(errflag);
	}
	return(0);
}

int _alam_logaction(unsigned short usesyslog, FILE *f,
		const char *fmt, va_list args)
{
	int ret = 0;

	if(usesyslog) {
		/* we can't use a va_list more than once, so we need to copy it
		 * so we can use the original when calling vfprintf below. */
		va_list args_syslog;
		va_copy(args_syslog, args);
		vsyslog(LOG_WARNING, fmt, args_syslog);
		va_end(args_syslog);
	}

	if(f) {
		time_t t;
		struct tm *tm;

		t = time(NULL);
		tm = localtime(&t);

		/* Use ISO-8601 date format */
		fprintf(f, "[%04d-%02d-%02d %02d:%02d] ",
						tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
						tm->tm_hour, tm->tm_min);
		ret = vfprintf(f, fmt, args);
		fflush(f);
	}

	return(ret);
}

int _alam_run_chroot(const char *root, const char *cmd)
{
	char cwd[PATH_MAX];
	pid_t pid;
	int restore_cwd = 0;
	int retval = 0;

	ALAM_LOG_FUNC;

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		_alam_log(AM_LOG_ERROR, _("could not get current working directory\n"));
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(root) != 0) {
		_alam_log(AM_LOG_ERROR, _("could not change directory to %s (%s)\n"), root, strerror(errno));
		goto cleanup;
	}

	_alam_log(AM_LOG_DEBUG, "executing \"%s\" under chroot \"%s\"\n", cmd, root);

	/* Flush open fds before fork() to avoid cloning buffers */
	fflush(NULL);

	/* fork- parent and child each have seperate code blocks below */
	pid = fork();
	if(pid == -1) {
		_alam_log(AM_LOG_ERROR, _("could not fork a new process (%s)\n"), strerror(errno));
		retval = 1;
		goto cleanup;
	}

	if(pid == 0) {
		FILE *pipe;
		/* this code runs for the child only (the actual chroot/exec) */
		_alam_log(AM_LOG_DEBUG, "chrooting in %s\n", root);
		if(chroot(root) != 0) {
			_alam_log(AM_LOG_ERROR, _("could not change the root directory (%s)\n"),
					strerror(errno));
			exit(1);
		}
		if(chdir("/") != 0) {
			_alam_log(AM_LOG_ERROR, _("could not change directory to / (%s)\n"),
					strerror(errno));
			exit(1);
		}
		umask(0022);
		pipe = popen(cmd, "r");
		if(!pipe) {
			_alam_log(AM_LOG_ERROR, _("call to popen failed (%s)\n"),
					strerror(errno));
			exit(1);
		}
		while(!feof(pipe)) {
			char line[PATH_MAX];
			if(fgets(line, PATH_MAX, pipe) == NULL)
				break;
			alam_logaction("%s", line);
			EVENT(handle->trans, AM_TRANS_EVT_SCRIPTLET_INFO, line, NULL);
		}
		retval = pclose(pipe);
		exit(WEXITSTATUS(retval));
	} else {
		/* this code runs for the parent only (wait on the child) */
		pid_t retpid;
		int status;
		while((retpid = waitpid(pid, &status, 0)) == -1 && errno == EINTR);
		if(retpid == -1) {
			_alam_log(AM_LOG_ERROR, _("call to waitpid failed (%s)\n"),
			          strerror(errno));
			retval = 1;
			goto cleanup;
		} else {
			/* check the return status, make sure it is 0 (success) */
			if(WIFEXITED(status)) {
				_alam_log(AM_LOG_DEBUG, "call to waitpid succeeded\n");
				if(WEXITSTATUS(status) != 0) {
					_alam_log(AM_LOG_ERROR, _("command failed to execute correctly\n"));
					retval = 1;
				}
			}
		}
	}

cleanup:
	if(restore_cwd) {
		chdir(cwd);
	}

	return(retval);
}

int _alam_ldconfig(const char *root)
{
	char line[PATH_MAX];

	_alam_log(AM_LOG_DEBUG, "running ldconfig\n");

	snprintf(line, PATH_MAX, "%setc/ld.so.conf", root);
	if(access(line, F_OK) == 0) {
		snprintf(line, PATH_MAX, "%ssbin/ldconfig", root);
		if(access(line, X_OK) == 0) {
			_alam_run_chroot(root, "/sbin/ldconfig");
		}
	}

	return(0);
}

/* Helper function for comparing strings using the
 * alam "compare func" signature */
int _alam_str_cmp(const void *s1, const void *s2)
{
	return(strcmp(s1, s2));
}

/** Find a filename in a registered alam cachedir.
 * @param filename name of file to find
 * @return malloced path of file, NULL if not found
 */
char *_alam_filecache_find(const char* filename)
{
	char path[PATH_MAX];
	char *retpath;
	alam_list_t *i;

	/* Loop through the cache dirs until we find a matching file */
	for(i = alam_option_get_cachedirs(); i; i = alam_list_next(i)) {
		snprintf(path, PATH_MAX, "%s%s", (char*)alam_list_getdata(i),
				filename);
		if(access(path, R_OK) == 0) {
			retpath = strdup(path);
			_alam_log(AM_LOG_DEBUG, "found cached pkg: %s\n", retpath);
			return(retpath);
		}
	}
	/* package wasn't found in any cachedir */
	return(NULL);
}

/** Check the alam cachedirs for existance and find a writable one.
 * If no valid cache directory can be found, use /tmp.
 * @return pointer to a writable cache directory.
 */
const char *_alam_filecache_setup(void)
{
	struct stat buf;
	alam_list_t *i, *tmp;
	char *cachedir;

	/* Loop through the cache dirs until we find a writeable dir */
	for(i = alam_option_get_cachedirs(); i; i = alam_list_next(i)) {
		cachedir = alam_list_getdata(i);
		if(stat(cachedir, &buf) != 0) {
			/* cache directory does not exist.... try creating it */
			_alam_log(AM_LOG_WARNING, _("no %s cache exists, creating...\n"),
					cachedir);
			if(_alam_makepath(cachedir) == 0) {
				_alam_log(AM_LOG_DEBUG, "using cachedir: %s\n", cachedir);
				return(cachedir);
			}
		} else if(S_ISDIR(buf.st_mode) && (buf.st_mode & S_IWUSR)) {
			_alam_log(AM_LOG_DEBUG, "using cachedir: %s\n", cachedir);
			return(cachedir);
		}
	}

	/* we didn't find a valid cache directory. use /tmp. */
	tmp = alam_list_add(NULL, strdup("/tmp/"));
	alam_option_set_cachedirs(tmp);
	_alam_log(AM_LOG_DEBUG, "using cachedir: %s", "/tmp/\n");
	_alam_log(AM_LOG_WARNING, _("couldn't create package cache, using /tmp instead\n"));
	return(alam_list_getdata(tmp));
}

/** lstat wrapper that treats /path/dirsymlink/ the same as /path/dirsymlink.
 * Linux lstat follows POSIX semantics and still performs a dereference on
 * the first, and for uses of lstat in libalam this is not what we want.
 * @param path path to file to lstat
 * @param buf structure to fill with stat information
 * @return the return code from lstat
 */
int _alam_lstat(const char *path, struct stat *buf)
{
	int ret;
	char *newpath = strdup(path);
	int len = strlen(newpath);

	/* strip the trailing slash if one exists */
	if(len != 0 && newpath[len - 1] == '/') {
			newpath[len - 1] = '\0';
	}

	ret = lstat(newpath, buf);

	FREE(newpath);
	return(ret);
}

/** Get the md5 sum of file.
 * @param filename name of the file
 * @return the checksum on success, NULL on error
 * @addtogroup alam_misc
 */
char SYMEXPORT *alam_compute_md5sum(const char *filename)
{
	unsigned char output[16];
	char *md5sum;
	int ret, i;

	ALAM_LOG_FUNC;

	ASSERT(filename != NULL, return(NULL));

	/* allocate 32 chars plus 1 for null */
	md5sum = calloc(33, sizeof(char));
	ret = md5_file(filename, output);

	if (ret > 0) {
		RET_ERR(AM_ERR_NOT_A_FILE, NULL);
	}

	/* Convert the result to something readable */
	for (i = 0; i < 16; i++) {
		/* sprintf is acceptable here because we know our output */
		sprintf(md5sum +(i * 2), "%02x", output[i]);
	}
	md5sum[32] = '\0';

	_alam_log(AM_LOG_DEBUG, "md5(%s) = %s\n", filename, md5sum);
	return(md5sum);
}

int _alam_test_md5sum(const char *filepath, const char *md5sum)
{
	char *md5sum2;
	int ret;

	md5sum2 = alam_compute_md5sum(filepath);

	if(md5sum == NULL || md5sum2 == NULL) {
		ret = -1;
	} else if(strcmp(md5sum, md5sum2) != 0) {
		ret = 1;
	} else {
		ret = 0;
	}

	FREE(md5sum2);
	return(ret);
}

char *_alam_archive_fgets(char *line, size_t size, struct archive *a)
{
	/* for now, just read one char at a time until we get to a
	 * '\n' char. we can optimize this later with an internal
	 * buffer. */
	/* leave room for zero terminator */
	char *last = line + size - 1;
	char *i;

	for(i = line; i < last; i++) {
		int ret = archive_read_data(a, i, 1);
		/* special check for first read- if null, return null,
		 * this indicates EOF */
		if(i == line && (ret <= 0 || *i == '\0')) {
			return(NULL);
		}
		/* check if read value was null or newline */
		if(ret <= 0 || *i == '\0' || *i == '\n') {
			last = i + 1;
			break;
		}
	}

	/* always null terminate the buffer */
	*last = '\0';

	return(line);
}

/* vim: set ts=2 sw=2 noet: */
