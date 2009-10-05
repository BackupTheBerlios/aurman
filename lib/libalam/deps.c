/*
 *  deps.c
 *
 *  Copyright (c) 2006-2009 Pacman Development Team <pacman-dev@archlinux.org>
 *  Copyright (c) 2002-2006 by Judd Vinet <jvinet@zeroflux.org>
 *  Copyright (c) 2005 by Aurelien Foret <orelien@chez.com>
 *  Copyright (c) 2005, 2006 by Miklos Vajna <vmiklos@frugalware.org>
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
#include <stdio.h>
#include <string.h>

/* libalam */
#include "deps.h"
#include "alam_list.h"
#include "util.h"
#include "log.h"
#include "graph.h"
#include "package.h"
#include "db.h"
#include "cache.h"
#include "handle.h"

void _alam_dep_free(amdepend_t *dep)
{
	FREE(dep->name);
	FREE(dep->version);
	FREE(dep);
}

amdepmissing_t *_alam_depmiss_new(const char *target, amdepend_t *dep,
		const char *causingpkg)
{
	amdepmissing_t *miss;

	ALAM_LOG_FUNC;

	MALLOC(miss, sizeof(amdepmissing_t), RET_ERR(AM_ERR_MEMORY, NULL));

	STRDUP(miss->target, target, RET_ERR(AM_ERR_MEMORY, NULL));
	miss->depend = _alam_dep_dup(dep);
	STRDUP(miss->causingpkg, causingpkg, RET_ERR(AM_ERR_MEMORY, NULL));

	return(miss);
}

void _alam_depmiss_free(amdepmissing_t *miss)
{
	_alam_dep_free(miss->depend);
	FREE(miss->target);
	FREE(miss->causingpkg);
	FREE(miss);
}

/* Convert a list of ampkg_t * to a graph structure,
 * with a edge for each dependency.
 * Returns a list of vertices (one vertex = one package)
 * (used by alam_sortbydeps)
 */
static alam_list_t *dep_graph_init(alam_list_t *targets)
{
	alam_list_t *i, *j;
	alam_list_t *vertices = NULL;
	/* We create the vertices */
	for(i = targets; i; i = i->next) {
		pmgraph_t *vertex = _alam_graph_new();
		vertex->data = (void *)i->data;
		vertices = alam_list_add(vertices, vertex);
	}

	/* We compute the edges */
	for(i = vertices; i; i = i->next) {
		pmgraph_t *vertex_i = i->data;
		ampkg_t *p_i = vertex_i->data;
		/* TODO this should be somehow combined with alam_checkdeps */
		for(j = vertices; j; j = j->next) {
			pmgraph_t *vertex_j = j->data;
			ampkg_t *p_j = vertex_j->data;
			if(_alam_dep_edge(p_i, p_j)) {
				vertex_i->children =
					alam_list_add(vertex_i->children, vertex_j);
			}
		}
		vertex_i->childptr = vertex_i->children;
	}
	return(vertices);
}

/* Re-order a list of target packages with respect to their dependencies.
 *
 * Example (reverse == 0):
 *   A depends on C
 *   B depends on A
 *   Target order is A,B,C,D
 *
 *   Should be re-ordered to C,A,B,D
 *
 * if reverse is > 0, the dependency order will be reversed.
 *
 * This function returns the new alam_list_t* target list.
 *
 */
alam_list_t *_alam_sortbydeps(alam_list_t *targets, int reverse)
{
	alam_list_t *newtargs = NULL;
	alam_list_t *vertices = NULL;
	alam_list_t *vptr;
	pmgraph_t *vertex;

	ALAM_LOG_FUNC;

	if(targets == NULL) {
		return(NULL);
	}

	_alam_log(AM_LOG_DEBUG, "started sorting dependencies\n");

	vertices = dep_graph_init(targets);

	vptr = vertices;
	vertex = vertices->data;
	while(vptr) {
		/* mark that we touched the vertex */
		vertex->state = -1;
		int found = 0;
		while(vertex->childptr && !found) {
			pmgraph_t *nextchild = (vertex->childptr)->data;
			vertex->childptr = (vertex->childptr)->next;
			if (nextchild->state == 0) {
				found = 1;
				nextchild->parent = vertex;
				vertex = nextchild;
			}
			else if(nextchild->state == -1) {
				ampkg_t *vertexpkg = vertex->data;
				ampkg_t *childpkg = nextchild->data;
				_alam_log(AM_LOG_WARNING, _("dependency cycle detected:\n"));
				if(reverse) {
					_alam_log(AM_LOG_WARNING, _("%s will be removed after its %s dependency\n"), vertexpkg->name, childpkg->name);
				} else {
					_alam_log(AM_LOG_WARNING, _("%s will be installed before its %s dependency\n"), vertexpkg->name, childpkg->name);
				}
			}
		}
		if(!found) {
			newtargs = alam_list_add(newtargs, vertex->data);
			/* mark that we've left this vertex */
			vertex->state = 1;
			vertex = vertex->parent;
			if(!vertex) {
				vptr = vptr->next;
				while(vptr) {
					vertex = vptr->data;
					if (vertex->state == 0) break;
					vptr = vptr->next;
				}
			}
		}
	}

	_alam_log(AM_LOG_DEBUG, "sorting dependencies finished\n");

	if(reverse) {
		/* reverse the order */
		alam_list_t *tmptargs = alam_list_reverse(newtargs);
		/* free the old one */
		alam_list_free(newtargs);
		newtargs = tmptargs;
	}

	alam_list_free_inner(vertices, _alam_graph_free);
	alam_list_free(vertices);

	return(newtargs);
}

