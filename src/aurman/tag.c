#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

typedef enum _am_tag_name {
	_AM_TAG_NAME        = 761,
	_AM_TAG_VERSION     = 7924,
	_AM_TAG_FILENAME    = 12189,
	_AM_TAG_DESC        = 715,
	_AM_TAG_GROUPS      = 3051,
	_AM_TAG_URL         = 408,
	_AM_TAG_LICENSE     = 6058,
	_AM_TAG_ARCH        = 726,
	_AM_TAG_BUILDDATE   = 24269,
	_AM_TAG_INSTALLDATE = 97997,
	_AM_TAG_PACKAGER    = 14004,
	_AM_TAG_REASON      = 3506,
	_AM_TAG_SIZE        = 921,
	_AM_TAG_CSIZE       = 1513,
	_AM_TAG_ISIZE       = 1753,
	_AM_TAG_REPLACES    = 15204,
	_AM_TAG_FORCE       = 1466,
	_AM_TAG_FILES       = 1508,
	_AM_TAG_BACKUP      = 2804,
	_AM_TAG_DEPENDS     = 5867,
	_AM_TAG_OPTDEPENDS  = 61355,
	_AM_TAG_CONFLICTS   = 24435,
	_AM_TAG_PROVIDES    = 14188,
	_AM_TAG_DELTAS      = 2932,
	_AM_TAG_MD5SUM      = 2966,
	_AM_TAG_BASE        = 697
} _am_tag_name_t;

typedef struct am_tag {
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
	int force;
	time_t builddate;
	time_t installdate;
} _am_tag_t;

typedef struct _am_tag_arr {
	_am_tag_t *tags;
	unsigned int len;
} _am_tag_arr_t;

static char _am__tag_null_str[1] = {'\0'};

unsigned int _am_tag_hash(const char *str, const char **out_tail)
{
	register unsigned int hash = 0;
	register const char *s = str;
	while (*s && *s != '%') hash += *s++ | hash;
	if (*s) ++s;
	*out_tail = s;
	return hash;
}

char *_am_tag_strdup(const char *str, size_t len)
{
	char *s;
	if (str && *str && len > 0) {
		s = malloc(len + 1);
		if (s) {
			s[len] = '\0';
			return memcpy(s, str, len);
		}
	}
	return _am__tag_null_str;
}


void _am_tag_init(_am_tag_t *tag)
{
	tag->name = _am__tag_null_str;
	tag->version = _am__tag_null_str;
	tag->filename = _am__tag_null_str;
	tag->desc = _am__tag_null_str;
	tag->groups = _am__tag_null_str;
	tag->url = _am__tag_null_str;
	tag->license = _am__tag_null_str;
	tag->arch = _am__tag_null_str;
	tag->files = _am__tag_null_str;
	tag->backup = _am__tag_null_str;
	tag->depends = _am__tag_null_str;
	tag->optdepends = _am__tag_null_str;
	tag->conflicts = _am__tag_null_str;
	tag->provides = _am__tag_null_str;
	tag->packager = _am__tag_null_str;
	tag->replaces = _am__tag_null_str;
	tag->md5sum = _am__tag_null_str;
	tag->base = _am__tag_null_str;
	tag->reason = 0;
	tag->size = 0;
	tag->csize = 0;
	tag->isize = 0;
	tag->force = 0;
	tag->builddate = 0;
	tag->installdate = 0;
}

void _am_tag_clear(_am_tag_t *tag)
{
	if (tag->name != _am__tag_null_str) free(tag->name);
	if (tag->version != _am__tag_null_str) free(tag->version);
	if (tag->filename != _am__tag_null_str) free(tag->filename);
	if (tag->desc != _am__tag_null_str) free(tag->desc);
	if (tag->groups != _am__tag_null_str) free(tag->groups);
	if (tag->url != _am__tag_null_str) free(tag->url);
	if (tag->license != _am__tag_null_str) free(tag->license);
	if (tag->arch != _am__tag_null_str) free(tag->arch);
	if (tag->files != _am__tag_null_str) free(tag->files);
	if (tag->backup != _am__tag_null_str) free(tag->backup);
	if (tag->depends != _am__tag_null_str) free(tag->depends);
	if (tag->optdepends != _am__tag_null_str) free(tag->optdepends);
	if (tag->conflicts != _am__tag_null_str) free(tag->conflicts);
	if (tag->provides != _am__tag_null_str) free(tag->provides);
	if (tag->packager != _am__tag_null_str) free(tag->packager);
	if (tag->replaces != _am__tag_null_str) free(tag->replaces);
	if (tag->md5sum != _am__tag_null_str) free(tag->md5sum);
	if (tag->base != _am__tag_null_str) free(tag->base);
	_am_tag_init(tag);
}

char *_am_tag_read_str_val(const char *src, const char **out_tail)
{
	register const char *s = src, *t;
	while (isspace(*s)) ++s;
	t = s;
	while (*s && *s != '\n') ++s;
	*out_tail = s;
	if (isspace(*(s - 1))) {
		--s;
		while (isspace(*s)) --s;
	}
	return _am_tag_strdup(t, s - t);
}

