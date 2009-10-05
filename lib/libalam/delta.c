/*
 *  delta.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2007-2006 by Judd Vinet <jvinet@zeroflux.org>
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h> /* intmax_t */
#include <limits.h>
#include <sys/types.h>
#include <regex.h>

/* libalam */
#include "delta.h"
#include "alam_list.h"
#include "util.h"
#include "log.h"
#include "graph.h"

/** \addtogroup alam_deltas Delta Functions
 * @brief Functions to manipulate libalam deltas
 * @{
 */

const char SYMEXPORT *alam_delta_get_from(amdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->from);
}

const char SYMEXPORT *alam_delta_get_to(amdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->to);
}

const char SYMEXPORT *alam_delta_get_filename(amdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->delta);
}

const char SYMEXPORT *alam_delta_get_md5sum(amdelta_t *delta)
{
	ASSERT(delta != NULL, return(NULL));
	return(delta->delta_md5);
}

off_t SYMEXPORT alam_delta_get_size(amdelta_t *delta)
{
	ASSERT(delta != NULL, return(-1));
	return(delta->delta_size);
}

/** @} */

static alam_list_t *delta_graph_init(alam_list_t *deltas)
{
	alam_list_t *i, *j;
	alam_list_t *vertices = NULL;
	/* create the vertices */
	for(i = deltas; i; i = i->next) {
		char *fpath, *md5sum;
		pmgraph_t *v = _alam_graph_new();
		amdelta_t *vdelta = i->data;
		vdelta->download_size = vdelta->delta_size;
		v->weight = LONG_MAX;

		/* determine whether the delta file already exists */
		fpath = _alam_filecache_find(vdelta->delta);
		md5sum = alam_compute_md5sum(fpath);
		if(fpath && md5sum && strcmp(md5sum, vdelta->delta_md5) == 0) {
			vdelta->download_size = 0;
		}
		FREE(fpath);
		FREE(md5sum);

		/* determine whether a base 'from' file exists */
		fpath = _alam_filecache_find(vdelta->from);
		if(fpath) {
			v->weight = vdelta->download_size;
		}
		FREE(fpath);

		v->data = vdelta;
		vertices = alam_list_add(vertices, v);
	}

	/* compute the edges */
	for(i = vertices; i; i = i->next) {
		pmgraph_t *v_i = i->data;
		amdelta_t *d_i = v_i->data;
		/* loop a second time so we make all possible comparisons */
		for(j = vertices; j; j = j->next) {
			pmgraph_t *v_j = j->data;
			amdelta_t *d_j = v_j->data;
			/* We want to create a delta tree like the following:
			 *          1_to_2
			 *            |
			 * 1_to_3   2_to_3
			 *   \        /
			 *     3_to_4
			 * If J 'from' is equal to I 'to', then J is a child of I.
			 * */
			if(strcmp(d_j->from, d_i->to) == 0) {
				v_i->children = alam_list_add(v_i->children, v_j);
			}
		}
		v_i->childptr = v_i->children;
	}
	return(vertices);
}

static off_t delta_vert(alam_list_t *vertices,
		const char *to, alam_list_t **path) {
	alam_list_t *i;
	pmgraph_t *v;
	while(1) {
		v = NULL;
		/* find the smallest vertice not visited yet */
		for(i = vertices; i; i = i->next) {
			pmgraph_t *v_i = i->data;

			if(v_i->state == -1) {
				continue;
			}

			if(v == NULL || v_i->weight < v->weight) {
				v = v_i;
			}
		}
		if(v == NULL || v->weight == LONG_MAX) {
			break;
		}

		v->state = -1;

		v->childptr = v->children;
		while(v->childptr) {
			pmgraph_t *v_c = v->childptr->data;
			amdelta_t *d_c = v_c->data;
			if(v_c->weight > v->weight + d_c->download_size) {
				v_c->weight = v->weight + d_c->download_size;
				v_c->parent = v;
			}

			v->childptr = (v->childptr)->next;

		}
	}

	v = NULL;
	off_t bestsize = 0;

	for(i = vertices; i; i = i->next) {
		pmgraph_t *v_i = i->data;
		amdelta_t *d_i = v_i->data;

		if(strcmp(d_i->to, to) == 0) {
			if(v == NULL || v_i->weight < v->weight) {
				v = v_i;
				bestsize = v->weight;
			}
		}
	}

	alam_list_t *rpath = NULL;
	while(v != NULL) {
		amdelta_t *vdelta = v->data;
		rpath = alam_list_add(rpath, vdelta);
		v = v->parent;
	}
	*path = alam_list_reverse(rpath);
	alam_list_free(rpath);

	return(bestsize);
}

