/*
 *  trans.h
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
#ifndef _ALAM_TRANS_H
#define _ALAM_TRANS_H

#include "alam.h"

typedef enum _amtransstate_t {
	STATE_IDLE = 0,
	STATE_INITIALIZED,
	STATE_PREPARED,
	STATE_DOWNLOADING,
	STATE_COMMITING,
	STATE_COMMITED,
	STATE_INTERRUPTED
} amtransstate_t;

/* Transaction */
struct __amtrans_t {
	amtransflag_t flags;
	amtransstate_t state;
	alam_list_t *add;      /* list of (ampkg_t *) */
	alam_list_t *remove;      /* list of (ampkg_t *) */
	alam_list_t *skip_add;      /* list of (char *) */
	alam_list_t *skip_remove;   /* list of (char *) */
	alam_trans_cb_event cb_event;
	alam_trans_cb_conv cb_conv;
	alam_trans_cb_progress cb_progress;
};

#define EVENT(t, e, d1, d2) \
do { \
	if((t) && (t)->cb_event) { \
		(t)->cb_event(e, d1, d2); \
	} \
} while(0)
#define QUESTION(t, q, d1, d2, d3, r) \
do { \
	if((t) && (t)->cb_conv) { \
		(t)->cb_conv(q, d1, d2, d3, r); \
	} \
} while(0)
#define PROGRESS(t, e, p, per, h, r) \
do { \
	if((t) && (t)->cb_progress) { \
		(t)->cb_progress(e, p, per, h, r); \
	} \
} while(0)

amtrans_t *_alam_trans_new(void);
void _alam_trans_free(amtrans_t *trans);
int _alam_trans_init(amtrans_t *trans, amtransflag_t flags,
                     alam_trans_cb_event event, alam_trans_cb_conv conv,
                     alam_trans_cb_progress progress);
int _alam_runscriptlet(const char *root, const char *installfn,
                       const char *script, const char *ver,
                       const char *oldver, amtrans_t *trans);

#endif /* _ALAM_TRANS_H */

/* vim: set ts=2 sw=2 noet: */
