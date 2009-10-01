/*
 *  aurman.c
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

/* #include "config.h" */

/* special handling of package version for GIT */
#if defined(GIT_VERSION)
#undef PACKAGE_VERSION
#define PACKAGE_VERSION GIT_VERSION
#endif

#include <stdlib.h> /* atoi */
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h> /* uname */
#include <locale.h> /* setlocale */
#include <time.h> /* time_t */
#include <errno.h>
#if defined(PACMAN_DEBUG) && defined(HAVE_MCHECK_H)
#include <mcheck.h> /* debug tracing (mtrace) */
#endif

/* alpm */
#include <alpm.h>
#include <alpm_list.h>

/* aurman */
#include "aurman.h"
#include "util.h"
#include "conf.h"

/* libcurl, libarchive */
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>

/* char TMPDIR="/tmp/aurvote-tmp-$(id -un)$$" */
const char aur_main_url[] = "http://aur.archlinux.org";
const char aur_rpc_json_url[] = "http://aur.archlinux.org/rpc.php";
const char aur_account_url[] = "http://aur.archlinux.org/account.php";
const char aur_packages_url[] = "http://aur.archlinux.org/packages.php";
const char aur_submit_url[] = "http://aur.archlinux.org/pkgsubmit.php";
const char aur_packages_dl_location[] = "http://aur.archlinux.org/packages/";

char pkg_search[200] = "";
char user[200];
char pass[200];
char dl_location[PATH_MAX];
char pkg_id[100];
char pkg_sample[200];
int error_flag = 0;
char package[200];
CURL *handle = NULL;
int pkg_action_id;
char *pkg_name;
int catix;

typedef enum _pkg_detail_action_type_t {
    AM_PKG_ACTION_TYPE_VOTE=1,
    AM_PKG_ACTION_TYPE_UNVOTE,
    AM_PKG_ACTION_TYPE_FLAG,
    AM_PKG_ACTION_TYPE_UNFLAG,
    AM_PKG_ACTION_TYPE_NOTIFY,
    AM_PKG_ACTION_TYPE_UNNOTIFY,
    AM_PKG_ACTION_TYPE_ADOPT,
    AM_PKG_ACTION_TYPE_DISOWN,
    AM_PKG_ACTION_TYPE_DELETE,
} pkg_detail_action_type_t;

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
	AM_ERR_LIBCURL,
	AM_ERR_EXTERNAL_DOWNLOAD
};

//------------------------------------------------------------------------------
struct HttpFile {
  const char *filename;
  FILE *stream;
};

typedef void (*line_handler_t) (char *line);

typedef struct pkg_info {
	const char *name;
	const char *version;
	const char *description;
	const char *url;
	const char *path;
	const char *license;
	unsigned int id;
	unsigned int category;
	unsigned int location;
	unsigned int votes;
	unsigned int out_of_date;
} pkg_info_t;

typedef enum pkg_info_type {
	PKG_INFO_TYPE_SEARCH,
	PKG_INFO_TYPE_INFO,
	PKG_INFO_TYPE_ERROR,
	PKG_INFO_TYPE_NONE
} pkg_info_type_t;

typedef void (*pkg_info_handler_t)(pkg_info_t *info);

//------------------------------------------------------------------
// Strip '\' character from a string
static char *_am_str_strip_back_slash(char *str)
{
	register char *s = str, *t = str;
	while (*s) {
		if (*t == '\\') ++t;
		*s++ = *t++;
	}
	return str;
}

//------------------------------------------------------------------
// Initialization the json/rpc interface related struct
static pkg_info_t *_am_pkg_info_init(pkg_info_t *info)
{
	info->name = "";
	info->version = "";
	info->description = "";
	info->url = "";
	info->path = "";
	info->license = "";
	info->id = 0;
	info->category = 0;
	info->location = 0;
	info->votes = 0;
	info->out_of_date = 0;
	return info;
}

//--------------------------------------------------------------------------
/*
  @brief Get the next key/value element of the output of rpc/json interface
 *
 * @param src: start of the result array
 * @param out_tail: pointer to the location after the got key/value string
 * @param out_str: the got key/value string in the result array
 *
 * @return out_tail
 *
 * @note src is expected to be a valid pointer, no validation done
 */
static char *_am_pip_next_str(char *src, char **out_tail, char **out_str)
{
	register char *s = src;
	*out_str = "";
	*out_tail = NULL;
	if (src) {
		while (*s && *s != '"') ++s;
		if (*s == '"') {
			*out_str = ++s;
			while (*s && (*s != '"' || *(s - 1) == '\\')) ++s;
			if (*s) {
				*s = '\0';
				*out_tail = s + 1;
			}
		}
	}
	return *out_tail;
}