/** Calculates the shortest path from one version to another.
 * The shortest path is defined as the path with the smallest combined
 * size, not the length of the path.
 * @param deltas the list of amdelta_t * objects that a file has
 * @param to the file to start the search at
 * @param path the pointer to a list location where amdelta_t * objects that
 * have the smallest size are placed. NULL is set if there is no path
 * possible with the files available.
 * @return the size of the path stored, or LONG_MAX if path is unfindable
 */
off_t _alam_shortest_delta_path(alam_list_t *deltas,
		const char *to, alam_list_t **path)
{
	alam_list_t *bestpath = NULL;
	alam_list_t *vertices;
	off_t bestsize = LONG_MAX;

	ALAM_LOG_FUNC;

	if(deltas == NULL) {
		*path = NULL;
		return(bestsize);
	}

	_alam_log(AM_LOG_DEBUG, "started delta shortest-path search for '%s'\n", to);

	vertices = delta_graph_init(deltas);

	bestsize = delta_vert(vertices, to, &bestpath);

	_alam_log(AM_LOG_DEBUG, "delta shortest-path search complete : '%jd'\n", (intmax_t)bestsize);

	alam_list_free_inner(vertices, _alam_graph_free);
	alam_list_free(vertices);

	*path = bestpath;
	return(bestsize);
}

/** Parses the string representation of a amdelta_t object.
 * This function assumes that the string is in the correct format.
 * This format is as follows:
 * $deltafile $deltamd5 $deltasize $oldfile $newfile
 * @param line the string to parse
 * @return A pointer to the new amdelta_t object
 */
/* TODO this does not really belong here, but in a parsing lib */
amdelta_t *_alam_delta_parse(char *line)
{
	amdelta_t *delta;
	char *tmp = line, *tmp2;
	regex_t reg;

	regcomp(&reg,
			"^[^[:space:]]* [[:xdigit:]]{32} [[:digit:]]*"
			" [^[:space:]]* [^[:space:]]*$",
			REG_EXTENDED | REG_NOSUB | REG_NEWLINE);
	if(regexec(&reg, line, 0, 0, 0) != 0) {
		/* delta line is invalid, return NULL */
		regfree(&reg);
		return(NULL);
	}
	regfree(&reg);

	CALLOC(delta, 1, sizeof(amdelta_t), RET_ERR(AM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->delta, tmp2, RET_ERR(AM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->delta_md5, tmp2, RET_ERR(AM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	delta->delta_size = atol(tmp2);

	tmp2 = tmp;
	tmp = strchr(tmp, ' ');
	*(tmp++) = '\0';
	STRDUP(delta->from, tmp2, RET_ERR(AM_ERR_MEMORY, NULL));

	tmp2 = tmp;
	STRDUP(delta->to, tmp2, RET_ERR(AM_ERR_MEMORY, NULL));

	_alam_log(AM_LOG_DEBUG, "delta : %s %s '%lld'\n", delta->from, delta->to, (long long)delta->delta_size);

	return(delta);
}

void _alam_delta_free(amdelta_t *delta)
{
	FREE(delta->from);
	FREE(delta->to);
	FREE(delta->delta);
	FREE(delta->delta_md5);
	FREE(delta);
}

/* vim: set ts=2 sw=2 noet: */
