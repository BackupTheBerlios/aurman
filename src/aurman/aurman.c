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
#include <ctype.h>
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

/* alam */
#include <alam.h>
#include <alam_list.h>

/* aurman */
#include "aurman.h"
#include "util.h"
#include "conf.h"
#include "db.h"

/* libcurl, libarchive */
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>

amdb_t *db_local;
/* list of targets specified on command line */
static alam_list_t *am_targets;

/* char TMPDIR="/tmp/aurvote-tmp-$(id -un)$$" */
const char aur_main_url[] = "http://aur.archlinux.org";
const char aur_rpc_json_url[] = "http://aur.archlinux.org/rpc.php";
const char aur_account_url[] = "http://aur.archlinux.org/account.php";
const char aur_packages_url[] = "http://aur.archlinux.org/packages.php";
const char aur_submit_url[] = "http://aur.archlinux.org/pkgsubmit.php";
const char aur_packages_dl_location[] = "http://aur.archlinux.org/packages/";

char pkg_search[200] = "";
char dl_pkg_location[200] = "";
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
char *global_arg_value;


typedef void (*line_handler_t) (char *line);


static void setarch(const char *arch)
{
	if (strcmp(arch, "auto") == 0) {
		struct utsname un;
		uname(&un);
		am_printf(AM_LOG_DEBUG, "config: architecture: %s\n", un.machine);
		alam_option_set_arch(un.machine);
	} else {
		am_printf(AM_LOG_DEBUG, "config: architecture: %s\n", arch);
		alam_option_set_arch(arch);
	}
}

//------------------------------------------------------
/** Free the resources.
 *
 * @param ret the return value
 */
static void cleanup(int ret) {
	/* free alam library resources */
	if(alam_release() == -1) {
		am_printf(AM_LOG_ERROR, alam_strerrorlast());
	}

	/* free memory */
	FREELIST(am_targets);
	if(config) {
		config_free(config);
		config = NULL;
	}

	exit(ret);
}

//------------------------------------------------------------------------------
struct HttpFile {
  const char *filename;
  FILE *stream;
};

//------------------------------------------------------------------------------
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

typedef void (*pkg_info_handler_t)(pkg_info_t *info);

//------------------------------------------------------------------------------
typedef enum pkg_info_type {
	PKG_INFO_TYPE_SEARCH,
	PKG_INFO_TYPE_INFO,
	PKG_INFO_TYPE_ERROR,
	PKG_INFO_TYPE_NONE
} pkg_info_type_t;

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
 * @brief Get the next key/value element of the output of rpc/json interface
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

//------------------------------------------------------------------
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
	/* ALAM_LOG_FUNC; */

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
    out->stream=fopen(dl_pkg_location, "wb");
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
	printf("USER: %s, PASS: %s\n", config->user, config->pass);
    snprintf(tmp_str, sizeof(tmp_str), "user=%s&passwd=%s", config->user, config->pass);
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, tmp_str);
    curl_easy_setopt(handle, CURLOPT_URL, aur_main_url);
    curl_easy_perform(handle);
    return 0;
}

//------------------------------------------------------------------------------
/*
 * @brief submits a package to aur
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
 * @brief submits a package to aur
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
/** Display usage/syntax for the specified operation.
 * @param myname basename(argv[0])
 */
