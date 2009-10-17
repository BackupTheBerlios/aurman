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

#include <alam.h>

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

	char *user;
	char *pass;
	char *dl_dir;
	char *category;
	/* TODO how to handle cachedirs? */

	char *maintainer;

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

	unsigned short am_downloadonly;
	unsigned short am_pkgsubmit;
	unsigned short am_comment;
	unsigned short am_newpackages;

	unsigned short group;
	amtransflag_t flags;
	unsigned short noask;
	unsigned int ask;

	/* conf file options */
	unsigned short chomp; /* I Love Candy! */
	unsigned short showsize; /* show individual package sizes */
	/* When downloading, display the amount downloaded, rate, ETA, and percent
	 * downloaded of the total download list */
	unsigned short totaldownload;
	unsigned short cleanmethod; /* select -Sc behavior */
	alam_list_t *holdpkg;
	alam_list_t *syncfirst;
	char *xfercommand;
} config_t;

/* Operations */
enum {
	AM_OP_MAIN = 1,
	AM_OP_REMOVE,
	AM_OP_UPGRADE,
	AM_OP_QUERY,
	AM_OP_SYNC,
	AM_OP_DEPTEST
};

/* Long Operations */
enum {
	OP_PACMAN = 1000,
	OP_MAKEPKG,
	OP_AURMAN,
	OP_VOTE,
	OP_UNVOTE,
	OP_NOTIFY,
	OP_UNNOTIFY,
	OP_FLAG,
	OP_UNFLAG,
	OP_DISOWN,
	OP_ADOPT,
	OP_CHECK,
	OP_CHECKOUTOFDATE,
	OP_USER,
	OP_PASS,
	OP_DELETE,
	OP_MYPACKAGES,
	OP_LISTCAT,
	OP_PKGSUBMIT,
	OP_CATEGORY,
	OP_DOWNLOAD,
	OP_COMMENT,
	OP_NOCONFIRM,
	OP_CONFIG,
	OP_IGNORE,
	OP_DEBUG,
	OP_NOPROGRESSBAR,
	OP_NOSCRIPTLET,
	OP_ASK,
	OP_CACHEDIR,
	OP_ASDEPS,
	OP_LOGFILE,
	OP_IGNOREGROUP,
	OP_NEEDED,
	OP_ASEXPLICIT,
	OP_ARCH,
//OP_SOURCE,
	OP_MSEARCH
};

/* clean method */
enum {
	AM_CLEAN_KEEPINST = 0, /* default */
	AM_CLEAN_KEEPCUR
};

/* global config variable */
extern config_t *config;

config_t *config_new(void);
int config_free(config_t *oldconfig);

#endif /* AM_CONF_H */

/* vim: set ts=2 sw=2 noet: */
