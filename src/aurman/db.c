#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include "db.h"
#include "util.h"

char AM_DB_NULL_STR[1] = {'\0'};

/**
 * @brief generate a hash of \cstr, this aligns with the precomputed
 *  hash values for an am_db_tag
 */
am_db_tag_t am_db_tag_gen(const char *str)
{
	unsigned int hash = 0;
	while (*str && *str != '%') hash += *str++ | hash;
	return hash;
}

/**
 * @brief analogous to strndup but returns AM_DB_NULL_STR on failure
 * @note fails states are memory allocation failure, str is NULL or "", len == 0
 */
char *am_db_strdup(const char *str, size_t len)
{
	char *s;
	if (str && *str && len > 0) {
		s = malloc(len + 1);
		if (s) {
			s[len] = '\0';
			return memcpy(s, str, len);
		}
	}
	return AM_DB_NULL_STR;
}

/**
 * @brief inits a db entry
 *  numeric(time) values to 0 and string entries to AM_DB_NULL_STR
 */
void am_db_entry_init(am_db_entry_t *entry)
{
	entry->name = AM_DB_NULL_STR;
	entry->version = AM_DB_NULL_STR;
	entry->filename = AM_DB_NULL_STR;
	entry->desc = AM_DB_NULL_STR;
	entry->groups = AM_DB_NULL_STR;
	entry->url = AM_DB_NULL_STR;
	entry->license = AM_DB_NULL_STR;
	entry->arch = AM_DB_NULL_STR;
	entry->files = AM_DB_NULL_STR;
	entry->backup = AM_DB_NULL_STR;
	entry->depends = AM_DB_NULL_STR;
	entry->optdepends = AM_DB_NULL_STR;
	entry->conflicts = AM_DB_NULL_STR;
	entry->provides = AM_DB_NULL_STR;
	entry->packager = AM_DB_NULL_STR;
	entry->replaces = AM_DB_NULL_STR;
	entry->md5sum = AM_DB_NULL_STR;
	entry->base = AM_DB_NULL_STR;
	entry->reason = 0;
	entry->size = 0;
	entry->csize = 0;
	entry->isize = 0;
	entry->force = 0;
	entry->hash = 0;
	entry->builddate = 0;
	entry->installdate = 0;
}

/**
 * @brief frees all memory that may have been allocated by am_db_* functions
 *  and re-init the entry
 */
void am_db_entry_clear(am_db_entry_t *entry)
{
	if (entry->name != AM_DB_NULL_STR) free(entry->name);
	if (entry->version != AM_DB_NULL_STR) free(entry->version);
	if (entry->filename != AM_DB_NULL_STR) free(entry->filename);
	if (entry->desc != AM_DB_NULL_STR) free(entry->desc);
	if (entry->groups != AM_DB_NULL_STR) free(entry->groups);
	if (entry->url != AM_DB_NULL_STR) free(entry->url);
	if (entry->license != AM_DB_NULL_STR) free(entry->license);
	if (entry->arch != AM_DB_NULL_STR) free(entry->arch);
	if (entry->files != AM_DB_NULL_STR) free(entry->files);
	if (entry->backup != AM_DB_NULL_STR) free(entry->backup);
	if (entry->depends != AM_DB_NULL_STR) free(entry->depends);
	if (entry->optdepends != AM_DB_NULL_STR) free(entry->optdepends);
	if (entry->conflicts != AM_DB_NULL_STR) free(entry->conflicts);
	if (entry->provides != AM_DB_NULL_STR) free(entry->provides);
	if (entry->packager != AM_DB_NULL_STR) free(entry->packager);
	if (entry->replaces != AM_DB_NULL_STR) free(entry->replaces);
	if (entry->md5sum != AM_DB_NULL_STR) free(entry->md5sum);
	if (entry->base != AM_DB_NULL_STR) free(entry->base);
	am_db_entry_init(entry);
}

/**
 * @brief reads a line from \cfp and strips leading and following whitespace
 *
 * @return a copy of the line read from \cfp
 *
 * @note this function never fails, AM_DB_NULL_STR is returned on error
 */
char *am_db_read_line(FILE *fp)
{
	char line[1024], *ln, *s;
	while ((ln = fgets(line, sizeof(line), fp))) {
		while (isspace(*ln)) ++ln;
		if (*ln) {
			s = ln;
			while (*ln) ++ln;
			while (isspace(*(ln - 1))) --ln;
			return am_db_strdup(s, ln - s);
		}
	}
	return AM_DB_NULL_STR;
}

/**
 * @brief reads a line from \cfp and interpret it as a base-10 number
 *
 * @return the next line in \cfp as a number
 *
 * @note this function never fails, 0 is return on error
 */
unsigned int am_db_read_number(FILE *fp)
{
	char line[128], *ln;
	unsigned int num = 0;
	while ((ln = fgets(line, sizeof(line), fp))) {
		while (isspace(*ln)) ++ln;
		if (*ln) {
			if (*ln >= '0' && *ln <= '9') {
				num = *ln - '0';
				++ln;
				while (*ln >= '0' && *ln <= '9') {
					num = (num * 10) + (*ln - '0');
					++ln;
				}
				return num;
			} else {
				return 0;
			}
		}
	}
	return 0;
}

/**
 * @brief parse a value into an \centry from \cfp based on tag
 *
 * @return 0 on success
 * @return 1 on failure
 */