//------------------------------------------------------------------
/*
 * @brief Get the the value of type argument
 *
 * @param src: start of the output of json/rpc interface
 * @param out_tail: pointer to the location after the result value string
 *
 * @return type: pkg_info_type_t value of the type value
 * PKG_INFO_TYPE_NONE in case of error
 *
 * @note src is expected to be a valid pointer, no validation done
 */
static pkg_info_type_t _am_pip_get_type(char *src, char **out_tail)
{
	pkg_info_type_t type = PKG_INFO_TYPE_NONE;
	src = strstr(src, "type\"");
	if (src && *(src += sizeof("type\"")) && _am_pip_next_str(src, out_tail, &src)) {
		if (strcmp(src, "search") == 0) {
			type = PKG_INFO_TYPE_SEARCH;
		} else if (strcmp(src, "info") == 0) {
			type = PKG_INFO_TYPE_INFO;
		} else if (strcmp(src, "error") == 0) {
			type = PKG_INFO_TYPE_ERROR;
		}

		if (!_am_pip_next_str(*out_tail, out_tail, &src) || strcmp(src, "results") != 0) {
			type = PKG_INFO_TYPE_NONE;
		}
	}
	return type;
}

//------------------------------------------------------------------
/* @brief To fill out the pkg_info_t struct with the got data from json/rpc
 * interface
 *
 * @param src: start of the output of json/rpc interface
 * @param out_tail: pointer to the location after the json/rpc interface per
 * package
 * @info: the filled struct with the result values
 *
 * @return type: the location after the the json/rpc interface per package
 *
 *
 * @note src is expected to be a valid pointer, no validation done
 */
static char *_am_pip_info(char *src, char **out_tail, pkg_info_t *info)
{
	char *key, *val;
	while (*src && *src != '{') ++src;
	while (src && *src && *src != '}') {
		_am_pip_next_str(src, &src, &key);
		_am_pip_next_str(src, &src, &val);
		if (src) {
			if (strcmp("ID", key) == 0) {
				info->id = atoi(val);
			} else if (strcmp("Name", key) == 0) {
				info->name = _am_str_strip_back_slash(val);
			} else if (strcmp("Version", key) == 0) {
				info->version = _am_str_strip_back_slash(val);
			} else if (strcmp("CategoryID", key) == 0) {
				info->category = atoi(val);
			} else if (strcmp("Description", key) == 0) {
				info->description = _am_str_strip_back_slash(val);
			} else if (strcmp("LocationID", key) == 0) {
				info->location = atoi(val);
			} else if (strcmp("URL", key) == 0) {
				info->url = _am_str_strip_back_slash(val);
			} else if (strcmp("URLPath", key) == 0) {
				info->path = _am_str_strip_back_slash(val);
			} else if (strcmp("License", key) == 0) {
				info->license = _am_str_strip_back_slash(val);
			} else if (strcmp("NumVotes", key) == 0) {
				info->votes = atoi(val);
			} else if (strcmp("OutOfDate", key) == 0) {
				info->out_of_date = atoi(val);
			}

			while (isspace(*src)) ++src;
		}
	}
	if (src && *src == '\0') src = NULL;
	*out_tail = src;
	return src;
}

/* @brief To fill out the pkg_info_t struct with the got data from json/rpc
 * interface
 *
 * @param src: start of the output of json/rpc interface
 * @param out_tail: pointer to the location after the json/rpc interface per
 * package
 * @handler: the handler function for pkg_info
 *
 * @return type: void
 *
 *
 * @note src is expected to be a valid pointer, no validation done
 * @note handler is expected to be a valid argument, no validation done
 */
//------------------------------------------------------------------
static void _am_pip_foreach(char *src, char **out_tail, pkg_info_handler_t handler)
{
	pkg_info_t info;
	_am_pkg_info_init(&info);
	while (_am_pip_info(src, &src, &info)) {
		handler(&info);
	}
}

//------------------------------------------------------------------
/* @brief Concatenate the elements of a string array into one string, space
 * character between the elements
 *
 * @param count: the number of strings in string vector that we would like to
 * concatenate
 * @param strv: the string vector which contains the strings which will be
 * concatenated
 *
 * @return str: the concatenated string
 *
 *
 * @note strv is expected to be a valid pointer, no validation done
 */