ampkg_t *_alam_find_dep_satisfier(alam_list_t *pkgs, amdepend_t *dep)
{
	alam_list_t *i;

	for(i = pkgs; i; i = alam_list_next(i)) {
		ampkg_t *pkg = i->data;
		if(alam_depcmp(pkg, dep)) {
			return(pkg);
		}
	}
	return(NULL);
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param db pointer to the local package database
 * @param targets an alam_list_t* of dependencies strings to satisfy
 * @return an alam_list_t* of missing dependencies strings
 */
alam_list_t SYMEXPORT *alam_deptest(amdb_t *db, alam_list_t *targets)
{
	alam_list_t *i, *ret = NULL;

	for(i = targets; i; i = alam_list_next(i)) {
		amdepend_t *dep;
		char *target;

		target = alam_list_getdata(i);
		dep = _alam_splitdep(target);

		if(!_alam_find_dep_satisfier(_alam_db_get_pkgcache(db), dep)) {
			ret = alam_list_add(ret, target);
		}
		_alam_dep_free(dep);
	}
	return(ret);
}

/** Checks dependencies and returns missing ones in a list.
 * Dependencies can include versions with depmod operators.
 * @param pkglist the list of local packages
 * @param reversedeps handles the backward dependencies
 * @param remove an alam_list_t* of packages to be removed
 * @param upgrade an alam_list_t* of packages to be upgraded (remove-then-upgrade)
 * @return an alam_list_t* of ampkg_t* of missing_t pointers.
 */
alam_list_t SYMEXPORT *alam_checkdeps(alam_list_t *pkglist, int reversedeps,
		alam_list_t *remove, alam_list_t *upgrade)
{
	alam_list_t *i, *j;
	alam_list_t *targets, *dblist = NULL, *modified = NULL;
	alam_list_t *baddeps = NULL;
	amdepmissing_t *miss = NULL;

	ALAM_LOG_FUNC;

	targets = alam_list_join(alam_list_copy(remove), alam_list_copy(upgrade));
	for(i = pkglist; i; i = i->next) {
		void *pkg = i->data;
		if(alam_list_find(targets, pkg, _alam_pkg_cmp)) {
			modified = alam_list_add(modified, pkg);
		} else {
			dblist = alam_list_add(dblist, pkg);
		}
	}
	alam_list_free(targets);

	/* look for unsatisfied dependencies of the upgrade list */
	for(i = upgrade; i; i = i->next) {
		ampkg_t *tp = i->data;
		_alam_log(AM_LOG_DEBUG, "checkdeps: package %s-%s\n",
				alam_pkg_get_name(tp), alam_pkg_get_version(tp));

		for(j = alam_pkg_get_depends(tp); j; j = j->next) {
			amdepend_t *depend = j->data;
			/* 1. we check the upgrade list */
			/* 2. we check database for untouched satisfying packages */
			if(!_alam_find_dep_satisfier(upgrade, depend) &&
			   !_alam_find_dep_satisfier(dblist, depend)) {
				/* Unsatisfied dependency in the upgrade list */
				char *missdepstring = alam_dep_compute_string(depend);
				_alam_log(AM_LOG_DEBUG, "checkdeps: missing dependency '%s' for package '%s'\n",
						missdepstring, alam_pkg_get_name(tp));
				free(missdepstring);
				miss = _alam_depmiss_new(alam_pkg_get_name(tp), depend, NULL);
				baddeps = alam_list_add(baddeps, miss);
			}
		}
	}

	if(reversedeps) {
		/* reversedeps handles the backwards dependencies, ie,
		 * the packages listed in the requiredby field. */
		for(i = dblist; i; i = i->next) {
			ampkg_t *lp = i->data;
			for(j = alam_pkg_get_depends(lp); j; j = j->next) {
				amdepend_t *depend = j->data;
				ampkg_t *causingpkg = _alam_find_dep_satisfier(modified, depend);
				/* we won't break this depend, if it is already broken, we ignore it */
				/* 1. check upgrade list for satisfiers */
				/* 2. check dblist for satisfiers */
				if(causingpkg &&
				   !_alam_find_dep_satisfier(upgrade, depend) &&
				   !_alam_find_dep_satisfier(dblist, depend)) {
					char *missdepstring = alam_dep_compute_string(depend);
					_alam_log(AM_LOG_DEBUG, "checkdeps: transaction would break '%s' dependency of '%s'\n",
							missdepstring, alam_pkg_get_name(lp));
					free(missdepstring);
					miss = _alam_depmiss_new(lp->name, depend, alam_pkg_get_name(causingpkg));
					baddeps = alam_list_add(baddeps, miss);
				}
			}
		}
	}
	alam_list_free(modified);
	alam_list_free(dblist);

	return(baddeps);
}

static int dep_vercmp(const char *version1, amdepmod_t mod,
		const char *version2)
{
	int equal = 0;

	if(mod == AM_DEP_MOD_ANY) {
		equal = 1;
	} else {
		int cmp = alam_pkg_vercmp(version1, version2);
		switch(mod) {
			case AM_DEP_MOD_EQ: equal = (cmp == 0); break;
			case AM_DEP_MOD_GE: equal = (cmp >= 0); break;
			case AM_DEP_MOD_LE: equal = (cmp <= 0); break;
			case AM_DEP_MOD_LT: equal = (cmp < 0); break;
			case AM_DEP_MOD_GT: equal = (cmp > 0); break;
			default: equal = 1; break;
		}
	}
	return(equal);
}

int SYMEXPORT alam_depcmp(ampkg_t *pkg, amdepend_t *dep)
{
	alam_list_t *i;

	ALAM_LOG_FUNC;

	const char *pkgname = alam_pkg_get_name(pkg);
	const char *pkgversion = alam_pkg_get_version(pkg);
	int satisfy = 0;

	/* check (pkg->name, pkg->version) */
	satisfy = (strcmp(pkgname, dep->name) == 0
			&& dep_vercmp(pkgversion, dep->mod, dep->version));

	/* check provisions, format : "name=version" */
	for(i = alam_pkg_get_provides(pkg); i && !satisfy; i = i->next) {
		char *provname = strdup(i->data);
		char *provver = strchr(provname, '=');

		if(provver == NULL) { /* no provision version */
			satisfy = (dep->mod == AM_DEP_MOD_ANY
					&& strcmp(provname, dep->name) == 0);
		} else {
			*provver = '\0';
			provver += 1;
			satisfy = (strcmp(provname, dep->name) == 0
					&& dep_vercmp(provver, dep->mod, dep->version));
		}
		free(provname);
	}

	return(satisfy);
}

amdepend_t *_alam_splitdep(const char *depstring)
{
	amdepend_t *depend;
	char *ptr = NULL;
	char *newstr = NULL;

	if(depstring == NULL) {
		return(NULL);
	}
	STRDUP(newstr, depstring, RET_ERR(AM_ERR_MEMORY, NULL));

	CALLOC(depend, 1, sizeof(amdepend_t), RET_ERR(AM_ERR_MEMORY, NULL));

	/* Find a version comparator if one exists. If it does, set the type and
	 * increment the ptr accordingly so we can copy the right strings. */
	if((ptr = strstr(newstr, ">="))) {
		depend->mod = AM_DEP_MOD_GE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(newstr, "<="))) {
		depend->mod = AM_DEP_MOD_LE;
		*ptr = '\0';
		ptr += 2;
	} else if((ptr = strstr(newstr, "="))) { /* Note: we must do =,<,> checks after <=, >= checks */
		depend->mod = AM_DEP_MOD_EQ;
		*ptr = '\0';
		ptr += 1;
	} else if((ptr = strstr(newstr, "<"))) {
		depend->mod = AM_DEP_MOD_LT;
		*ptr = '\0';
		ptr += 1;
	} else if((ptr = strstr(newstr, ">"))) {
		depend->mod = AM_DEP_MOD_GT;
		*ptr = '\0';
		ptr += 1;
	} else {
		/* no version specified - copy the name and return it */
		depend->mod = AM_DEP_MOD_ANY;
		STRDUP(depend->name, newstr, RET_ERR(AM_ERR_MEMORY, NULL));
		depend->version = NULL;
		free(newstr);
		return(depend);
	}

	/* if we get here, we have a version comparator, copy the right parts
	 * to the right places */
	STRDUP(depend->name, newstr, RET_ERR(AM_ERR_MEMORY, NULL));
	STRDUP(depend->version, ptr, RET_ERR(AM_ERR_MEMORY, NULL));
	free(newstr);

	return(depend);
}

