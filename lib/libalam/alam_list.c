/*
 *  alam_list.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* libalam */
#include "alam_list.h"

/* check exported library symbols with: nm -C -D <lib> */
#define SYMEXPORT __attribute__((visibility("default")))
#define SYMHIDDEN __attribute__((visibility("internal")))

//------------------------------------------------------------------------------
/**
 * @addtogroup alam_list List Functions
 * @brief Functions to manipulate alam_list_t lists.
 *
 * These functions are designed to create, destroy, and modify lists of
 * type alam_list_t. This is an internal list type used by libalam that is
 * publicly exposed for use by frontends if desired.
 *
 * @{ */

/* Allocation */

/**
 * @brief Free a list, but not the contained data.
 *
 * @param list the list to free
 */
void SYMEXPORT alam_list_free(alam_list_t *list)
{
	alam_list_t *it = list;

	while(it) {
		alam_list_t *tmp = it->next;
		free(it);
		it = tmp;
	}
}

//------------------------------------------------------------------------------
/**
 * @brief Free the internal data of a list structure.
 *
 * @param list the list to free
 * @param fn   a free function for the internal data
 */
void SYMEXPORT alam_list_free_inner(alam_list_t *list, alam_list_fn_free fn)
{
	alam_list_t *it = list;

	while(it) {
		if(fn && it->data) {
			fn(it->data);
		}
		it = it->next;
	}
}

//------------------------------------------------------------------
/* Mutators */

/**
 * @brief Add a new item to the end of the list.
 *
 * @param list the list to add to
 * @param data the new item to be added to the list
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_add(alam_list_t *list, void *data)
{
	alam_list_t *ptr;
	alam_list_t *lp;

	ptr = calloc(1, sizeof(alam_list_t));
	if(ptr == NULL) {
		return(list);
	}

	ptr->data = data;
	ptr->next = NULL;

	/* Special case: the input list is empty */
	if(list == NULL) {
		ptr->prev = ptr;
		return(ptr);
	}

	lp = alam_list_last(list);
	lp->next = ptr;
	ptr->prev = lp;
	list->prev = ptr;

	return(list);
}

//------------------------------------------------------------------------------
/**
 * @brief Add items to a list in sorted order.
 *
 * @param list the list to add to
 * @param data the new item to be added to the list
 * @param fn   the comparison function to use to determine order
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_add_sorted(alam_list_t *list, void *data, alam_list_fn_cmp fn)
{
	if(!fn || !list) {
		return(alam_list_add(list, data));
	} else {
		alam_list_t *add = NULL, *prev = NULL, *next = list;

		add = calloc(1, sizeof(alam_list_t));
		if(add == NULL) {
			return(list);
		}
		add->data = data;

		/* Find insertion point. */
		while(next) {
			if(fn(add->data, next->data) <= 0) break;
			prev = next;
			next = next->next;
		}

		/* Insert the add node to the list */
		if(prev == NULL) { /* special case: we insert add as the first element */
			add->prev = list->prev; /* list != NULL */
			add->next = list;
			list->prev = add;
			return(add);
		} else if(next == NULL) { /* another special case: add last element */
			add->prev = prev;
			add->next = NULL;
			prev->next = add;
			list->prev = add;
			return(list);
		} else {
			add->prev = prev;
			add->next = next;
			next->prev = add;
			prev->next = add;
			return(list);
		}
	}
}

//------------------------------------------------------------------------------
/**
 * @brief Join two lists.
 * The two lists must be independent. Do not free the original lists after
 * calling this function, as this is not a copy operation. The list pointers
 * passed in should be considered invalid after calling this function.
 *
 * @param first  the first list
 * @param second the second list
 *
 * @return the resultant joined list
 */
alam_list_t SYMEXPORT *alam_list_join(alam_list_t *first, alam_list_t *second)
{
	alam_list_t *tmp;

	if (first == NULL) {
		return(second);
	}
	if (second == NULL) {
		return(first);
	}
	/* tmp is the last element of the first list */
	tmp = first->prev;
	/* link the first list to the second */
	tmp->next = second;
	/* link the second list to the first */
	first->prev = second->prev;
	/* set the back reference to the tail */
	second->prev = tmp;

	return(first);
}