static char *_am_strvcat(int count, char **strv)
{
	char *s, *str;
	size_t n, len = 0;
	int i;

	for (i = 0; i < count; ++i) {
		len += strlen(strv[i]);
	}

	if (len) {
		s = str = malloc(len + count);
		if (str) {
			for (i = 0; i < count; ++i) {
				n = strlen(strv[i]);
				memcpy(s, strv[i], n);
				s += n;
				*s++ = ' ';
			}
			s[-1] = '\0';
		}
	}
	return str;
}

//------------------------------------------------------------------
static int _am_exec(int argc, char **argv, line_handler_t line_handler)
{
	char line[257], *command;
	FILE *proc;
	int ret = -1;

	command = _am_strvcat(argc, argv);
	if (command) {
		if (line_handler) {
			proc = popen(command, "r");
			if (proc) {
				while (fgets(line, sizeof(line), proc)) {
					line_handler(line);
				}
				ret = pclose(proc);
			}
		} else {
			ret = system(command);
		}
		free(command);
	}
	return ret;
}

//------------------------------------------------------------------
/** Parse the basename of a program from a path.
* Grabbed from the uClibc source.
* @param path path to parse basename from
*
* @return everything following the final '/'
*/
char *mbasename(const char *path)
{
	const char *s;
	const char *p;

	p = s = path;

	while (*s) {
		if (*s++ == '/') {
			p = s;
		}
	}

	return (char *)p;
}

//------------------------------------------------------------------
/* Compression functions */
/**
 * @brief Unpack a specific file or all files in an archive.
 *
 * @param archive  the archive to unpack
 * @param prefix   where to extract the files
 * @param fn       a file within the archive to unpack or NULL for all
 * @return 0 on success, 1 on failure
 */
