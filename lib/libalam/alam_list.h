/*
 *  alam_list.h
 *
 *  Copyright (c) 2009 Pacman Development Team <pacman-dev@archlinux.org>
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
#ifndef _ALAM_LIST_H
#define _ALAM_LIST_H

#include <stdlib.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Linked list type used by libalam.
 *
 * It is exposed so front ends can use it to prevent the need to reimplement
 * lists of their own; however, it is not required that the front end uses
 * it.
 */
typedef struct __alam_list_t {
	/** data held by the list node */
	void *data;
	/** pointer to the previous node */
	struct __alam_list_t *prev;
	/** pointer to the next node */
	struct __alam_list_t *next;
} alam_list_t;

#define FREELIST(p) do { alam_list_free_inner(p, free); alam_list_free(p); p = NULL; } while(0)

typedef void (*alam_list_fn_free)(void *); /* item deallocation callback */
typedef int (*alam_list_fn_cmp)(const void *, const void *); /* item comparison callback */

/* allocation */
void alam_list_free(alam_list_t *list);
void alam_list_free_inner(alam_list_t *list, alam_list_fn_free fn);

/* item mutators */
alam_list_t *alam_list_add(alam_list_t *list, void *data);
alam_list_t *alam_list_add_sorted(alam_list_t *list, void *data, alam_list_fn_cmp fn);
alam_list_t *alam_list_join(alam_list_t *first, alam_list_t *second);
alam_list_t *alam_list_mmerge(alam_list_t *left, alam_list_t *right, alam_list_fn_cmp fn);
alam_list_t *alam_list_msort(alam_list_t *list, int n, alam_list_fn_cmp fn);
alam_list_t *alam_list_remove(alam_list_t *haystack, const void *needle, alam_list_fn_cmp fn, void **data);
alam_list_t *alam_list_remove_str(alam_list_t *haystack, const char *needle, char **data);
alam_list_t *alam_list_remove_dupes(const alam_list_t *list);
alam_list_t *alam_list_strdup(const alam_list_t *list);
alam_list_t *alam_list_copy(const alam_list_t *list);
alam_list_t *alam_list_copy_data(const alam_list_t *list, size_t size);
alam_list_t *alam_list_reverse(alam_list_t *list);

/* item accessors */
alam_list_t *alam_list_first(const alam_list_t *list);
alam_list_t *alam_list_nth(const alam_list_t *list, int n);
alam_list_t *alam_list_next(const alam_list_t *list);
alam_list_t *alam_list_last(const alam_list_t *list);
void *alam_list_getdata(const alam_list_t *entry);

/* misc */
int alam_list_count(const alam_list_t *list);
void *alam_list_find(const alam_list_t *haystack, const void *needle, alam_list_fn_cmp fn);
void *alam_list_find_ptr(const alam_list_t *haystack, const void *needle);
char *alam_list_find_str(const alam_list_t *haystack, const char *needle);
alam_list_t *alam_list_diff(const alam_list_t *lhs, const alam_list_t *rhs, alam_list_fn_cmp fn);

#ifdef __cplusplus
}
#endif
#endif /* _ALAM_LIST_H */

/* vim: set ts=2 sw=2 noet: */
