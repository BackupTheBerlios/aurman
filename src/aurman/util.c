/*
 *  util.c
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <wchar.h>

#include <alam.h>
#include <alam_list.h>

/* pacman */
#include "util.h"
#include "conf.h"
/* #include "callback.h" */


int am_printf(amloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = am_vfprintf(stdout, level, format, args);
	va_end(args);

	return(ret);
}

int am_fprintf(FILE *stream, amloglevel_t level, const char *format, ...)
{
	int ret;
	va_list args;

	/* print the message using va_arg list */
	va_start(args, format);
	ret = am_vfprintf(stream, level, format, args);
	va_end(args);

	return(ret);
}

int am_vasprintf(char **string, amloglevel_t level, const char *format, va_list args)
{
	int ret = 0;
	char *msg = NULL;

	/* if current logmask does not overlap with level, do not print msg */
	if(!(config->logmask & level)) {
		return ret;
	}

	/* print the message using va_arg list */
	ret = vasprintf(&msg, format, args);

	/* print a prefix to the message */
	switch(level) {
		case AM_LOG_DEBUG:
			asprintf(string, "debug: %s", msg);
			break;
		case AM_LOG_ERROR:
			asprintf(string, _("error: %s"), msg);
			break;
		case AM_LOG_WARNING:
			asprintf(string, _("warning: %s"), msg);
			break;
		case AM_LOG_FUNCTION:
			asprintf(string, _("function: %s"), msg);
			break;
		default:
			break;
	}
	free(msg);

	return(ret);
}

int am_vfprintf(FILE *stream, amloglevel_t level, const char *format, va_list args)
{
	int ret = 0;

	/* if current logmask does not overlap with level, do not print msg */
	if(!(config->logmask & level)) {
		return ret;
	}

#if defined(PACMAN_DEBUG)
	/* If debug is on, we'll timestamp the output */
  if(config->logmask & AM_LOG_DEBUG) {
		time_t t;
		struct tm *tmp;
		char timestr[10] = {0};

		t = time(NULL);
		tmp = localtime(&t);
		strftime(timestr, 9, "%H:%M:%S", tmp);
		timestr[8] = '\0';

		printf("[%s] ", timestr);
	}
#endif

	/* print a prefix to the message */
	switch(level) {
		case AM_LOG_DEBUG:
			fprintf(stream, "debug: ");
			break;
		case AM_LOG_ERROR:
			fprintf(stream, _("error: "));
			break;
		case AM_LOG_WARNING:
			fprintf(stream, _("warning: "));
			break;
		case AM_LOG_FUNCTION:
		  /* TODO we should increase the indent level when this occurs so we can see
			 * program flow easier.  It'll be fun */
			fprintf(stream, _("function: "));
			break;
		default:
			break;
	}

	/* print the message using va_arg list */
	ret = vfprintf(stream, format, args);
	return(ret);
}

/**
 * @brief count the number of entries in a directory
 *
 * @param dp pointer to an open DIR
 * @param one of am_dirlen_mode
 * @return the number of valid entries counted (effected by \cmode)
 *
 * @note \cdp is assumed to be at the start of the DIRectory and is rewound accordingly
 */
unsigned int am_dirlen(DIR *dp, am_dirlen_mode_t mode)
{
	unsigned int dlen = 0;
	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		switch (mode) {
			case AM_DIRLEN_SKIP_DOTFILES:
				if (de->d_name[0] != '.') {
					++dlen;
				}
				break;
			case AM_DIRLEN_MODE_NORMAL:
				++dlen;
				break;
		}
	}
	rewinddir(dp);
	return dlen;
}

/* Trim whitespace and newlines from a string
 */
char *strtrim(char *str)
{
	char *pch = str;

	if(str == NULL || *str == '\0') {
		/* string is empty, so we're done. */
		return(str);
	}

	while(isspace(*pch)) {
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
	while(isspace(*pch)) {
		pch--;
	}
	*++pch = '\0';

	return(str);
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
/* Replace all occurances of 'needle' with 'replace' in 'str', returning
 * a new string (must be free'd) */
char *strreplace(const char *str, const char *needle, const char *replace)
{
	const char *p = NULL;
	const char *q = NULL;
	char *newstr = NULL;
	char *newp = NULL;
	alam_list_t *i = NULL;
	alam_list_t	*list = NULL;
	size_t needlesz = strlen(needle);
	size_t replacesz = strlen(replace);
	size_t newsz;

	if(!str) {
		return(NULL);
	}

	p = str;
	q = strstr(p, needle);
	while(q) {
		list = alam_list_add(list, (char *)q);
		p = q + needlesz;
		q = strstr(p, needle);
	}

	/* no occurences of needle found */
	if(!list) {
		return(strdup(str));
	}
	/* size of new string = size of old string + "number of occurences of needle"
	 * x "size difference between replace and needle" */
	newsz = strlen(str) + 1 +
		alam_list_count(list) * (replacesz - needlesz);
	newstr = malloc(newsz);
	if(!newstr) {
		return(NULL);
	}
	*newstr = '\0';

	p = str;
	newp = newstr;
	for(i = list; i; i = alam_list_next(i)) {
		q = alam_list_getdata(i);
		if(q > p){
			/* add chars between this occurence and last occurence, if any */
			strncpy(newp, p, q - p);
			newp += q - p;
		}
		strncpy(newp, replace, replacesz);
		newp += replacesz;
		p = q + needlesz;
	}
	alam_list_free(list);

	if(*p) {
		/* add the rest of 'p' */
		strcpy(newp, p);
		newp += strlen(p);
	}
	*newp = '\0';

	return(newstr);
}

//------------------------------------------------------------------
/** Splits a string into a list of strings using the chosen character as
 * a delimiter.
 *
 * @param str the string to split
 * @param splitchar the character to split at
 *
 * @return a list containing the duplicated strings
 */
alam_list_t *strsplit(const char *str, const char splitchar)
{
	alam_list_t *list = NULL;
	const char *prev = str;
	char *dup = NULL;

	while((str = strchr(str, splitchar))) {
		dup = strndup(prev, str - prev);
		if(dup == NULL) {
			return(NULL);
		}
		list = alam_list_add(list, dup);

		str++;
		prev = str;
	}

	dup = strdup(prev);
	if(dup == NULL) {
		return(NULL);
	}
	list = alam_list_add(list, dup);

	return(list);
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


