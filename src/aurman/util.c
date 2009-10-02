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

#include <alpm.h>
#include <alpm_list.h>

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
  if(config->logmask & PM_LOG_DEBUG) {
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

