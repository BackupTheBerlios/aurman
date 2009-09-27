/*
 *  conf.h
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
#ifndef _AM_CONF_H
#define _AM_CONF_H

#include <alpm.h>

typedef struct __config_t {
	unsigned short op;
	unsigned short quiet;
	unsigned short verbose;
	unsigned short version;
	unsigned short help;
	unsigned short noconfirm;
	unsigned short noprogressbar;
	unsigned short logmask;
	/* unfortunately, we have to keep track of paths both here and in the library
	 * because they can come from both the command line or config file, and we
	 * need to ensure we get the order of preference right. */
	char *configfile;
	char *rootdir;
	char *dbpath;
	char *logfile;
	/* TODO how to handle cachedirs? */

	unsigned short op_q_isfile;
	unsigned short op_q_info;
	unsigned short op_q_list;
	unsigned short op_q_foreign;
	unsigned short op_q_unrequired;
	unsigned short op_q_deps;
	unsigned short op_q_explicit;
	unsigned short op_q_owns;
	unsigned short op_q_search;
	unsigned short op_q_changelog;
	unsigned short op_q_upgrade;
	unsigned short op_q_check;

	unsigned short op_s_clean;
	unsigned short op_s_downloadonly;
	unsigned short op_s_info;
	unsigned short op_s_sync;
	unsigned short op_s_search;
	unsigned short op_s_upgrade;
	unsigned short op_s_printuris;

	unsigned short group;
	pmtransflag_t flags;
	unsigned short noask;
	unsigned int ask;

	/* conf file options */
	unsigned short chomp; /* I Love Candy! */
	unsigned short showsize; /* show individual package sizes */
	/* When downloading, display the amount downloaded, rate, ETA, and percent
	 * downloaded of the total download list */
	unsigned short totaldownload;
	unsigned short cleanmethod; /* select -Sc behavior */
	alpm_list_t *holdpkg;
	alpm_list_t *syncfirst;
	char *xfercommand;
} config_t;

/* Operations */
enum {
	PM_OP_MAIN = 1,
	PM_OP_REMOVE,
	PM_OP_UPGRADE,
	PM_OP_QUERY,
	PM_OP_SYNC,
	PM_OP_DEPTEST
};

/* Long Operations */
enum {
	AM_LONG_OP_PACMAN = 1000,
	AM_LONG_OP_MAKEPKG,
	AM_LONG_OP_AURMAN,
	AM_LONG_OP_VOTE,
	AM_LONG_OP_UNVOTE,
	AM_LONG_OP_NOTIFY,
	AM_LONG_OP_UNNOTIFY,
	AM_LONG_OP_FLAG,
	AM_LONG_OP_UNFLAG,
	AM_LONG_OP_DISOWN,
	AM_LONG_OP_ADOPT,
	AM_LONG_OP_CHECK,
	AM_LONG_OP_CHECKOUTOFDATE,
	AM_LONG_OP_USER,
	AM_LONG_OP_PASS,
	AM_LONG_OP_DELETE,
	AM_LONG_OP_MYPACKAGES,
	AM_LONG_OP_LISTCAT,
	AM_LONG_OP_PKGSUBMIT,
	AM_LONG_OP_CATEGORY,
	AM_LONG_OP_DOWNLOAD,
	AM_LONG_OP_COMMENT
};

/* clean method */
enum {
	PM_CLEAN_KEEPINST = 0, /* default */
	PM_CLEAN_KEEPCUR
};

/* global config variable */
extern config_t *config;

config_t *config_new(void);
int config_free(config_t *oldconfig);

#endif /* AM_CONF_H */

/* vim: set ts=2 sw=2 noet: */