amdepend_t *_alam_dep_dup(const amdepend_t *dep)
{
	amdepend_t *newdep;
	CALLOC(newdep, 1, sizeof(amdepend_t), RET_ERR(AM_ERR_MEMORY, NULL));

	STRDUP(newdep->name, dep->name, RET_ERR(AM_ERR_MEMORY, NULL));
	STRDUP(newdep->version, dep->version, RET_ERR(AM_ERR_MEMORY, NULL));
	newdep->mod = dep->mod;

	return(newdep);
}

/* These parameters are messy. We check if this package, given a list of
 * targets and a db is safe to remove. We do NOT remove it if it is in the
 * target list, or if if the package was explictly installed and
 * include_explicit == 0 */
static int can_remove_package(amdb_t *db, ampkg_t *pkg, alam_list_t *targets,
		int include_explicit)
{
	alam_list_t *i;

	if(_alam_pkg_find(targets, alam_pkg_get_name(pkg))) {
		return(0);
	}

	if(!include_explicit) {
		/* see if it was explicitly installed */
		if(alam_pkg_get_reason(pkg) == AM_PKG_REASON_EXPLICIT) {
			_alam_log(AM_LOG_DEBUG, "excluding %s -- explicitly installed\n",
					alam_pkg_get_name(pkg));
			return(0);
		}
	}

	/* TODO: checkdeps could be used here, it handles multiple providers
	 * better, but that also makes it slower.
	 * Also this would require to first add the package to the targets list,
	 * then call checkdeps with it, then remove the package from the targets list
	 * if checkdeps detected it would break something */

	/* see if other packages need it */
	for(i = _alam_db_get_pkgcache(db); i; i = i->next) {
		ampkg_t *lpkg = i->data;
		if(_alam_dep_edge(lpkg, pkg) && !_alam_pkg_find(targets, lpkg->name)) {
			return(0);
		}
	}

	/* it's ok to remove */
	return(1);
}