static void usage(const char * const myname)
{
	/* prefetch some strings for usage below, which moves a lot of calls
	 * out of gettext. */
	char const * const str_pkg = _("package(s)");
	char const * const str_usg = _("usage");
	char const * const str_opr = _("operation");

    printf("Aurman %s is an AUR manager (based on pacman/libalam) with the desired support\n", PACKAGE_VERSION);
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
	printf("                      Aurman v%s - libalam v%s\n", PACKAGE_VERSION, alam_version());
	printf("                      Copyright (C) 2009 Laszlo Papp <djszapi@archlinux.us>\n");
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
        case AM_LONG_OP_VOTE:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Vote=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_UNVOTE:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnVote=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_FLAG:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Flag=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_UNFLAG:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnFlag=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_NOTIFY:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Notify=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_UNNOTIFY:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_UnNotify=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_ADOPT:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Adopt=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_DISOWN:
            snprintf(tmp_str, sizeof(tmp_str), "IDs[%s]=1&ID=%s&do_Disown=1", pkg_id, pkg_id);
            break;
        case AM_LONG_OP_DELETE:
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

//------------------------------------------------------------------
/* helper for being used with setrepeatingoption */
static void option_add_holdpkg(const char *name) {
	config->holdpkg = alam_list_add(config->holdpkg, strdup(name));
}

//------------------------------------------------------------------
/* helper for being used with setrepeatingoption */
static void option_add_syncfirst(const char *name) {
	config->syncfirst = alam_list_add(config->syncfirst, strdup(name));
}

//------------------------------------------------------------------
/** Add repeating options such as NoExtract, NoUpgrade, etc to libalam
 * settings. Refactored out of the parseconfig code since all of them did
 * the exact same thing and duplicated code.
 * @param ptr a pointer to the start of the multiple options
 * @param option the string (friendly) name of the option, used for messages
 * @param optionfunc a function pointer to an alam_option_add_* function
 */
static void setrepeatingoption(const char *ptr, const char *option,
		void (*optionfunc)(const char*))
{
	char *p = (char*)ptr;
	char *q;

	while((q = strchr(p, ' '))) {
		*q = '\0';
		(*optionfunc)(p);
		am_printf(AM_LOG_DEBUG, "config: %s: %s\n", option, p);
		p = q;
		p++;
	}
	(*optionfunc)(p);
	am_printf(AM_LOG_DEBUG, "config: %s: %s\n", option, p);
}

//------------------------------------------------------------------
static char *get_filename(const char *url) {
	char *filename = strrchr(url, '/');
	if(filename != NULL) {
		filename++;
	}
	return(filename);
}

//------------------------------------------------------------------
static char *get_destfile(const char *path, const char *filename) {
	char *destfile;
	/* len = localpath len + filename len + null */
	int len = strlen(path) + strlen(filename) + 1;
	destfile = calloc(len, sizeof(char));
	snprintf(destfile, len, "%s%s", path, filename);

	return(destfile);
}

//------------------------------------------------------------------
static char *get_tempfile(const char *path, const char *filename) {
	char *tempfile;
	/* len = localpath len + filename len + '.part' len + null */
	int len = strlen(path) + strlen(filename) + 6;
	tempfile = calloc(len, sizeof(char));
	snprintf(tempfile, len, "%s%s.part", path, filename);

	return(tempfile);
}

//------------------------------------------------------------------
/** Sets all libalpm required paths in one go. Called after the command line
 * and inital config file parsing. Once this is complete, we can see if any
 * paths were defined. If a rootdir was defined and nothing else, we want all
 * of our paths to live under the rootdir that was specified. Safe to call
 * multiple times (will only do anything the first time).
 */
static void setlibpaths(void)
{
	static int init = 0;
	if (!init) {
		int ret = 0;

		am_printf(AM_LOG_DEBUG, "setlibpaths() called\n");
		/* Configure root path first. If it is set and dbpath/logfile were not
		 * set, then set those as well to reside under the root. */
		if(config->rootdir) {
			char path[PATH_MAX];
			ret = alam_option_set_root(config->rootdir);
			if(ret != 0) {
				am_printf(AM_LOG_ERROR, _("problem setting rootdir '%s' (%s)\n"),
						config->rootdir, alam_strerrorlast());
				cleanup(ret);
			}
			if(!config->dbpath) {
				/* omit leading slash from our static DBPATH, root handles it */
				snprintf(path, PATH_MAX, "%s%s", alam_option_get_root(), DBPATH + 1);
				config->dbpath = strdup(path);
			}
			if(!config->logfile) {
				/* omit leading slash from our static LOGFILE path, root handles it */
				snprintf(path, PATH_MAX, "%s%s", alam_option_get_root(), LOGFILE + 1);
				config->logfile = strdup(path);
			}
		}
		/* Set other paths if they were configured. Note that unless rootdir
		 * was left undefined, these two paths (dbpath and logfile) will have
		 * been set locally above, so the if cases below will now trigger. */
		if(config->dbpath) {
			ret = alam_option_set_dbpath(config->dbpath);
			if(ret != 0) {
				am_printf(AM_LOG_ERROR, _("problem setting dbpath '%s' (%s)\n"),
						config->dbpath, alam_strerrorlast());
				cleanup(ret);
			}
		}
		if(config->logfile) {
			ret = alam_option_set_logfile(config->logfile);
			if(ret != 0) {
				am_printf(AM_LOG_ERROR, _("problem setting logfile '%s' (%s)\n"),
						config->logfile, alam_strerrorlast());
				cleanup(ret);
			}
		}

		/* add a default cachedir if one wasn't specified */
		if(alam_option_get_cachedirs() == NULL) {
			alam_option_add_cachedir(CACHEDIR);
		}
		init = 1;
	}
}

//------------------------------------------------------------------
/* The real parseconfig. Called with a null section argument by the publicly
 * visible parseconfig so we can recall from within ourself on an include */
static int _parseconfig(const char *file, const char *givensection, amdb_t * const givendb)
{
	FILE *fp = NULL;
	char line[PATH_MAX+1];
	int linenum = 0;
	char *ptr;
	char *section = NULL;
	amdb_t *db = NULL;
	int ret = 0;

	am_printf(AM_LOG_DEBUG, "config: attempting to read file %s\n", file);
	fp = fopen(file, "r");
	if(fp == NULL) {
		am_printf(AM_LOG_ERROR, _("config file %s could not be read.\n"), file);
		return(1);
	}
	/* if we are passed a section, use it as our starting point */
	if(givensection != NULL) {
		section = strdup(givensection);
	}
	/* if we are passed a db, use it as our starting point */
	if(givendb != NULL) {
		db = givendb;
	}

	while(fgets(line, PATH_MAX, fp)) {
		linenum++;
		strtrim(line);

		/* ignore whole line and end of line comments */
		if(strlen(line) == 0 || line[0] == '#') {
			continue;
		}
		if((ptr = strchr(line, '#'))) {
			*ptr = '\0';
		}
		if(line[0] == '[' && line[strlen(line)-1] == ']') {
			/* new config section, skip the '[' */
			ptr = line;
			ptr++;
			if(section) {
				free(section);
			}
			section = strdup(ptr);
			section[strlen(section)-1] = '\0';
			am_printf(AM_LOG_DEBUG, "config: new section '%s'\n", section);
			if(!strlen(section)) {
				am_printf(AM_LOG_ERROR, _("config file %s, line %d: bad section name.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			/* if we are not looking at the options section, register a db */
			if(strcmp(section, "options") != 0) {
				db = alam_db_register_sync(section);
				if(db == NULL) {
					am_printf(AM_LOG_ERROR, _("could not register '%s' database (%s)\n"),
							section, alam_strerrorlast());
					ret = 1;
					goto cleanup;
				}
			}
		} else {
			/* directive */
			char *key;
			/* strsep modifies the 'line' string: 'key \0 ptr' */
			key = line;
			ptr = line;
			key = strsep(&ptr, "=");
			strtrim(key);
			strtrim(ptr);

			if(key == NULL) {
				am_printf(AM_LOG_ERROR, _("config file %s, line %d: syntax error in config file- missing key.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			/* For each directive, compare to the camelcase string. */
			if(section == NULL) {
				am_printf(AM_LOG_ERROR, _("config file %s, line %d: All directives must belong to a section.\n"),
						file, linenum);
				ret = 1;
				goto cleanup;
			}
			if(ptr == NULL && strcmp(section, "options") == 0) {
				/* directives without settings, all in [options] */
				if(strcmp(key, "AlwaysUpgradeAur") == 0) {
					alam_option_set_usesyslog(1);
					am_printf(AM_LOG_DEBUG, "config: usesyslog\n");
				} else if(strcmp(key, "AlwaysUpgradeDevel") == 0) {
					config->chomp = 1;
					am_printf(AM_LOG_DEBUG, "config: chomp\n");
				} else if(strcmp(key, "AlwaysForce") == 0) {
					config->showsize = 1;
					am_printf(AM_LOG_DEBUG, "config: showsize\n");
				} else if(strcmp(key, "AurVoteSupport") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "AutoSaveBackupFile") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "ColorMod") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "DontNeedToPressEnter") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "EditPkgbuild") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "ExportToLocalRepository") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "ForceEnglish") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "LastCommentsNumber") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "LastCommentsOrder") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "NoConfirm") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "PkgbuildEditor") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "SearchInAurUnsupported") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "TmpDirectory") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "SourceforgeMirror") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "UpdateTerminalTitle") == 0) {
					alam_option_set_usedelta(1);
					am_printf(AM_LOG_DEBUG, "config: usedelta\n");
				} else if(strcmp(key, "PacmanBin") == 0) {
					config->totaldownload = 1;
					am_printf(AM_LOG_DEBUG, "config: totaldownload\n");
				} else if(strcmp(key, "UseSyslog") == 0) {
					config->totaldownload = 1;
					am_printf(AM_LOG_DEBUG, "config: totaldownload\n");
				} else if(strcmp(key, "ShowSize") == 0) {
					config->totaldownload = 1;
					am_printf(AM_LOG_DEBUG, "config: totaldownload\n");
				} else if(strcmp(key, "UseDelta") == 0) {
					config->totaldownload = 1;
					am_printf(AM_LOG_DEBUG, "config: totaldownload\n");
				} else if(strcmp(key, "TotalDownload") == 0) {
					config->totaldownload = 1;
					am_printf(AM_LOG_DEBUG, "config: totaldownload\n");
				} else {
					am_printf(AM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					ret = 1;
					goto cleanup;
				}
			} else {
				/* directives with settings */
				if(strcmp(key, "Include") == 0) {
					am_printf(AM_LOG_DEBUG, "config: including %s\n", ptr);
					_parseconfig(ptr, section, db);
					/* Ignore include failures... assume non-critical */
				} else if(strcmp(section, "options") == 0) {
					if(strcmp(key, "NoUpgrade") == 0) {
						setrepeatingoption(ptr, "NoUpgrade", alam_option_add_noupgrade);
					} else if(strcmp(key, "NoExtract") == 0) {
						setrepeatingoption(ptr, "NoExtract", alam_option_add_noextract);
					} else if(strcmp(key, "IgnorePkg") == 0) {
						setrepeatingoption(ptr, "IgnorePkg", alam_option_add_ignorepkg);
					} else if(strcmp(key, "IgnoreGroup") == 0) {
						setrepeatingoption(ptr, "IgnoreGroup", alam_option_add_ignoregrp);
					} else if(strcmp(key, "HoldPkg") == 0) {
						setrepeatingoption(ptr, "HoldPkg", option_add_holdpkg);
					} else if(strcmp(key, "SyncFirst") == 0) {
						setrepeatingoption(ptr, "SyncFirst", option_add_syncfirst);
					} else if(strcmp(key, "Architecture") == 0) {
						if(!alam_option_get_arch()) {
							setarch(ptr);
						}
					} else if(strcmp(key, "User") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->user) {
							config->user = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: User: %s\n", ptr);
						}
					} else if(strcmp(key, "Pass") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->pass) {
							config->pass = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: Pass: %s\n", ptr);
						}
					} else if(strcmp(key, "DownloadDir") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->dl_dir) {
							config->dl_dir = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: DownloadDir: %s\n", ptr);
						}
					} else if(strcmp(key, "DBPath") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->dbpath) {
							config->dbpath = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: dbpath: %s\n", ptr);
						}
					} else if(strcmp(key, "CacheDir") == 0) {
						if(alam_option_add_cachedir(ptr) != 0) {
							am_printf(AM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
									ptr, alam_strerrorlast());
							ret = 1;
							goto cleanup;
						}
						am_printf(AM_LOG_DEBUG, "config: cachedir: %s\n", ptr);
					} else if(strcmp(key, "RootDir") == 0) {
						/* don't overwrite a path specified on the command line */
						if(!config->rootdir) {
							config->rootdir = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: rootdir: %s\n", ptr);
						}
					} else if (strcmp(key, "LogFile") == 0) {
						if(!config->logfile) {
							config->logfile = strdup(ptr);
							am_printf(AM_LOG_DEBUG, "config: logfile: %s\n", ptr);
						}
					/* } else if (strcmp(key, "XferCommand") == 0) { */
						/* config->xfercommand = strdup(ptr); */
						/* alam_option_set_fetchcb(download_with_xfercommand); */
						/* am_printf(AM_LOG_DEBUG, "config: xfercommand: %s\n", ptr); */
					} else if (strcmp(key, "CleanMethod") == 0) {
						if (strcmp(ptr, "KeepInstalled") == 0) {
							config->cleanmethod = AM_CLEAN_KEEPINST;
						} else if (strcmp(ptr, "KeepCurrent") == 0) {
							config->cleanmethod = AM_CLEAN_KEEPCUR;
						} else {
							am_printf(AM_LOG_ERROR, _("invalid value for 'CleanMethod' : '%s'\n"), ptr);
							ret = 1;
							goto cleanup;
						}
						am_printf(AM_LOG_DEBUG, "config: cleanmethod: %s\n", ptr);
					} else {
						am_printf(AM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
								file, linenum, key);
						ret = 1;
						goto cleanup;
					}
				} else if(strcmp(key, "Server") == 0) {
					/* let's attempt a replacement for the current repo */
					char *temp = strreplace(ptr, "$repo", section);
					/* let's attempt a replacement for the arch */
					const char *arch = alam_option_get_arch();
					char *server;
					if(arch) {
						server = strreplace(temp, "$arch", arch);
						free(temp);
					} else {
						if(strstr(temp, "$arch")) {
							free(temp);
							am_printf(AM_LOG_ERROR, _("The mirror '%s' contains the $arch"
										" variable, but no Architecture is defined.\n"), ptr);
							ret = 1;
							goto cleanup;
						}
						server = temp;
					}

					if(alam_db_setserver(db, server) != 0) {
						/* am_errno is set by alam_db_setserver */
						am_printf(AM_LOG_ERROR, _("could not add server URL to database '%s': %s (%s)\n"),
								alam_db_get_name(db), server, alam_strerrorlast());
						free(server);
						ret = 1;
						goto cleanup;
					}

					free(server);
				} else {
					am_printf(AM_LOG_ERROR, _("config file %s, line %d: directive '%s' not recognized.\n"),
							file, linenum, key);
					ret = 1;
					goto cleanup;
				}
			}
		}
	}
