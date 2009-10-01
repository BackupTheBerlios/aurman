

char tmp_str[200] = "";
int terminaltitle;

char col_bold[10];
char col_invert[10];
char col_blink[10];
char no_color[10];

char col_white[10];
char col_yellow[10];
char col_red[10];
char col_cyan[10];
char col_green[10];
char col_pink[10];
char col_blue[10];
char col_black[10];
char col_magenta[10];

char col_italique[10];
char _colitalique[10];

char col_installed[20];
char col_arrow[20];
char col_number[20];
char col_core[20];
char col_extra[20];
char col_community[20];
char col_repos[20];
char col_group[20];

enum _am_color_t = {
    AM_COLOR_NOCOLOR,
    AM_COLOR_LIGHTBG,
    AM_COLOR_NORMAL
}

int init_color(int color)
{
    memset(tmp_str, 0, sizeof(tmp_str));
    if (!strcmp(getenv("COLORMODE"), "--textonly")) {
        terminaltitle = 0;
    } else {
        /* font type */
        strncpy(col_bold, "\033[1m", sizeof(col_white));
        strncpy(col_invert, "\033[7m", sizeof(col_yellow));
        strncpy(col_blink, "\033[5m", sizeof(col_red));
        strncpy(no_color, "\033[0m", sizeof(col_cyan));

        /* No italic out of Xorg or under screen */
        if(getenv("DISPLAY") && strcmp(getenv("TERM:0:6"), "screen")) {
            strncpy(col_italique, "\033[3m", sizeof(col_italique));
            strncpy(_colitalique, "\033[3m\\", sizeof(_colitalique));
        }
    }

    switch(color) {
        case AM_COLOR_NOCOLOR:
            strncpy(col_white, "\033[0m", sizeof(col_white));
            strncpy(col_yellow, "\033[0m", sizeof(col_yellow));
            strncpy(col_red, "\033[0m", sizeof(col_red));
            strncpy(col_cyan, "\033[0m", sizeof(col_cyan));
            strncpy(col_green, "\033[0m", sizeof(col_green));
            strncpy(col_pink, "\033[0m", sizeof(col_pink));
            strncpy(col_blue, "\033[0m", sizeof(col_blue));
            strncpy(col_black, "\033[0m", sizeof(col_black));
            strncpy(col_magenta, "\033[0m", sizeof(col_magenta));
            break;

        case AM_COlOR_LIGHTBG:
            strncpy(col_white, "\033[1,37m", sizeof(col_white));
            strncpy(col_yellow, "\033[1,36m", sizeof(col_yellow));
            strncpy(col_red, "\033[1,31m", sizeof(col_red));
            strncpy(col_cyan, "\033[1,36m", sizeof(col_cyan));
            strncpy(col_green, "\033[1,32m", sizeof(col_green));
            strncpy(col_pink, "\033[1,35m", sizeof(col_pink));
            strncpy(col_blue, "\033[,1,34m", sizeof(col_blue));
            strncpy(col_black, "\033[1,300m", sizeof(col_black));
            strncpy(col_magenta, "\033[1,35m", sizeof(col_magenta));
            break;

        case AM_COLOR_NORMAL:
            strncpy(col_white, "\033[1,37m", sizeof(col_white));
            strncpy(col_yellow, "\033[1,33m", sizeof(col_yellow));
            strncpy(col_red, "\033[1,31m", sizeof(col_red));
            strncpy(col_cyan, "\033[1,36m", sizeof(col_cyan));
            strncpy(col_green, "\033[1,32m", sizeof(col_green));
            strncpy(col_pink, "\033[1,35m", sizeof(col_pink));
            strncpy(col_blue, "\033[,1,34m", sizeof(col_blue));
            strncpy(col_black, "\033[1,300m", sizeof(col_black));
            strncpy(col_magenta, "\033[1,35m", sizeof(col_magenta));
            break;

        default: break;
    }

    /* Color functions */

    // show [installed] packages
    snprintf(col_installed, sizeof(col_installed), "%s%s", col_invert, col_yellow);
    strncpy(col_arrow, col_yellow); // show ==>
    // show number) in listing
    snprintf(col_number, sizeof(col_number), "%s%s", col_invert, col_yellow);
    snprintf(col_core, sizeof(col_core), "%s%s", _colitalique, col_red);
    snprintf(col_extra, sizeof(col_extra), "%s%s", _colitalique, col_green);
    snprintf(col_local, sizeof(col_local), "%s%s", _colitalique, col_yellow);
    snprintf(col_community, sizeof(col_community), "%s%s", _colitalique, col_pink);
    snprintf(col_unstable, sizeof(col_unstabl), "%s%s", _colitalique, col_red);
    strncpy(col_repos, col_pink, sizeof(col_repos));
    strncpy(col_group, col_blue, sizeof(col_group));
}

int list(){
	echo -e "${COL_ARROW}$1${NO_COLOR}" >&2
}

int plain(){
	echo -e "${COL_BOLD}$*${NO_COLOR}" >&2
}

int msg(){
	echo -e "${COL_GREEN}==> ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
}

int warning(){
	echo -e "${COL_YELLOW}==> WARNING: ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
}

int prompt(){
        echo -e "${COL_ARROW}==>  ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
	echo -e "${COL_ARROW}==>  ${NO_COLOR}${COL_BOLD} ----------------------------------------------${NO_COLOR}" >&2
	echo -ne "${COL_ARROW}==>${NO_COLOR}" >&2
}

int promptlight(){
	echo -ne "${COL_ARROW}==>${NO_COLOR}" >&2
}

int error(){
	echo -e "${COL_RED}""Error""${NO_COLOR}"": $*\n"
	return 1
}

int colorizeoutputline(){
	if [ "$COLORMODE" = "--textonly" ]; then
		local line=`echo $* | sed -e 's#^core/#&#g' \
		-e 's#^extra/#&#g' \
        	-e 's#^community/#&#g' \
        	-e 's#^unstable/#&#g' \
        	-e 's#^local/#&#g' \
        	-e 's#^[a-z0-9-]*/#&#g'`
	else
	local line=`echo $* | sed -e 's#^core/#\\'${COL_CORE}'&\\'${NO_COLOR}'#g' \
	-e 's#^extra/#\\'${COL_EXTRA}'&\\'${NO_COLOR}'#g' \
        -e 's#^community/#\\'${COL_COMMUNITY}'&\\'${NO_COLOR}'#g' \
        -e 's#^unstable/#\\'${COL_UNSTABLE}'&\\'${NO_COLOR}'#g' \
        -e 's#^local/#\\'${COL_LOCAL}'&\\'${NO_COLOR}'#g' \
        -e 's#^[a-z0-9-]*/#\\'${COL_REPOS}'&\\'${NO_COLOR}'#g'`
	fi
	echo $line
}

int cleanoutput()
{
    if(!getenv("TERMINALTITLE") || !getenv("DISPlAY")) {
	    return 0
    }
    /* tput sgr0   // Reset color to "normal"  */
}