//------------------------------------------------------------------------------
/**
 * @brief Merge the two sorted sublists into one sorted list.
 *
 * @param left  the first list
 * @param right the second list
 * @param fn    comparison function for determining merge order
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_mmerge(alam_list_t *left, alam_list_t *right, alam_list_fn_cmp fn)
{
	alam_list_t *newlist, *lp;

	if (left == NULL)
		return right;
	if (right == NULL)
		return left;

	if (fn(left->data, right->data) <= 0) {
		newlist = left;
		left = left->next;
	}
	else {
		newlist = right;
		right = right->next;
	}
	newlist->prev = NULL;
	newlist->next = NULL;
	lp = newlist;

	while ((left != NULL) && (right != NULL)) {
		if (fn(left->data, right->data) <= 0) {
			lp->next = left;
			left->prev = lp;
			left = left->next;
		}
		else {
			lp->next = right;
			right->prev = lp;
			right = right->next;
		}
		lp = lp->next;
		lp->next = NULL;
	}
	if (left != NULL) {
		lp->next = left;
		left->prev = lp;
	}
	else if (right != NULL) {
		lp->next = right;
		right->prev = lp;
	}

	/* Find our tail pointer
	 * TODO maintain this in the algorithm itself */
	lp = newlist;
	while(lp && lp->next) {
		lp = lp->next;
	}
	newlist->prev = lp;

	return(newlist);
}

//------------------------------------------------------------------------------
/**
 * @brief Sort a list of size `n` using mergesort algorithm.
 *
 * @param list the list to sort
 * @param n    the size of the list
 * @param fn   the comparison function for determining order
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_msort(alam_list_t *list, int n, alam_list_fn_cmp fn)
{
	if (n > 1) {
		alam_list_t *left = list;
		alam_list_t *lastleft = alam_list_nth(list, n/2 - 1);
		alam_list_t *right = lastleft->next;
		/* terminate first list */
		lastleft->next = NULL;

		left = alam_list_msort(left, n/2, fn);
		right = alam_list_msort(right, n - (n/2), fn);
		list = alam_list_mmerge(left, right, fn);
	}
	return(list);
}

//------------------------------------------------------------------------------
/**
 * @brief Remove an item from the list.
 *
 * @param haystack the list to remove the item from
 * @param needle   the data member of the item we're removing
 * @param fn       the comparison function for searching
 * @param data     output parameter containing data of the removed item
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_remove(alam_list_t *haystack, const void *needle, alam_list_fn_cmp fn, void **data)
{
	alam_list_t *i = haystack, *tmp = NULL;

	if(data) {
		*data = NULL;
	}

	if(needle == NULL) {
		return(haystack);
	}

	while(i) {
		if(i->data == NULL) {
			continue;
		}
		tmp = i->next;
		if(fn(i->data, needle) == 0) {
			/* we found a matching item */
			if(i == haystack) {
				/* Special case: removing the head node which has a back reference to
				 * the tail node */
				haystack = i->next;
				if(haystack) {
					haystack->prev = i->prev;
				}
				i->prev = NULL;
			} else if(i == haystack->prev) {
				/* Special case: removing the tail node, so we need to fix the back
				 * reference on the head node. We also know tail != head. */
				if(i->prev) {
					/* i->next should always be null */
					i->prev->next = i->next;
					haystack->prev = i->prev;
					i->prev = NULL;
				}
			} else {
				/* Normal case, non-head and non-tail node */
				if(i->next) {
					i->next->prev = i->prev;
				}
				if(i->prev) {
					i->prev->next = i->next;
				}
			}

			if(data) {
				*data = i->data;
			}
			i->data = NULL;
			free(i);
			i = NULL;
		} else {
			i = tmp;
		}
	}

	return(haystack);
}

//------------------------------------------------------------------------------
/**
 * @brief Remove a string from a list.
 *
 * @param haystack the list to remove the item from
 * @param needle   the data member of the item we're removing
 * @param data     output parameter containing data of the removed item
 *
 * @return the resultant list
 */