int am_unpack(const char *archive, const char *prefix, const char *fn)
{
	int ret = 0;
	mode_t oldmask;
	struct archive *_archive;
	struct archive_entry *entry;
	char cwd[PATH_MAX];
	int restore_cwd = 0;

	/* ALPM_LOG_FUNC; */
    if (fn == NULL)
        printf("ARCHIVE_INSIDE: %s, LOCATION: %s\n", archive, prefix);
	if((_archive = archive_read_new()) == NULL)
    printf("TEST1\n");
		/* RET_ERR(PM_ERR_LIBARCHIVE, 1); */

	archive_read_support_compression_all(_archive);
	archive_read_support_format_all(_archive);

	if(archive_read_open_filename(_archive, archive,
				ARCHIVE_DEFAULT_BYTES_PER_BLOCK) != ARCHIVE_OK) {
		/* _alpm_log(PM_LOG_ERROR, _("could not open %s: %s\n"), archive, */
				/* archive_error_string(_archive)); */
		/* RET_ERR(PM_ERR_PKG_OPEN, 1); */
	}

	oldmask = umask(0022);

	/* save the cwd so we can restore it later */
	if(getcwd(cwd, PATH_MAX) == NULL) {
		/* _alpm_log(PM_LOG_ERROR, _("could not get current working directory\n")); */
	} else {
		restore_cwd = 1;
	}

	/* just in case our cwd was removed in the upgrade operation */
	if(chdir(prefix) != 0) {
		/* _alpm_log(PM_LOG_ERROR, _("could not change directory to %s (%s)\n"), prefix, strerror(errno)); */
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
			/* _alpm_log(PM_LOG_DEBUG, "skipping: %s\n", entryname); */
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
			/* _alpm_log(PM_LOG_DEBUG, "warning extracting %s (%s)\n", */
					/* entryname, archive_error_string(_archive)); */
		} else if(readret != ARCHIVE_OK) {
			/* _alpm_log(PM_LOG_ERROR, _("could not extract %s (%s)\n"), */
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

//------------------------------------------------------------------------------
CURL *_am_handle_new()
{
	CURL *handle;

	handle = malloc(sizeof(CURL));
	if(handle == NULL) {
        printf("Couldn't allocate memory for handler\n");
	}
	memset(handle, 0, sizeof(CURL));
	return(handle);
}

//------------------------------------------------------------------------------
void _am_handle_free(CURL *handle)
{
	/* ALPM_LOG_FUNC; */

	if(handle == NULL) {
		return;
	}

	free(handle);
}

//------------------------------------------------------------------------------
static size_t my_fwrite(void *buffer, size_t size, size_t nmemb, void *stream)
{
  struct HttpFile *out=(struct HttpFile *)stream;
  if(out && !out->stream) {
    /* open file for writing */
    /* out->stream=fopen(out->filename, "wb"); */
    out->stream=fopen(dl_location, "wb");
    if(!out->stream)
      return -1; /* failure, can't open file to write */
  }
  return fwrite(buffer, size, nmemb, out->stream);
}

//------------------------------------------------------------------
const char *categories[] = {
    "Select Category",
    "daemons",
    "devel",
    "editors",
    "emulators",
    "games",
    "gnome",
    "i18n",
    "kde",
    "lib",
    "modules",
    "multimedia",
    "network",
    "office",
    "science",
    "system",
    "x11",
    "xfce",
    "kernels",
};

extern enum _aurerrno_t aur_errno;

//------------------------------------------------------------------
int findix_cat(char *category)
{
    int ix = 0;
    for(ix = 0; categories[ix] != NULL || !strncmp(categories[ix], "", sizeof(categories[ix])) || ix < 20; ix++) {
        if(!strncmp(categories[ix], category, sizeof(categories[ix])))
            break;
    }
    return(ix+1);
}
//------------------------------------------------------------------
bool file_exists(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

//-------------------------------------------------------------------------
size_t dummy_func(void *ptr, size_t size, size_t nmemb, void *stream)
{
    return size*nmemb;
}

//-------------------------------------------------------------------------
int am_login(CURL *handle)
{
    char tmp_str[200] = "";
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, dummy_func);
    memset(tmp_str,0,sizeof(tmp_str));
    snprintf(tmp_str, sizeof(tmp_str), "%s/.aurman.cookies", getenv("HOME"));
    curl_easy_setopt(handle, CURLOPT_COOKIEFILE, tmp_str);
    curl_easy_setopt(handle, CURLOPT_COOKIEJAR, tmp_str);
    memset(tmp_str,0,sizeof(tmp_str));
    snprintf(tmp_str, sizeof(tmp_str), "user=%s&passwd=%s", user, pass);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, tmp_str);
    curl_easy_setopt(handle, CURLOPT_URL, aur_main_url);
    curl_easy_perform(handle);
    return 0;
}

//------------------------------------------------------------------------------
/*
  @brief submits a package to aur
 *
 * @param handle AUR handler for that cURL session
 * @param url AUR url
 * @param filename Path of file to upload
 * @param category Numeric package category
 * @param result Out-pointer to store cURL result, pass NULL to ignore the result

 * @return 0 on success, non-zero on error
 *
 * @note the return value does not reflect cURL status/result, use @param:result
 * @note filename is expected to be a valid package, no validation done
 */
size_t get_pkg_id(void *ptr, size_t size, size_t nmemb, void *stream)
{
    char *tmp_str = NULL;
    char id_local[100] = "";
    int retval = 0;
    int tmp_len = 0;
    int i = 0;

    tmp_str = ptr;

    while(1) {
        tmp_str = strstr((char*)tmp_str, "\"ID\":\"");
        if (tmp_str == NULL) {
            error_flag = 1;
            break;
        } else {
            /* printf("%s\n", tmp_str); */
        }
        memset(id_local, 0, sizeof(id_local));
        tmp_len = strlen("\"ID\":\"");
        for (i = 0; *(tmp_str+tmp_len+i) <= '9' && *(tmp_str+tmp_len+i) >= '0'; i++) {
            id_local[i] = *(tmp_str+tmp_len+i);
        }

        tmp_str = strstr((char*)tmp_str, "\"Name\":\"");
        if (tmp_str == NULL) {
            error_flag = 1;
            break;
        } else {
            tmp_len = strlen("\"Name\":\"");
        }
        retval = strncmp(pkg_sample, tmp_str+tmp_len, strlen(pkg_sample));
        if (!retval) {
            strncpy(pkg_id, id_local, sizeof(pkg_id));
            break;
        } else {
            error_flag = 1;
        }
    }
    return size*nmemb;
}

//------------------------------------------------------------------------------
/*
  @brief submits a package to aur
 *
 * @param handle AUR handler for that cURL session
 * @param url AUR url
 * @param filename Path of file to upload
 * @param category Numeric package category
 * @param result out-pointer to store cURL result, pass NULL to ignore the result

 * @return 0 on success, non-zero on error
 *
 * @note the return value does not reflect cURL status/result, use @param:result
 * @note filename is expected to be a valid package, no validation done
 */
int am_pkgsubmit (CURL *handle, const char *url, const char *filename, const char *category, CURLcode *result)
{
    struct curl_httppost *form = NULL;
    struct curl_httppost *last = NULL;
    struct curl_slist *headers = NULL;
    static const char buf[] = "Expect:";

    if (handle) {
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "pfile", CURLFORM_FILE,
                filename, CURLFORM_END);
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "category",
                CURLFORM_COPYCONTENTS, category, CURLFORM_END);
        curl_formadd(&form, &last, CURLFORM_COPYNAME, "pkgsubmit",
                CURLFORM_COPYCONTENTS, "1", CURLFORM_END);

        /* disable the curl's expect: header, causes errors sometimes */
        headers = curl_slist_append(headers, buf);
        if (headers) {
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        }
        curl_easy_setopt(handle, CURLOPT_URL, url);
        curl_easy_setopt(handle, CURLOPT_HTTPPOST, form);

        if (result) {
            *result = curl_easy_perform(handle);
        } else {
            curl_easy_perform(handle);
        }
        curl_formfree(form);
        curl_slist_free_all(headers);
        return 0;
    }
    return 1;
}

