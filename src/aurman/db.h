#ifndef _AM_DESC
#define _AM_DESC

#include <time.h>
#include <alpm_list.h>


/* Database */
struct __amdb_t {
	char *treename;
	/* do not access directly, use _alpm_db_path(db) for lazy access */
	char *_path;
	unsigned short pkgcache_loaded;
	unsigned short grpcache_loaded;
	unsigned short is_local;
	alpm_list_t *pkgcache;
	alpm_list_t *grpcache;
	alpm_list_t *servers;
};

typedef struct __amdb_t amdb_t;

/**
 * @note all values are hash values generated from \cam_db_tag_hash()
 * and should should generated with the function which is shipped with aurman.
 */
typedef enum am_db_tag {
	AM_DB_TAG_NAME        = 761,
	AM_DB_TAG_VERSION     = 7924,
	AM_DB_TAG_FILENAME    = 12189,
	AM_DB_TAG_DESC        = 715,
	AM_DB_TAG_GROUPS      = 3051,
	AM_DB_TAG_URL         = 408,
	AM_DB_TAG_LICENSE     = 6058,
	AM_DB_TAG_ARCH        = 726,
	AM_DB_TAG_BUILDDATE   = 24269,
	AM_DB_TAG_INSTALLDATE = 97997,
	AM_DB_TAG_PACKAGER    = 14004,
	AM_DB_TAG_REASON      = 3506,
	AM_DB_TAG_SIZE        = 921,
	AM_DB_TAG_CSIZE       = 1513,
	AM_DB_TAG_ISIZE       = 1753,
	AM_DB_TAG_REPLACES    = 15204,
	AM_DB_TAG_FORCE       = 1466,
	AM_DB_TAG_FILES       = 1508,
	AM_DB_TAG_BACKUP      = 2804,
	AM_DB_TAG_DEPENDS     = 5867,
	AM_DB_TAG_OPTDEPENDS  = 61355,
	AM_DB_TAG_CONFLICTS   = 24435,
	AM_DB_TAG_PROVIDES    = 14188,
	AM_DB_TAG_DELTAS      = 2932,
	AM_DB_TAG_MD5SUM      = 2966,
	AM_DB_TAG_BASE        = 697
} am_db_tag_t;

/**
 * @note all members correspond to pacman's desc and depends files,
 *  except \chash which is an \cam_db_tag_gen() hash value of \cname
 *  and should be updated whenever \cname is changed
 * @note am_db_* functions use AM_DB_NULL_STR in place NULL and "" on string members
 */
typedef struct am_db_entry {
	char *name;
	char *version;
	char *filename;
	char *desc;
	char *groups;
	char *url;
	char *license;
	char *arch;
	char *files;
	char *backup;
	char *depends;
	char *optdepends;
	char *conflicts;
	char *provides;
	char *packager;
	char *replaces;
	char *md5sum;
	char *base;
	unsigned int reason;
	unsigned int size;
	unsigned int csize;
	unsigned int isize;
	unsigned int hash;
	int force;
	time_t builddate;
	time_t installdate;
} am_db_entry_t;

typedef struct am_db {
	am_db_entry_t *entries;
	unsigned int len;
} am_db_t;

typedef void (*am_db_handler_t)(am_db_entry_t *entry);

/**
 * @brief used in place NULL and "" for all \cam_db_entry strings
 */
extern char AM_DB_NULL_STR[];
#endif