alam_list_t SYMEXPORT *alam_list_remove_str(alam_list_t *haystack,
		const char *needle, char **data)
{
	return(alam_list_remove(haystack, (const void *)needle,
				(alam_list_fn_cmp)strcmp, (void **)data));
}

//------------------------------------------------------------------------------
/**
 * @brief Create a new list without any duplicates.
 *
 * This does NOT copy data members.
 *
 * @param list the list to copy
 *
 * @return a new list containing non-duplicate items
 */
alam_list_t SYMEXPORT *alam_list_remove_dupes(const alam_list_t *list)
{
	const alam_list_t *lp = list;
	alam_list_t *newlist = NULL;
	while(lp) {
		if(!alam_list_find_ptr(newlist, lp->data)) {
			newlist = alam_list_add(newlist, lp->data);
		}
		lp = lp->next;
	}
	return(newlist);
}

//------------------------------------------------------------------------------
/**
 * @brief Copy a string list, including data.
 *
 * @param list the list to copy
 *
 * @return a copy of the original list
 */
alam_list_t SYMEXPORT *alam_list_strdup(const alam_list_t *list)
{
	const alam_list_t *lp = list;
	alam_list_t *newlist = NULL;
	while(lp) {
		newlist = alam_list_add(newlist, strdup(lp->data));
		lp = lp->next;
	}
	return(newlist);
}

//------------------------------------------------------------------------------
/**
 * @brief Copy a list, without copying data.
 *
 * @param list the list to copy
 *
 * @return a copy of the original list
 */
alam_list_t SYMEXPORT *alam_list_copy(const alam_list_t *list)
{
	const alam_list_t *lp = list;
	alam_list_t *newlist = NULL;
	while(lp) {
		newlist = alam_list_add(newlist, lp->data);
		lp = lp->next;
	}
	return(newlist);
}

//------------------------------------------------------------------------------
/**
 * @brief Copy a list and copy the data.
 * Note that the data elements to be copied should not contain pointers
 * and should also be of constant size.
 *
 * @param list the list to copy
 * @param size the size of each data element
 *
 * @return a copy of the original list, data copied as well
 */
alam_list_t SYMEXPORT *alam_list_copy_data(const alam_list_t *list,
		size_t size)
{
	const alam_list_t *lp = list;
	alam_list_t *newlist = NULL;
	while(lp) {
		void *newdata = calloc(1, size);
		if(newdata) {
			memcpy(newdata, lp->data, size);
			newlist = alam_list_add(newlist, newdata);
			lp = lp->next;
		}
	}
	return(newlist);
}

//------------------------------------------------------------------------------
/**
 * @brief Create a new list in reverse order.
 *
 * @param list the list to copy
 *
 * @return a new list in reverse order
 */
alam_list_t SYMEXPORT *alam_list_reverse(alam_list_t *list)
{
	const alam_list_t *lp;
	alam_list_t *newlist = NULL, *backup;

	if(list == NULL) {
		return(NULL);
	}

	lp = alam_list_last(list);
	/* break our reverse circular list */
	backup = list->prev;
	list->prev = NULL;

	while(lp) {
		newlist = alam_list_add(newlist, lp->data);
		lp = lp->prev;
	}
	list->prev = backup; /* restore tail pointer */
	return(newlist);
}

//------------------------------------------------------------------------------
/* Accessors */

/**
 * @brief Get the first element of a list.
 *
 * @param list the list
 *
 * @return the first element in the list
 */
inline alam_list_t SYMEXPORT *alam_list_first(const alam_list_t *list)
{
	if(list) {
		return((alam_list_t*)list);
	} else {
		return(NULL);
	}
}

//------------------------------------------------------------------------------
/**
 * @brief Return nth element from list (starting from 0).
 *
 * @param list the list
 * @param n    the index of the item to find (n < alam_list_count(list) IS needed)
 *
 * @return an alam_list_t node for index `n`
 */
alam_list_t SYMEXPORT *alam_list_nth(const alam_list_t *list, int n)
{
	const alam_list_t *i = list;
	while(n--) {
		i = i->next;
	}
	return((alam_list_t*)i);
}