//-------------------------------------------------------------------------
int am_comment(CURL *handle)
{
    char comment_file[200] = "";
    char *editor;
    char tmp_str[200] = "";
    struct curl_httppost *post=NULL;
    struct curl_httppost *last=NULL;
    struct curl_slist *headers=NULL;

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_pkg_id);
    curl_easy_setopt(handle, CURLOPT_URL, pkg_search);
    curl_easy_setopt(handle, CURLOPT_POST, 0);
    curl_easy_perform(handle);

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, sizeof(tmp_str), "%s?ID=%s", aur_packages_url, pkg_id);
    curl_easy_setopt(handle, CURLOPT_URL, tmp_str);

    editor = getenv("EDITOR");
    if (editor == NULL) {
        editor="vi";
    }

    snprintf(comment_file, sizeof(comment_file), "%s/.aurman.comment", getenv("HOME"));
    unlink(comment_file);
    snprintf(tmp_str, sizeof(tmp_str), "%s %s", editor, comment_file);
    system(tmp_str);

    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    curl_formadd(&post, &last,
                 CURLFORM_COPYNAME, "comment",
                 CURLFORM_FILECONTENT, comment_file, CURLFORM_END);
    curl_easy_setopt(handle, CURLOPT_HTTPPOST, post);

    snprintf(tmp_str, sizeof(tmp_str), "%s?ID=%s", aur_packages_url, pkg_id);
    curl_easy_setopt(handle, CURLOPT_URL, tmp_str);
    curl_easy_perform(handle);
    curl_slist_free_all(headers);
    curl_formfree(post);
    unlink(comment_file);

    return 0;
}

//-------------------------------------------------------------------------
#ifndef __GNUC__
/* #ifndef HAVE_STRNDUP */
/* A quick and dirty implementation derived from glibc */
static size_t strnlen(const char *s, size_t max)
{
    register const char *p;
    for(p = s; *p && max--; ++p);
    return(p - s);
}

//-------------------------------------------------------------------------
char *strndup(const char *s, size_t n)
{
  size_t len = strnlen(s, n);
  char *new = (char *) malloc(len + 1);

  if (new == NULL)
    return NULL;

  new[len] = '\0';
  return (char *) memcpy(new, s, len);
}
#endif

//-------------------------------------------------------------------------
/** Display usage/syntax for the specified operation.
 * @param op     the operation code requested
 * @param myname basename(argv[0])
 */
static void usage(const char * const myname)
{
	/* prefetch some strings for usage below, which moves a lot of calls
	 * out of gettext. */
	char const * const str_pkg = _("package(s)");
	char const * const str_usg = _("usage");
	char const * const str_opr = _("operation");

    printf("Aurman %s is a pacman frontend with AUR support and more\n", PACKAGE_VERSION);
    printf("homepage: %s\n", HOMEPAGE);
    printf("      Copyright (C) 2009 Laszlo Papp <djszapi@archlinux.us>\n");
    printf("      This program may be freely redistributed under\n");
    printf("      the terms of the GNU General Public License\n");
    printf("%s:  %s <%s> [...]\n", str_usg, myname, str_opr);
    printf("operations:\n");
    printf("      -h, --help         Print Help (this message) and exit\n");
    printf("      -V, --version       Print version information and exit\n");
    printf("      --vote <package>    Vote for %s\n", str_pkg);
    printf("      --unvote <package>  Unvote for %s\n", str_pkg);
    printf("      --notify            Notify for %s\n", str_pkg);
    printf("      --unnotify          Unnotify for %s\n", str_pkg);
    printf("      --flag              Flag for %s\n", str_pkg);
    printf("      --unflag            Unflag for %s\n", str_pkg);
    printf("      --adopt             Adopt %s\n", str_pkg);
    printf("      --unnotify          Unnotify for %s\n", str_pkg);
    printf("      --flag              Flag for %s\n", str_pkg);
    printf("      --unflag            Unflag for %s\n", str_pkg);
    printf("      --adopt             Adopt %s\n", str_pkg);
    printf("      --user              Set user which will do the operations\n");
    printf("      --pass              Set the password for the user\n");
    printf("      --listcat           List available category\n");
    printf("      --pkgsubmit         Submit the pkgfile.tar.gz %s\n", str_pkg);
    printf("      --category          Set the category for submitting\n");
    printf("      --download          Downlaod %s from AUR\n", str_pkg);
    printf("      --comment           Take a comment/feedback for %s\n", str_pkg);
}