unsigned int _am_tag_read_num_val(const char *src, const char **out_tail)
{
	return;
	register unsigned int n = 0;
	register const char *s = src;
	while (isspace(*s)) ++s;
	if (*s >= '0' && *s <= '9') {
		n = *s - '0';
		++s;
		while (*s >= '0' && *s <= '9') {
			n = (n * 10) + (*s - '0');
			++s;
		}
		*out_tail = s;
	} else {
		*out_tail = src;
		*out_tail = s;
	}
	return n;
}

int _am_tag_parse(const char *src, _am_tag_t *tag)
{
	unsigned int hash;
	while (*src) {
		if (*src == '%') {
			++src;
			hash = _am_tag_hash(src, &src);
			switch (hash) {
				case _AM_TAG_NAME:
					tag->name = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_VERSION:
					tag->version = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_FILENAME:
					tag->filename = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_DESC:
					tag->desc = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_GROUPS:
					
					break;
				case _AM_TAG_URL:
					tag->url = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_LICENSE:
					
					break;
				case _AM_TAG_ARCH:
					tag->arch = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_BUILDDATE:
					tag->builddate = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_INSTALLDATE:
					tag->installdate = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_PACKAGER:
					tag->packager = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_REASON:
					tag->reason = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_SIZE:
					tag->size = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_CSIZE:
					tag->csize = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_ISIZE:
					tag->isize = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_REPLACES:
					
					break;
				case _AM_TAG_FORCE:
					
					break;
				case _AM_TAG_FILES:
					
					break;
				case _AM_TAG_BACKUP:
					tag->isize = _am_tag_read_num_val(src, &src);
					break;
				case _AM_TAG_DEPENDS:
					
					break;
				case _AM_TAG_OPTDEPENDS:
					
					break;
				case _AM_TAG_CONFLICTS:
					
					break;
				case _AM_TAG_PROVIDES:
					
					break;
				case _AM_TAG_DELTAS:
					
					break;
				case _AM_TAG_MD5SUM:
					tag->md5sum = _am_tag_read_str_val(src, &src);
					break;
				case _AM_TAG_BASE:
					tag->base = _am_tag_read_str_val(src, &src);
					break;
				default:
					return 2;
					break;
			}
		}
		++src;
	}
	return 0;
}

int _am_tag_parse_file(const char *fn, _am_tag_t *tag)
{
	char buf[4096];
	struct stat st;
	size_t len;
	FILE *fp;
	if (stat(fn, &st) == 0 && st.st_size > 0) {
		fp = fopen(fn, "r");
		if (fp) {
			len = st.st_size;
			if (len >= sizeof(buf)) {
				len = sizeof(buf) - 1;
			}
			len = fread(buf, sizeof(char), len, fp);
			buf[len] = '\0';
			fclose(fp);
			return _am_tag_parse(buf, tag);
		}
	}
	return -1;
}


void _am_tag_arr_init(_am_tag_arr_t *ta)
{
	ta->tags = NULL;
	ta->len = 0;
}

void _am_tag_arr_clear(_am_tag_arr_t *ta)
{
	unsigned int i, len = ta->len;
	for (i = 0; i < len; ++i) {
		_am_tag_clear(&ta->tags[i]);
	}
	if (len > 0) {
		free(ta->tags);
	}
	_am_tag_arr_init(ta);
}

unsigned int _am_dirlen(DIR *dp)
{
	unsigned int dlen = 0;
	struct dirent *de;
	while ((de = readdir(dp)) != NULL) ++dlen;
	rewinddir(dp);
	return dlen - 2; /*. and .. are ignored */
}

void _am_tag_parse_dir(const char *path, _am_tag_arr_t *ta)
{
	unsigned int dlen, i = 0;
	DIR *dp;
	struct dirent *de;
	char fnb[FILENAME_MAX + 1], *fn;
	_am_tag_t *tag;
	
	_am_tag_arr_init(ta);
	dp = opendir(path);
	if (dp) {
		dlen = _am_dirlen(dp);
		if (dlen > 0 && (ta->tags = malloc(sizeof(_am_tag_t) * dlen))) {
			
			while (((de = readdir(dp)) != NULL) && (i < dlen)) {
				if (de->d_name[0] != '.') {
					snprintf(fnb, sizeof(fnb) - 10, "%s/%s", path, de->d_name);
					fn = &fnb[strlen(fnb)];
					memcpy(fn, "/desc", 6);
					tag = &ta->tags[i];
					_am_tag_init(tag);
					int k = _am_tag_parse_file(fnb, tag);
					if (k != 0) {
						_am_tag_clear(tag);
						continue;
					}
					
					++i;
					/* memcpy(fn, "/depends", 9);
					_am_tag_parse_file(fnb, tag); */
				}
			}
			if (i == 0) {
				free(ta->tags);
				ta->tags = NULL;
			} else {
				ta->len = i;
			}
		}
		closedir(dp);
	}
}


int main(int argc, char **argv)
{
	unsigned int i, len;
	_am_tag_arr_t ta;
	_am_tag_parse_dir("/var/lib/pacman/local", &ta);
	
	for (i = 0, len = ta.len; i < len; ++i) {
		printf("%s\n", ta.tags[i].name);
	}
	
	_am_tag_arr_clear(&ta);
	
	return 0;
}