cleanup:
	if(fp) {
		fclose(fp);
	}
	if(section){
		free(section);
	}
	/* call setlibpaths here to ensure we have called it at least once */
	setlibpaths();
	am_printf(AM_LOG_DEBUG, "config: finished parsing %s\n", file);
	return(ret);
}

//------------------------------------------------------------------
/** Parse a configuration file.
 * @param file path to the config file.
 * @return 0 on success, non-zero on error
 */
static int parseconfig(const char *file)
{
	/* call the real parseconfig function with a null section & db argument */
	return(_parseconfig(file, NULL, NULL));
}

//------------------------------------------------------------------
/** Parse command-line arguments for each operation.
 * @param argc argc
 * @param argv argv
 * @return 0 on success, 1 on error
 */
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
		{"noconfirm",  			no_argument,       0, AM_LONG_OP_NOCONFIRM},
		{"config",     			required_argument, 0, AM_LONG_OP_CONFIG},
		{"ignore",     			required_argument, 0, AM_LONG_OP_IGNORE},
		{"debug",      			optional_argument, 0, AM_LONG_OP_DEBUG},
		{"noprogressbar", 		no_argument,    	0, AM_LONG_OP_NOPROGRESSBAR},
		{"noscriptlet", 		no_argument,      	0, AM_LONG_OP_NOSCRIPTLET},
		{"ask",        			required_argument, 0, AM_LONG_OP_ASK},
		{"cachedir",   			required_argument, 0, AM_LONG_OP_CACHEDIR},
		{"asdeps",     			no_argument,       0, AM_LONG_OP_ASDEPS},
		{"logfile",    			required_argument, 0, AM_LONG_OP_LOGFILE},
		{"ignoregroup", 		required_argument, 0, AM_LONG_OP_IGNOREGROUP},
		{"needed",     			no_argument,       0, AM_LONG_OP_NEEDED},
		{"asexplicit",     		no_argument,   0, AM_LONG_OP_ASEXPLICIT},
		{"arch",       			required_argument, 0, AM_LONG_OP_ARCH},

		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "RUFQSTr:b:vkhscVfmnoldepqituwygz", opts, &option_index))) {
		alam_list_t *list = NULL, *item = NULL; /* lists for splitting strings */

		if(opt < 0) {
			break;
		}
		switch(opt) {
			case AM_LONG_OP_NOCONFIRM: config->noconfirm = 1; break;
			case AM_LONG_OP_CONFIG:
				if(config->configfile) {
					free(config->configfile);
				}
				config->configfile = strndup(optarg, PATH_MAX);
				break;
			case AM_LONG_OP_IGNORE:
				list = strsplit(optarg, ',');
				for(item = list; item; item = alam_list_next(item)) {
					alam_option_add_ignorepkg((char *)alam_list_getdata(item));
				}
				FREELIST(list);
				break;
			case AM_LONG_OP_DEBUG:
				/* debug levels are made more 'human readable' than using a raw logmask
				 * here, error and warning are set in config_new, though perhaps a
				 * --quiet option will remove these later */
				if(optarg) {
					unsigned short debug = atoi(optarg);
					switch(debug) {
						case 2:
							config->logmask |= AM_LOG_FUNCTION; /* fall through */
						case 1:
							config->logmask |= AM_LOG_DEBUG;
							break;
						default:
						  am_printf(AM_LOG_ERROR, _("'%s' is not a valid debug level\n"),
									optarg);
							return(1);
					}
				} else {
					config->logmask |= AM_LOG_DEBUG;
				}
				/* progress bars get wonky with debug on, shut them off */
				config->noprogressbar = 1;
				break;
			case AM_LONG_OP_NOPROGRESSBAR: config->noprogressbar = 1; break;
			case AM_LONG_OP_NOSCRIPTLET: config->flags |= AM_TRANS_FLAG_NOSCRIPTLET; break;
			case AM_LONG_OP_ASK: config->noask = 1; config->ask = atoi(optarg); break;
			case AM_LONG_OP_CACHEDIR:
				if(alam_option_add_cachedir(optarg) != 0) {
					am_printf(AM_LOG_ERROR, _("problem adding cachedir '%s' (%s)\n"),
							optarg, alam_strerrorlast());
					return(1);
				}
				break;
			case AM_LONG_OP_ASDEPS:
				config->flags |= AM_TRANS_FLAG_ALLDEPS;
				break;
			case AM_LONG_OP_LOGFILE:
				config->logfile = strndup(optarg, PATH_MAX);
				break;
			case AM_LONG_OP_IGNOREGROUP:
				list = strsplit(optarg, ',');
				for(item = list; item; item = alam_list_next(item)) {
					alam_option_add_ignoregrp((char *)alam_list_getdata(item));
				}
				FREELIST(list);
				break;
			case AM_LONG_OP_NEEDED: config->flags |= AM_TRANS_FLAG_NEEDED; break;
			case AM_LONG_OP_ASEXPLICIT:
				config->flags |= AM_TRANS_FLAG_ALLEXPLICIT;
				break;
			case AM_LONG_OP_ARCH:
				setarch(optarg);
				break;

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
		am_targets = alam_list_add(am_targets, strdup(argv[optind]));
		optind++;
	}

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


	/* init config data */
	config = config_new();

	/* initialize library */
	if(alam_initialize() == -1) {
		am_printf(AM_LOG_ERROR, _("failed to initialize alpm library (%s)\n"),
		        alam_strerrorlast());
		cleanup(EXIT_FAILURE);
	}


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

	/* parse the config file */
	ret = parseconfig(config->configfile);
	if(ret != 0) {
		cleanup(ret);
	}

    /* memset(tmp_str, 0, sizeof(tmp_str)); */
    /* snprintf(tmp_str, sizeof(tmp_str), "%s/.aurmanrc", getenv("HOME")); */
    /* if (!file_exists(tmp_str)) { */
        /* printf("To create a new account just go to:"); */
        /* printf("%s", aur_account_url); */
        /* printf("You should create ~/.aurmanrc with inside:\n"); */
        /* printf("user=YOUR_AUR_USERNAME\n"); */
        /* printf("pass=YOUR_AUR_PASSWORD\n"); */
        /* printf("download=YOUR_DOWNLOAD_LOCATION\n"); */
        /* printf("to avoid manual input of these data.\n"); */
        /* printf("Your AUR username: \n"); */
        /* fgets(user, 200, stdin); */
        /* printf("Your AUR password: \n"); */
        /* fgets(pass, 200, stdin); */
        /* printf("Your AUR location, to where to download the packages: \n"); */
        /* fgets(pass, PATH_MAX, stdin); */
    /* } else { // We will implement a better parser here */
        /* aurman_conf_file = fopen(tmp_str, "r"); */
        /* memset(tmp_str, 0, sizeof(tmp_str)); */
        /* fgets(tmp_str, sizeof(tmp_str), aurman_conf_file); */
        /* strncpy(user, &tmp_str[strlen("user=")], sizeof(user)); */
        /* memset(tmp_str, 0, sizeof(tmp_str)); */
        /* fgets(tmp_str, sizeof(tmp_str), aurman_conf_file); */
        /* strncpy(pass, &tmp_str[strlen("pass=")], sizeof(pass)); */
        /* memset(tmp_str, 0, sizeof(tmp_str)); */
        /* fgets(tmp_str, sizeof(tmp_str), aurman_conf_file); */
        /* strncpy(dl_location, &tmp_str[strlen("download=")], sizeof(pass)); */
        /* fclose(aurman_conf_file); */
    /* } */

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
			memset(dl_pkg_location, 0, sizeof(dl_pkg_location));
			snprintf(dl_pkg_location, sizeof(dl_pkg_location), "%s%s", config->dl_dir, pkg_archive);
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
        alam_unpack(dl_pkg_location,  config->dl_dir, NULL);
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
	printf("\tam: AUR, pm: pacman, mp: makepkg related operations\n");
	return -1;
}