//------------------------------------------------------------------
/** Output aurman version and copyright.
 */
static void version(void)
{
	printf("\n");
	printf("                      Aurman v%s - libalpm v%s\n", PACKAGE_VERSION, alpm_version());
	printf("                      Copyright (C) 2009 Laszlo Papp <djszapi@archlinux.us>\n");
	/* printf("\\  '-. '-'  '-'  '-'   Copyright (C) 2002-2006 Aurman * Development Team\n"); */
	printf("\n");
	printf(_("                    This program may be freely redistributed under\n"
	         "                    the terms of the GNU General Public License.\n"));
	printf("\n");
}

//------------------------------------------------------------------
int list_cat() {
   printf("Category List:\n");
   printf("daemons\n");
   printf("devel\n");
   printf("editors\n");
   printf("emulators\n");
   printf("games\n");
   printf("gnome\n");
   printf("i18n\n");
   printf("kde\n");
   printf("lib\n");
   printf("modules\n");
   printf("multimedia\n");
   printf("network\n");
   printf("office\n");
   printf("science\n");
   printf("system\n");
   printf("x11\n");
   printf("xfce\n");
   printf("kernels\n");
   return 0;
} // end of list_cat()

//------------------------------------------------------------------
int am_pkg_action(CURL *handle, int am_pkg_action_t) {

    char tmp_str[200] = "";

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_pkg_id);
    curl_easy_setopt(handle, CURLOPT_URL, pkg_search);
    curl_easy_setopt(handle, CURLOPT_POST, 0);
    curl_easy_perform(handle);

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, sizeof(tmp_str), "%s?ID=%s", aur_packages_url, pkg_id);
    curl_easy_setopt(handle, CURLOPT_URL, tmp_str);

    memset(tmp_str, 0, sizeof(tmp_str));
    switch(am_pkg_action_t) {
        case AM_PKG_ACTION_TYPE_VOTE:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Vote=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_UNVOTE:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnVote=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_FLAG:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Flag=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_UNFLAG:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnFlag=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_NOTIFY:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Notify=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_UNNOTIFY:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnNotify=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_ADOPT:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Adopt=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_DISOWN:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Disown=1", pkg_id, pkg_id);
            break;
        case AM_PKG_ACTION_TYPE_DELETE:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Delete=1", pkg_id, pkg_id);
            break;
        default:
            return(1);
            break;
    }

    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, tmp_str);
    curl_easy_perform(handle);

    return 0;
}