/**
 * @brief Adds unneeded dependencies to an existing list of packages.
 * By unneeded, we mean dependencies that are only required by packages in the
 * target list, so they can be safely removed.
 * If the input list was topo sorted, the output list will be topo sorted too.
 *
 * @param db package database to do dependency tracing in
 * @param *targs pointer to a list of packages
 * @param include_explicit if 0, explicitly installed packages are not included
 */
void _alam_recursedeps(amdb_t *db, alam_list_t *targs, int include_explicit)
{
	alam_list_t *i, *j;

	ALAM_LOG_FUNC;

	if(db == NULL || targs == NULL) {
		return;
	}

	for(i = targs; i; i = i->next) {
		ampkg_t *pkg = i->data;
		for(j = _alam_db_get_pkgcache(db); j; j = j->next) {
			ampkg_t *deppkg = j->data;
			if(_alam_dep_edge(pkg, deppkg)
					&& can_remove_package(db, deppkg, targs, include_explicit)) {
				_alam_log(AM_LOG_DEBUG, "adding '%s' to the targets\n",
						alam_pkg_get_name(deppkg));
				/* add it to the target list */
				targs = alam_list_add(targs, _alam_pkg_dup(deppkg));
			}
		}
	}
}

/**
 * helper function for resolvedeps: search for dep satisfier in dbs
 *
 * @param dep is the dependency to search for
 * @param dbs are the databases to search
 * @param excluding are the packages to exclude from the search
 * @param prompt if true, will cause an unresolvable dependency to issue an
 *        interactive prompt asking whether the package should be removed from
 *        the transaction or the transaction aborted; if false, simply returns
 *        an error code without prompting
 * @return the resolved package
 **/