int am_db_handle_tags(FILE *fp, am_db_entry_t *entry, am_db_tag_t hash)
{
	switch (hash) {
		case AM_DB_TAG_NAME:
			entry->name = am_db_read_line(fp);
			break;
		case AM_DB_TAG_VERSION:
			entry->version = am_db_read_line(fp);
			break;
		case AM_DB_TAG_FILENAME:
			entry->filename = am_db_read_line(fp);
			break;
		case AM_DB_TAG_DESC:
			entry->desc = am_db_read_line(fp);
			break;
		case AM_DB_TAG_GROUPS:

			break;
		case AM_DB_TAG_URL:
			entry->url = am_db_read_line(fp);
			break;
		case AM_DB_TAG_LICENSE:

			break;
		case AM_DB_TAG_ARCH:
			entry->arch = am_db_read_line(fp);
			break;
		case AM_DB_TAG_BUILDDATE:
			entry->builddate = am_db_read_number(fp);
			break;
		case AM_DB_TAG_INSTALLDATE:
			entry->installdate = am_db_read_number(fp);
			break;
		case AM_DB_TAG_PACKAGER:
			entry->packager = am_db_read_line(fp);
			break;
		case AM_DB_TAG_REASON:
			entry->reason = am_db_read_number(fp);
			break;
		case AM_DB_TAG_SIZE:
			entry->size = am_db_read_number(fp);
			break;
		case AM_DB_TAG_CSIZE:
			entry->csize = am_db_read_number(fp);
			break;
		case AM_DB_TAG_ISIZE:
			entry->isize = am_db_read_number(fp);
			break;
		case AM_DB_TAG_REPLACES:

			break;
		case AM_DB_TAG_FORCE:

			break;
		case AM_DB_TAG_FILES:

			break;
		case AM_DB_TAG_BACKUP:
			entry->isize = am_db_read_number(fp);
			break;
		case AM_DB_TAG_DEPENDS:

			break;
		case AM_DB_TAG_OPTDEPENDS:

			break;
		case AM_DB_TAG_CONFLICTS:

			break;
		case AM_DB_TAG_PROVIDES:

			break;
		case AM_DB_TAG_DELTAS:

			break;
		case AM_DB_TAG_MD5SUM:
			entry->md5sum = am_db_read_line(fp);
			break;
		case AM_DB_TAG_BASE:
			entry->base = am_db_read_line(fp);
			break;
		default:
			return 1;
			break;
	}
	return 0;
}

/**
 * @brief parse a file \cfn and fill the members of \centry
 *
 * @return -1 if the file \cfn could not be opened
 * @return 0 on success
 * @return greater than 0 to indicate the number unrecognised %TAG%s
 *
 * @note free the entry with \cam_db_entry_clear
 */
int am_db_load_file(const char *fn, am_db_entry_t *entry)
{
	char line[128], *ln;
	int ret;
	FILE *fp;

	fp = fopen(fn, "r");
	if (fp) {
		ret = 0;
		while ((ln = fgets(line, sizeof(line), fp))) {
			while (isspace(*ln)) ++ln;
			if (*ln == '%') {
				if (am_db_handle_tags(fp, entry, am_db_tag_gen(++ln)) != 0) {
					++ret;
				}
			}
		}
		fclose(fp);
		return ret;
	}
	return -1;
}


void am_db_init(am_db_t *db)
{
	db->entries = NULL;
	db->len = 0;
}

/**
 * @brief clears all entries and re-initializes the db
 *  applies \cam_db_entryclear() to each entry in the db
 *  then \cfree() the db  if necessary before calling \cam_db_init()
 */
void am_db_clear(am_db_t *db)
{
	unsigned int i, len = db->len;
	for (i = 0; i < len; ++i) am_db_entry_clear(&db->entries[i]);
	if (len > 0) free(db->entries);
	am_db_init(db);
}

/**
 * @brief read all subdir/{desc, depends} files from \cpath and fills \cdb
 * @note free the db with \cam_db_clear
 */
void am_db_load_dir(const char *path, am_db_t *db)
{
	unsigned int dlen, i;
	size_t len, pmax;
	DIR *dp;
	struct dirent *de;
	char fnb[FILENAME_MAX + 1], *fn;
	am_db_entry_t *entry;

	am_db_init(db);
	dp = opendir(path);
	if (dp) {
		dlen = am_dirlen(dp, AM_DIRLEN_SKIP_DOTFILES);
		if (dlen > 0 && (db->entries = malloc(sizeof(am_db_entry_t) * dlen))) {
			i = 0;
			len = strlen(path);
			/* 10 chars reserved for / + /depends and /entry + \0
			 * 110 chars reserved for d_name (pkgname + - + pkgver + pkgrel
			 */
			#define _AM__NAME_LEN 128
			if (len < sizeof(fnb) - _AM__NAME_LEN) {
				memcpy(fnb, path, len);
				fnb[len++] = '/';
				pmax = _AM__NAME_LEN - len;
				fn = &fnb[len];
				while (((de = readdir(dp)) != NULL) && (i < dlen)) {
					if (de->d_name[0] != '.') {
						len = strlen(de->d_name);
						if (len < _AM__NAME_LEN - 10) {
							memcpy(fn, de->d_name, len);
							memcpy(&fn[len], "/desc", 6);
							entry = &db->entries[i];
							am_db_entry_init(entry);
							if (am_db_load_file(fnb, entry) == 0) {
								memcpy(&fn[len], "/depends", 9);
								am_db_load_file(fnb, entry);
								++i;
							} else {
								am_db_entry_clear(entry);
							}
						}
					}
				}
			}
			#undef _AM__NAME_LEN
			if (i == 0) {
				free(db->entries);
				db->entries = NULL;
			} else {
				db->len = i;
			}
		}
		closedir(dp);
	}
}

void am_db_foreach(am_db_t *db, am_db_handler_t handler)
{
	unsigned int i;
	for (i = 0; i < db->len; ++i) {
		handler(&db->entries[i]);
	}
}