char *global_arg_value;
//------------------------------------------------------------------
static int parseargs(int argc, char *argv[])
{
	int opt;
	int option_index = 0;
	static struct option opts[] =
	{
		{"help",                no_argument,       0, 'h'},
		{"version",             no_argument,       0, 'V'},
		{"pacman",              no_argument,       0,  AM_LONG_OP_PACMAN},
		{"makepkg",             required_argument, 0,  AM_LONG_OP_MAKEPKG},
		{"aurman",              required_argument, 0,  AM_LONG_OP_AURMAN},
		{"vote",                required_argument, 0,  AM_LONG_OP_VOTE},
		{"unvote",              required_argument, 0,  AM_LONG_OP_UNVOTE},
		{"notify",              required_argument, 0,  AM_LONG_OP_NOTIFY},
		{"unnotify",            required_argument, 0,  AM_LONG_OP_UNNOTIFY},
		{"flag",                required_argument, 0,  AM_LONG_OP_FLAG},
		{"unflag",              required_argument, 0,  AM_LONG_OP_UNFLAG},
		{"adopt",               required_argument, 0,  AM_LONG_OP_ADOPT},
		{"disown",              required_argument, 0,  AM_LONG_OP_DISOWN},
		{"check",               required_argument, 0,  AM_LONG_OP_CHECK},
		{"checkoutofdate",      required_argument, 0,  AM_LONG_OP_CHECKOUTOFDATE},
		{"user",                required_argument, 0,  AM_LONG_OP_USER},
		{"pass",                required_argument, 0,  AM_LONG_OP_PASS},
		{"delete",              required_argument, 0,  AM_LONG_OP_DELETE},
		{"mypackages",          required_argument, 0,  AM_LONG_OP_MYPACKAGES},
		{"listcat",             no_argument,       0,  AM_LONG_OP_LISTCAT},
		{"pkgsubmit",           required_argument, 0,  AM_LONG_OP_PKGSUBMIT},
		{"category",            no_argument,       0,  AM_LONG_OP_CATEGORY},
		{"download",            required_argument, 0,  AM_LONG_OP_DOWNLOAD},
		{"comment",             required_argument, 0,  AM_LONG_OP_COMMENT},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "RUFQSTr:b:vkhscVfmnoldepqituwygz", opts, &option_index))) {
		/* alpm_list_t *list = NULL, *item = NULL; [> lists for splitting strings <] */

		if(opt < 0) {
			break;
		}
		switch(opt) {
            case AM_LONG_OP_VOTE:
                pkg_action_id = AM_LONG_OP_VOTE;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_UNVOTE:
                pkg_action_id = AM_LONG_OP_UNVOTE;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_NOTIFY:
                pkg_action_id = AM_LONG_OP_NOTIFY;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_UNNOTIFY:
                pkg_action_id = AM_LONG_OP_UNNOTIFY;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_FLAG:
                pkg_action_id = AM_LONG_OP_FLAG;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_UNFLAG:
                pkg_action_id = AM_LONG_OP_UNFLAG;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_ADOPT:
                pkg_action_id = AM_LONG_OP_ADOPT;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_DISOWN:
                pkg_action_id = AM_LONG_OP_DISOWN;
                pkg_name = strdup(optarg);
                break;
            case AM_LONG_OP_USER:
                memset(user, 0, sizeof(user));
                strncpy(user, optarg, sizeof(optarg));
                break;
            case AM_LONG_OP_PASS:
                memset(pass, 0, sizeof(pass));
                strncpy(pass, optarg, sizeof(optarg));
                break;
            case AM_LONG_OP_PKGSUBMIT:
                config->am_pkgsubmit = 1;
                pkg_name = strndup(optarg, PATH_MAX);
            case AM_LONG_OP_CATEGORY:
                break;
            case AM_LONG_OP_DOWNLOAD:
                config->am_downloadonly = 1;
                pkg_name = strndup(optarg, PATH_MAX);
                break;
            case AM_LONG_OP_COMMENT:
                config->am_comment = 1;
                pkg_name = strndup(optarg, PATH_MAX);
                break;
            case 'h':
		        usage(mbasename(argv[0]));
                break;
            case 'V':
                version();
                break;
            case AM_LONG_OP_LISTCAT:
                list_cat();
                break;
            case '?': return(1);
            default: return(1);
        }
    }

    while(optind < argc) {
        /* add the target to our target array */
        // The next version will handle more arguments too
        memset(package,0,sizeof(package));
        strncpy(package,argv[optind],strlen(package));
        /* pm_targets = alpm_list_add(pm_targets, strdup(argv[optind])); */
        optind++;
    }

	/* return(0); */
    return(1);
}