ampkg_t *_alam_resolvedep(amdepend_t *dep, alam_list_t *dbs,
		alam_list_t *excluding, int prompt)
{
	alam_list_t *i, *j;
	int ignored = 0;
	/* 1. literals */
	for(i = dbs; i; i = i->next) {
		ampkg_t *pkg = _alam_db_get_pkgfromcache(i->data, dep->name);
		if(pkg && alam_depcmp(pkg, dep) && !_alam_pkg_find(excluding, pkg->name)) {
			if(_alam_pkg_should_ignore(pkg)) {
				int install = 0;
				if (prompt) {
					QUESTION(handle->trans, AM_TRANS_CONV_INSTALL_IGNOREPKG, pkg,
							 NULL, NULL, &install);
				} else {
					_alam_log(AM_LOG_WARNING, _("ignoring package %s-%s\n"), pkg->name, pkg->version);
				}
				if(!install) {
					ignored = 1;
					continue;
				}
			}
			return(pkg);
		}
	}
	/* 2. satisfiers (skip literals here) */
	for(i = dbs; i; i = i->next) {
		for(j = _alam_db_get_pkgcache(i->data); j; j = j->next) {
			ampkg_t *pkg = j->data;
			if(alam_depcmp(pkg, dep) && strcmp(pkg->name, dep->name) &&
			             !_alam_pkg_find(excluding, pkg->name)) {
				if(_alam_pkg_should_ignore(pkg)) {
					int install = 0;
					if (prompt) {
						QUESTION(handle->trans, AM_TRANS_CONV_INSTALL_IGNOREPKG,
									pkg, NULL, NULL, &install);
					} else {
						_alam_log(AM_LOG_WARNING, _("ignoring package %s-%s\n"), pkg->name, pkg->version);
					}
					if(!install) {
						ignored = 1;
						continue;
					}
				}
				_alam_log(AM_LOG_WARNING, _("provider package was selected (%s provides %s)\n"),
				                         pkg->name, dep->name);
				return(pkg);
			}
		}
	}
	if(ignored) { /* resolvedeps will override these */
		am_errno = AM_ERR_PKG_IGNORED;
	} else {
		am_errno = AM_ERR_PKG_NOT_FOUND;
	}
	return(NULL);
}

/* Computes resolvable dependencies for a given package and adds that package
 * and those resolvable dependencies to a list.
 *
 * @param local is the local database
 * @param dbs_sync are the sync databases
 * @param pkg is the package to resolve
 * @param packages is a pointer to a list of packages which will be
 *        searched first for any dependency packages needed to complete the
 *        resolve, and to which will be added any [pkg] and all of its
 *        dependencies not already on the list
 * @param remove is the set of packages which will be removed in this
 *        transaction
 * @param data returns the dependency which could not be satisfied in the
 *        event of an error
 * @return 0 on success, with [pkg] and all of its dependencies not already on
 *         the [*packages] list added to that list, or -1 on failure due to an
 *         unresolvable dependency, in which case the [*packages] list will be
 *         unmodified by this function
 */