//------------------------------------------------------------------------------
/**
 * @brief Get the next element of a list.
 *
 * @param node the list node
 *
 * @return the next element, or NULL when no more elements exist
 */
inline alam_list_t SYMEXPORT *alam_list_next(const alam_list_t *node)
{
	if(node) {
		return(node->next);
	} else {
		return(NULL);
	}
}

//------------------------------------------------------------------------------
/**
 * @brief Get the last item in the list.
 *
 * @param list the list
 *
 * @return the last element in the list
 */
alam_list_t SYMEXPORT *alam_list_last(const alam_list_t *list)
{
	if(list) {
		return(list->prev);
	} else {
		return(NULL);
	}
}

//------------------------------------------------------------------------------
/**
 * @brief Get the data member of a list node.
 *
 * @param node the list node
 *
 * @return the contained data, or NULL if none
 */
void SYMEXPORT *alam_list_getdata(const alam_list_t *node)
{
	if(node == NULL) return(NULL);
	return(node->data);
}


//------------------------------------------------------------------------------
/* Misc */

/**
 * @brief Get the number of items in a list.
 *
 * @param list the list
 *
 * @return the number of list items
 */
int SYMEXPORT alam_list_count(const alam_list_t *list)
{
	unsigned int i = 0;
	const alam_list_t *lp = list;
	while(lp) {
		++i;
		lp = lp->next;
	}
	return(i);
}


//------------------------------------------------------------------------------
/**
 * @brief Find an item in a list.
 *
 * @param needle   the item to search
 * @param haystack the list
 * @param fn       the comparison function for searching (!= NULL)
 *
 * @return `needle` if found, NULL otherwise
 */
void SYMEXPORT *alam_list_find(const alam_list_t *haystack, const void *needle,
		alam_list_fn_cmp fn)
{
	const alam_list_t *lp = haystack;
	while(lp) {
		if(lp->data && fn(lp->data, needle) == 0) {
			return(lp->data);
		}
		lp = lp->next;
	}
	return(NULL);
}


//------------------------------------------------------------------------------
/* trivial helper function for alam_list_find_ptr */
static int ptr_cmp(const void *p, const void *q)
{
	return(p != q);
}

//------------------------------------------------------------------------------
/**
 * @brief Find an item in a list.
 *
 * Search for the item whos data matches that of the `needle`.
 *
 * @param needle   the data to search for (== comparison)
 * @param haystack the list
 *
 * @return `needle` if found, NULL otherwise
 */
void SYMEXPORT *alam_list_find_ptr(const alam_list_t *haystack, const void *needle)
{
	return(alam_list_find(haystack, needle, ptr_cmp));
}

//------------------------------------------------------------------------------
/**
 * @brief Find a string in a list.
 *
 * @param needle   the string to search for
 * @param haystack the list
 *
 * @return `needle` if found, NULL otherwise
 */
char SYMEXPORT *alam_list_find_str(const alam_list_t *haystack,
		const char *needle)
{
	return((char *)alam_list_find(haystack, (const void*)needle,
				(alam_list_fn_cmp)strcmp));
}

//------------------------------------------------------------------------------
/**
 * @brief Find the items in list `lhs` that are not present in list `rhs`.
 *
 * Entries are not duplicated. Operation is O(m*n). The first list is stepped
 * through one node at a time, and for each node in the first list, each node
 * in the second list is compared to it.
 *
 * @param lhs the first list
 * @param rhs the second list
 * @param fn  the comparison function
 *
 * @return a list containing all items in `lhs` not present in `rhs`
 */
alam_list_t SYMEXPORT *alam_list_diff(const alam_list_t *lhs,
		const alam_list_t *rhs, alam_list_fn_cmp fn)
{
	const alam_list_t *i, *j;
	alam_list_t *ret = NULL;
	for(i = lhs; i; i = i->next) {
		int found = 0;
		for(j = rhs; j; j = j->next) {
			if(fn(i->data, j->data) == 0) {
				found = 1;
				break;
			}
		}
		if(!found) {
			ret = alam_list_add(ret, i->data);
		}
	}

	return(ret);
}

/** @} */

/* vim: set ts=2 sw=2 noet: */