//------------------------------------------------------------------
int am_aur (int argc, char *argv[])
{
    int ret = 0;
    FILE *aurman_conf_file = NULL;
    char tmp_str[200] = "";
    char pkg_archive[200] = "";
    CURLcode res = (CURLcode)NULL;
    struct HttpFile httpfile={
        NULL, /* name to store the file as if succesful */
        NULL
    };

    handle = _am_handle_new();

    memset(tmp_str, 0, sizeof(tmp_str));
    snprintf(tmp_str, sizeof(tmp_str), "%s/.aurmanrc", getenv("HOME"));
    if (!file_exists(tmp_str)) {
        printf("To create a new account just go to:");
        printf("%s", aur_account_url);
        printf("You should create ~/.aurmanrc with inside:\n");
        printf("user=YOUR_AUR_USERNAME\n");
        printf("pass=YOUR_AUR_PASSWORD\n");
        printf("download=YOUR_DOWNLOAD_LOCATION\n");
        printf("to avoid manual input of these data.\n");
        printf("Your AUR username: \n");
        fgets(user, 200, stdin);
        printf("Your AUR password: \n");
        fgets(pass, 200, stdin);
        printf("Your AUR location, to where to download the packages: \n");
        fgets(pass, PATH_MAX, stdin);
    } else { // We will implement a better parser here
        aurman_conf_file = fopen(tmp_str, "r");
        memset(tmp_str, 0, sizeof(tmp_str));
        fgets(tmp_str, sizeof(tmp_str), aurman_conf_file);
        strncpy(user, &tmp_str[strlen("user=")], sizeof(user));
        memset(tmp_str, 0, sizeof(tmp_str));
        fgets(tmp_str, sizeof(tmp_str), aurman_conf_file);
        strncpy(pass, &tmp_str[strlen("pass=")], sizeof(pass));
        memset(tmp_str, 0, sizeof(tmp_str));
        fgets(tmp_str, sizeof(tmp_str), aurman_conf_file);
        strncpy(dl_location, &tmp_str[strlen("download=")], sizeof(pass));
        fclose(aurman_conf_file);
    }

	/* init config data */
	config = config_new();

	/* Priority of options:
	 * 1. command line
	 * 2. config file
	 * 3. compiled-in defaults
	 * However, we have to parse the command line first because a config file
	 * location can be specified here, so we need to make sure we prefer these
	 * options over the config file coming second.
	 */

	/* parse the command line */
	ret = parseargs(argc, argv);
	if(!ret) {
		return -1;
	}

    if (argv[1] != NULL) {
        snprintf(pkg_sample, sizeof(pkg_sample), "%s\",", pkg_name);
        snprintf(pkg_search, sizeof(pkg_search), "%s?type=search&arg=%s", aur_rpc_json_url, pkg_name);
    } else {
        printf("Invalid argument\n");
        return -1;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    handle = curl_easy_init();

    if (pkg_action_id) {
        am_login(handle);
        am_pkg_action(handle, pkg_action_id);
    }

    if (config->am_pkgsubmit) {
        am_login(handle);
        am_pkgsubmit (handle, aur_submit_url, pkg_name, "3", &res);
    }

    if (config->am_downloadonly) {
        if(handle) {
            /*
             * Get curl 7.9.2 from AUR http site. curl 7.9.2 is most likely not
             * present there by the time you read this, so you'd better replace the
             * URL with one that works!
             */
            memset(pkg_archive, 0, sizeof(pkg_archive));
            snprintf(pkg_archive, sizeof(pkg_archive), "%s.tar.gz", pkg_name);
            httpfile.filename = strndup(pkg_archive, sizeof(pkg_archive));
            memset(tmp_str, 0, sizeof(tmp_str));
            snprintf(tmp_str, sizeof(tmp_str), "%s%s/%s.tar.gz", aur_packages_dl_location, pkg_name, pkg_name);
            curl_easy_setopt(handle, CURLOPT_URL, tmp_str);
            /* Define our callback to get called when there's data to be written */
            curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, my_fwrite);
            /* Set a pointer to our struct to pass to the callback */
            curl_easy_setopt(handle, CURLOPT_WRITEDATA, &httpfile);

            /* Switch on full protocol/debug output */
            /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

            res = curl_easy_perform(handle);
        }
        if(httpfile.stream)
            fclose(httpfile.stream); /* close the local file */
        am_unpack(pkg_archive, ".", NULL);
    }

    if (config->am_comment) {
        am_login(handle);
        am_comment(handle);
    }

    curl_easy_cleanup(handle);
    curl_global_cleanup();
    free(pkg_name);
    /* _am_handle_free(handle); */

    return 0;
}

//------------------------------------------------------------------
int am_pacman(int argc, char *argv[])
{
	argv[0] = "pacman";
	return _am_exec(argc, argv, NULL);
}

//------------------------------------------------------------------
int am_makepkg(int argc, char *argv[])
{
	argv[0] = "makepkg";
	return _am_exec(argc, argv, NULL);
}

//------------------------------------------------------------------
int main (int argc, char *argv[])
{
	const char *command;
	char **args;

	if (argc > 1) {
		command = argv[1];
		args = argv + 1;
		args[0] = argv[0];
		--argc;

		if (strcmp("am", command) == 0) {
			return am_aur(argc, args);
		} else if (strcmp("pm", command) == 0) {
			return am_pacman(argc, args);
		} else if (strcmp("mp", command) == 0) {
			return am_makepkg(argc, args);
		} else {
			printf(_("Invalid operation: %s\n"), command);
		}
	}
	command = mbasename(argv[0]);
	printf(_("Usage: %s <am|pm|mp> [operation e.g: -h, --help...]\n"), command);
	printf("\tam: AUR, pm: pacman, mp: makepkg\n");
	return -1;
}