int _alam_resolvedeps(amdb_t *local, alam_list_t *dbs_sync, ampkg_t *pkg,
                      alam_list_t *preferred, alam_list_t **packages,
                      alam_list_t *remove, alam_list_t **data)
{
	alam_list_t *i, *j;
	alam_list_t *targ;
	alam_list_t *deps = NULL;
	alam_list_t *packages_copy;

	ALAM_LOG_FUNC;

	if(local == NULL) {
		return(-1);
	}

	if(_alam_pkg_find(*packages, pkg->name) != NULL) {
		return(0);
	}

	/* Create a copy of the packages list, so that it can be restored
	   on error */
	packages_copy = alam_list_copy(*packages);
	/* [pkg] has not already been resolved into the packages list, so put it
	   on that list */
	*packages = alam_list_add(*packages, pkg);

	_alam_log(AM_LOG_DEBUG, "started resolving dependencies\n");
	for(i = alam_list_last(*packages); i; i = i->next) {
		ampkg_t *tpkg = i->data;
		targ = alam_list_add(NULL, tpkg);
		deps = alam_checkdeps(_alam_db_get_pkgcache(local), 0, remove, targ);
		alam_list_free(targ);
		for(j = deps; j; j = j->next) {
			amdepmissing_t *miss = j->data;
			amdepend_t *missdep = alam_miss_get_dep(miss);
			/* check if one of the packages in the [*packages] list already satisfies this dependency */
			if(_alam_find_dep_satisfier(*packages, missdep)) {
				continue;
			}
			/* check if one of the packages in the [preferred] list already satisfies this dependency */
			ampkg_t *spkg = _alam_find_dep_satisfier(preferred, missdep);
			if(!spkg) {
				/* find a satisfier package in the given repositories */
				spkg = _alam_resolvedep(missdep, dbs_sync, *packages, 0);
			}
			if(!spkg) {
				am_errno = AM_ERR_UNSATISFIED_DEPS;
				char *missdepstring = alam_dep_compute_string(missdep);
				_alam_log(AM_LOG_WARNING, _("cannot resolve \"%s\", a dependency of \"%s\"\n"),
						missdepstring, tpkg->name);
				free(missdepstring);
				if(data) {
					amdepmissing_t *missd = _alam_depmiss_new(miss->target,
							miss->depend, miss->causingpkg);
					if(missd) {
						*data = alam_list_add(*data, missd);
					}
				}
				alam_list_free(*packages);
				*packages = packages_copy;
				alam_list_free_inner(deps, (alam_list_fn_free)_alam_depmiss_free);
				alam_list_free(deps);
				return(-1);
			} else {
				_alam_log(AM_LOG_DEBUG, "pulling dependency %s (needed by %s)\n",
						alam_pkg_get_name(spkg), alam_pkg_get_name(tpkg));
				*packages = alam_list_add(*packages, spkg);
			}
		}
		alam_list_free_inner(deps, (alam_list_fn_free)_alam_depmiss_free);
		alam_list_free(deps);
	}
	alam_list_free(packages_copy);
	_alam_log(AM_LOG_DEBUG, "finished resolving dependencies\n");
	return(0);
}

/* Does pkg1 depend on pkg2, ie. does pkg2 satisfy a dependency of pkg1? */
int _alam_dep_edge(ampkg_t *pkg1, ampkg_t *pkg2)
{
	alam_list_t *i;
	for(i = alam_pkg_get_depends(pkg1); i; i = i->next) {
		if(alam_depcmp(pkg2, i->data)) {
			return(1);
		}
	}
	return(0);
}

const char SYMEXPORT *alam_miss_get_target(const amdepmissing_t *miss)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return(miss->target);
}

const char SYMEXPORT *alam_miss_get_causingpkg(const amdepmissing_t *miss)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return miss->causingpkg;
}

amdepend_t SYMEXPORT *alam_miss_get_dep(amdepmissing_t *miss)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(miss != NULL, return(NULL));

	return(miss->depend);
}

amdepmod_t SYMEXPORT alam_dep_get_mod(const amdepend_t *dep)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(-1));

	return(dep->mod);
}

const char SYMEXPORT *alam_dep_get_name(const amdepend_t *dep)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	return(dep->name);
}

const char SYMEXPORT *alam_dep_get_version(const amdepend_t *dep)
{
	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	return(dep->version);
}

/** Reverse of splitdep; make a dep string from a amdepend_t struct.
 * The string must be freed!
 * @param dep the depend to turn into a string
 * @return a string-formatted dependency with operator if necessary
 */
char SYMEXPORT *alam_dep_compute_string(const amdepend_t *dep)
{
	char *name, *opr, *ver, *str = NULL;
	size_t len;

	ALAM_LOG_FUNC;

	/* Sanity checks */
	ASSERT(dep != NULL, return(NULL));

	if(dep->name) {
		name = dep->name;
	} else {
		name = "";
	}

	switch(dep->mod) {
		case AM_DEP_MOD_ANY:
			opr = "";
			break;
		case AM_DEP_MOD_GE:
			opr = ">=";
			break;
		case AM_DEP_MOD_LE:
			opr = "<=";
			break;
		case AM_DEP_MOD_EQ:
			opr = "=";
			break;
		case AM_DEP_MOD_LT:
			opr = "<";
			break;
		case AM_DEP_MOD_GT:
			opr = ">";
			break;
		default:
			opr = "";
			break;
	}

	if(dep->version) {
		ver = dep->version;
	} else {
		ver = "";
	}

	/* we can always compute len and print the string like this because opr
	 * and ver will be empty when AM_DEP_MOD_ANY is the depend type. the
	 * reassignments above also ensure we do not do a strlen(NULL). */
	len = strlen(name) + strlen(opr) + strlen(ver) + 1;
	MALLOC(str, len, RET_ERR(AM_ERR_MEMORY, NULL));
	snprintf(str, len, "%s%s%s", name, opr, ver);

	return(str);
}
/* vim: set ts=2 sw=2 noet: */
