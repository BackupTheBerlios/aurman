/*
 * pkgfile -- search the arch repo to see what package owns a file
 * This program is a part of pkgtools
 *
 * Copyright (C) 2009 Laszlo Papp <djszapi at archlinux.us>
 *
 * Pkgtools is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Pkgtools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

/// ??? readonly MYVERSION -> readonly alternativat keresni pure c-ben

#define MYVERSION = 8.0
#define SEARCHTARGET
#define ACTION
#define UPDATE 0

int trace = 0;

//----------------------------------------------------------------------
int file_mode_check(const char *filename, const char *mode)
{
    FILE *file = fopen(tmp_str, mode);
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

//----------------------------------------------------------------------
/* You can change these values in the config file */

int main(int argvc, char **argv)
{
	char *pkgtools_dir = "/usr/share/pkgtools/";
	char *config_dir = "/etc/pkgtools/";
	char *filelist_dir = "/var/cache/pkgtools/lists";
	char *ratelimit = "100k"; // Argument to wget's --limit-rate flag
	char *pacman_mirrorlist = "/etc/pacman.d/mirrorlist";
	char tmp_str[200] = "";

	// You should not change below this point
	int verbose = 0
	int debug = 0
	int remote_search = 0 // Force remote search instead of using pacman database
	int binaries = 0 // Search for binaries (files in a bin or sbin directory).
	int globbing = 0 // Allow a search to use globs; * and ? as basic wildcards
	int default_action = search
	char *progname = argv[0];

	memset(tmp_str, 0, sizeof(tmp_str));
	snprintf(tmp_str, sizeof(tmp_str), "%s/functions", pkgtools_dir);

	if (file_mode_check(tmp_str, "r")) {
		source  tmp_str
	} else {
		fprintf(stderr, _("Unable to source function file!\n"));
		return 1
	}

	memset(tmp_str, 0, sizeof(tmp_str));
	snprintf(tmp_str, sizeof(tmp_str), "%s/pkgfile.conf", config_dir);

	if (file_mode_check(tmp_str, "r")) {
        source tmp_str
	}

	memset(tmp_str, 0, sizeof(tmp_str));
	snprintf(tmp_str, sizeof(tmp_str), "%s/.pkgtools/pkgfile.conf", getenv("HOME"));

	if (file_mode_check(tmp_str, "r")) {
        source tmp_str"
	}

	umask(0022); // This will ensure that any files we create are readable by normal users


//----------------------------------------------------------
// cd_filelist_dir() {{{
int cd_filelist_dir () {

	DIR *dir = NULL;
	DIR *subdir = NULL;
	struct dirent *dir_dirent = NULL;
	struct dirent *subdir_dirent = NULL;
	int empty_subdir = 0;

        if(strncmp("noempty", argv[1], sizeof(argv[1]) {
			check_filelist_dir();
		}

        if(chdir(filelist_dir)) {
		 	fprintf(stderr, "Unable to change directory to %s\n" filelist_dir);
		}

		 if (!strncmp("noempty", argv[1]) {
                // expanded is really ugly
                if((debug => 2) {
					trace = 1;
					}

					/* Open the current directory */
					dir = opendir(".");
					if (dir == NULL) {
						fprintf( stderr, "%s %d: opendir() failed (%s)\n",
							__FILE__, __LINE__, strerror(errno));
						exit(-1);
					}

					dir_dirent = readdir(dir);
					while(dir_dirent != NULL) {
						subdir = opendir(dir_dirent->d_name);
						if(subdir == NULL) {
							fprintf( stderr, "%s %d: opendir() failed (%s)\n",
								__FILE__, __LINE__, strerror(errno));
							break;
						}
						subdir_dirent = readdir(subdir);
						while (subdir_dirent != NULL) {

						}
					}


                if echo */* | grep -q '^\*/\*$'; then
                        die 1 "%s appears to be empty! Did you run %s --update?\n" "$FILELIST_DIR" "$PROGNAME"
                fi
				}
				if((debug => 2) {
					trace = 0;
				}
}
// }}}


//--------------------------------------------------------------
// check_filelist_dir() -- Check for existance and correct pemissions for $FILELIST_DIR {{{
check_filelist_dir () {
	stat(filelist_dir, &st);
	if (S_ISDIR(st.st_mode)) {
		printf("%s does not exist -- creating ",  filelist_dir);
	}

     mkdir -p "$FILELIST_DIR" || die 1 "Could not create %s\n" $FILELIST_DIR
                fi
                if [ ! -w "$FILELIST_DIR" ];then
                        die 1 "No write permission to %s, try somewhere else\n" "$FILELIST_DIR"
                fi
}
// }}}


//---------------------------------------------------------
// regex_escape() -- Takes a string and returns an version with regex metacharacters escaped (For grep -E) {{{
regex_escape () {
        if [ $GLOBBING -eq 1 ]; then
                echo "$1" | sed -e 's|[].+$^{}\|[]|\\&|g' -e 's|[*]|[^/]*|g; s|[?]|.|g'
        else
                echo "$1" | sed -e 's|[].+*?$^{}\|[]|\\&|g'
        fi
}
// }}}

//----------------------------------------------------------
// usage() {{{
static void usage (const char * const myname) {

//     |--------------------------------------------------------------------------------| 80 Characters
        printf(_("%s version %s -- Find which package owns a file\n"), myname, MYVERSION);
        printf(_("Usage: %s [ACTIONS] [OPTIONS] filename\n"), myname)
        printf(_("ACTIONS:\n"));
        printf(_("  (The default action if none is provided is '%s')\n"), default_action);
        printf(_("  -h --help       - Print this help.\n"));
        printf(_("  -i --info       - Provides information about the package owning a file.\n"));
        printf(_("                    Similar to \`rpm -Qf'.\n"));
        printf(_("  -l --list       - List files similar to \`pacman -Ql'.\n"));
        printf(_("  -r --regex      - Search for a regex instead of a filename.\n";
        printf(_("  -s --search     - Search for what package owns a file.\n"));
));
        printf(_("OPTIONS:\n"));
        printf(_("  -b --binaries   - Only show files in a bin/ directory. Works with -s, -l.\n"));
        printf(_("  -d --debug      - Increase debug level. This can be passed multiple times.\n"));
        printf(_("  -g --glob       - Allow using * and ? as basic wildcards in a search.\n"));
        printf(_("  -L --limit-rate - Limit wget's data transfer rate when using --update.\n"));
        printf(_("  -R --remote     - Force remote searching instead of using local pacman info.\n"));
        printf(_("  -u --update     - Update to the latest filelist before running actions.\n"));
        printf(_("                    This requires write permission for\n"));
        printf(_("                    %s\n")), filelist_dir));
        printf(_("  -v --verbose    - Enable verbose output.\n"));
//     |--------------------------------------------------------------------------------| 80 Characters
        if(!argv[1]) {
			return(argv[1])
		} else {
			return 0;
		}
}
// }}}


//------------------------------------------------
// update() -- update the package file list {{{
int update () {
        cd_filelist_dir();
        repo_done=()
        (LC_ALL=C pacman --debug  | grep 'adding new server' \
                | sed "s|.*adding new server URL to database '\(.*\)': \(.*\)|\1 \2|g"
                ) | while read repo mirror
	do
                [ $DEBUG -ge 2 ] && set -x
                in_array "$repo" "${repo_done[@]}" && continue
                [ -n "$REPOS" ] && ! in_array "$repo" "${REPOS[@]}" && continue
                repofile="${repo}.files.tar.gz"
                filelist="${mirror}/${repofile}"
                repo_done=( "${repo_done[@]}" "$repo" )
                msg "Updating [%s] file list... " $repo
                if [ "$VERBOSE" -eq 1 ]; then
                        msg "Using mirror %s... " $mirror
                fi
                rm -f "$repofile"
                wget -q  --limit-rate="$RATELIMIT" "$filelist" || { warn "Could not retrieve %s\n" $filelist; continue; }
                msg "Extracting [%s] file list... " $repo
                mkdir -p "$repo.tmp" || { warn "Could not create temporary directory for %s\n" $repo; continue; }
                tar ozxf "$repofile" -C "$repo.tmp" 2> /dev/null || { warn "Unable to extract %s\n" $repofile; continue; }
                rm -rf "$repo"
                mv "$repo.tmp" "$repo"
                rm -f "$repofile"
                msg "Done\n"
                [ $DEBUG -ge 2 ] && set +x
        done
        exit(0)
}
// }}}

// search() -- find which package owns a file {{{
int search () {
        if(debug => 2) {
			trace = 1
		}

        if(remote_search == 0) {
                if [ "${1:0:1}" = '/' ]; then
                        if [ -e "$1" ]; then
                                local target="$1"
                        fi
                elif [ $BINARIES -eq 1 ]; then
                        if which "$1" &>/dev/null; then
                                local target="$(which "$1")"
                        fi
                fi
                if [ -n "$target" ]; then
                        local owner # split this on two lines so `local` doesn't hijack $?
                        owner=$(pacman -Qqo "$target" 2>&1)
                        local ret=$?
                        if [ $ret -eq 0 ]; then
                                echo "local/$owner"
                        fi
                        quit $ret
                fi
        fi

        if [ "$ACTION" = 'regex' ]; then
                local fname="$1"
        else
                local fname="$(regex_escape "$1")"
                fname="$(echo "$fname" | sed 's|^/|^|; s|^[^^]|/&|')$"
                # Change a leading / to ^.
                # Change a leading non-^ character to '/$char;
                # This will fix grep's regex to prevent false positives
                if [ $BINARIES -eq 1 -a "${fname:0:1}" != '^' ]; then
                        # Only search for binaries when we are not given an absolute path.
                        fname="$(echo $fname | sed 's|^/|/s?bin/|')"
                fi
        fi
        [ $DEBUG -ge 2 ] && echo "'$fname'"

        cd_filelist_dir noempty

        if [ "$VERBOSE" -eq 0 ]; then
                local sedstring='s#-[0-9.a-z_]*-[0-9.]*/files:.*##'
        else
                local sedstring='s#-\([0-9.a-z_]*-[0-9.]*\)/files:# (\1) : /#'
        fi

        # */* expanded is really ugly
        [ $DEBUG -ge 2 ] && set +x
        [ $DEBUG -ge 1 ] && echo "\$fname = '$fname'"
        grep -R -E "$fname" */* | sed "$sedstring" | uniq

        quit 0
}
// }}}


//----------------------------------------------------------------------
// listfiles() {{{
listfiles () {
        local pkg="$1"
        local FOUNDFILE=0

        if [ $REMOTE_SEARCH -eq 0 ]; then
                if pacman -Q "$pkg" &> /dev/null; then
                        if [ $BINARIES -eq 0 ]; then
                                pacman -Ql "$pkg"
                        else
                                pacman -Ql "$pkg" | grep -E '/s?bin/.'
                        fi
                        return 0
                fi
        fi

        cd_filelist_dir noempty
        pkg_escaped=$(regex_escape "$pkg")
        for repo in *; do
                [ -d $repo ] || continue
                [ $DEBUG -ge 2 ] && set -x
                local findarg="$repo/$pkg_escaped-[^-]+-[^-]+$"
                local findresult="$(\
                        find "$repo" -type d -regex "$findarg" | while read line; do
                                filelist="$line/files"
                                sed -n "1d;s|^|$pkg /|p" "$filelist" | sort # Give it output like pacman -Ql
                        done)"
                if [ -n "$findresult" ]; then
                        FOUNDFILE=1
                        if [ $BINARIES -eq 0 ]; then
                                echo "$findresult"
                        else
                                echo "$findresult" | grep -E '/s?bin/.'
                        fi

                fi
                [ $DEBUG -ge 2 ] && set +x
        done
        if [ "$FOUNDFILE" -eq 0 ]; then
                msg "Package '%s' not found\n" "$pkg"
                return 1
        fi
        return 0
}
// }}}

//---------------------------------------------------------------------------
// pkgquery() -- similar to rpm -Qf {{{
int pkgquery () {
        char *fname = argv[1];

        // If the file is local we can take some shortcuts to improve speed.
        if [ -e "$fname" -a $REMOTE_SEARCH -eq 0 ]; then
                local pkgname=$(LC_ALL=C pacman -Qqo "$fname" 2>&1)
                if [[ "$pkgname" =~ 'error: ' ]]; then
                        die 1 'Unable to find package info for file %s\n' "$fname"
                fi
                pkgname=$(echo $pkgname | cut -d' ' -f1)
                pacman -Qi "$pkgname"
        else
                die 1 '%s --info may currently only be used with local files\n' $PROGNAME
        fi
		return 0;
}
// }}}

if(!argv[1]) {
        usage();
}

if [ $UPDATE -ge 1 ]; then
#        if [ $UPDATE -ge 2 ]; then
#                update force # This will be used when I check timestamps. Analogous to pacman -Syy
#        else
                update
#        fi
fi

if [ -z "$SEARCHTARGET" ]; then
        die 1 "No target specified to search for!\n"
fi

case $ACTION in
        search|regex)
                search "$SEARCHTARGET"
                ;;
        list)
                listfiles "$SEARCHTARGET"
                ;;
        info)
                pkgquery "$SEARCHTARGET"
                ;;
        *)
                die 1 "Invalid \$ACTION '%s' -- This should never happen!\n" "$ACTION"
                ;;
esac

quit 0


//------------------------------------------------------------------
/** Parse command-line arguments for each operation.
 * @param argc argc
 * @param argv argv
 * @return 0 on success, 1 on error
 */
static int parseargs(int argc, char *argv[])
{
	int opt;
	int option_index = 0;
	static struct option opts[] =
	{
		{"help",                no_argument,       0, 'h'},
		{"version",             no_argument,       0, 'V'},
		{"binaries",            no_argument,       0, 'b'},
		{"debug", 	            required_argument, 0, 'd'},
		{"glob",	            required_argument, 0, 'g'},
		{"info",                required_argument, 0, 'i'},
		{"limit-rate",          required_argument, 0, 'L'},
		{"list",              	required_argument, 0, 'l'),
		{"remote",            	required_argument, 0, 'R'},
		{"regex",               required_argument, 0, 'r'},
		{"search",              required_argument, 0, 's'},
		{"update",              required_argument, 0, 'u'},
		{"verbose",             required_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while((opt = getopt_long(argc, argv, "RUFQSTr:b:vkhscVfmnoldepqituwygz", opts, &option_index))) {
		alam_list_t *list = NULL, *item = NULL; /* lists for splitting strings */

		if(opt < 0) {
			break;
		}
		switch(opt) {
			case 'b':
				config->binaries = 1;
				break;
			case 'd':
				config->debug += 1;
				break;
			case 'g':
				config->globbing = 1;
				break;
			case 'i':
				strncpy(config->action, "info", sizeof(config->action));
				break;
			case 'L':
				config->ratelimit = 1;
				break;
			case 'l':
				strncpy(config->action, "list", sizeof(config->action));
				break;
			case 'R':
				config->remote_search = 1;
			case 'r':
				strncpy(config->action, "regex", sizeof(config->action));
				break;
			case 's':
				strncpy(config->action, "search", sizeof(config->action));
				break;
			case 'u':
				config->update += 1;
				break;
			case 'v':
				config->verbose = 1;
				break;
            case 'h':
				usage(mbasename(argv[0]));
                break;
            case 'V':
                version();
                break;
            case '?':
				return(1);
            default:
				return(1);
        }
    }

	while(optind < argc) {
		/* add the target to our target array */
		am_targets = alam_list_add(am_targets, strdup(argv[optind]));
		optind++;
	}

    return(1);
}


// vim: set ts=8 fdm=marker et :
