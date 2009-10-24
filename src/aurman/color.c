#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

const char *col_core = "\x1b[31m";
const char *col_extra = "\x1b[32m";
const char *col_community = "\x1b[33m";
const char *col_repos = "\x1b[34m";
const char *col_group = "\x1b[35m";
const char *col_name = "\x1b[36m";
const char *col_version = "\x1b[32m";
const char *col_reset = "\x1b[0m";
const char *col_reset = "\x1b[0m";


/*
#define PRINT_ALL_ARG(val, c_arg) va_list val; \
	va_start(val, c_arg); \
		for (int i = 0; i < c_arg; i++) fprintf(stderr,"%s ", va_arg(val, const char*)); \
	va_end(val)
*/

char tmp_str[200] = "";
int terminaltitle;

char col_installed[20];
char col_arrow[20];
char col_number[20];
char col_local[20];
char col_unstable[20];

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
		return 0;
    } else {

		const char *col_bold = "\033[1m";
		const char *col_invert = "\033[7m";
		const char *col_blink = "\033[5m";
		const char *col_nocolor = "\033[0m";

        /* No italic out of Xorg or under screen */
        if(getenv("DISPLAY") && strcmp(getenv("TERM:0:6"), "screen")) {
            const char *col_italique = "\033[3m";
            const char *_colitalique = "\033[3m\\";
        }
    }

    switch(color) {
        case AM_COLOR_NOCOLOR:
            const char *col_white = "\033[0m";
            const char *col_yellow = "\033[0m";
            const char *col_red = "\033[0m";
            const char *col_cyan = "\033[0m";
            const char *col_green = "\033[0m";
            const char *col_pink = "\033[0m";
            const char *col_blue = "\033[0m";
            const char *col_black = "\033[0m";
            const char *col_magenta = "\033[0m";
            break;

        case AM_COlOR_LIGHTBG:
            const char *col_white = "\033[1,37m";
            const char *col_yellow = "\033[1,36m";
            const char *col_red = "\033[1,31m";
            const char *col_cyan = "\033[1,36m";
            const char *col_green = "\033[1,32m";
            const char *col_pink = "\033[1,35m";
            const char *col_blue = "\033[,1,34m";
            const char *col_black = "\033[1,300m";
            const char *col_magenta = "\033[1,35m";
            break;

        case AM_COLOR_NORMAL:
            const char *col_white = "\033[1,37m";
            const char *col_yellow = "\033[1,33m";
            const char *col_red = "\033[1,31m";
            const char *col_cyan = "\033[1,36m";
            const char *col_green = "\033[1,32m";
            const char *col_pink = "\033[1,35m";
            const char *col_blue = "\033[,1,34m";
            const char *col_black = "\033[1,300m";
            const char *col_magenta = "\033[1,35m";
            break;

        default: break;
    }

    /* Color functions */

    // show [installed] packages
    col_installed, "%s%s", col_invert, col_yellow);
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

int list(char * arg1){
	//echo -e "${COL_ARROW}$1${NO_COLOR}" >&2
	fprintf(stderr,"%s%s%s\n", col_arrow, arg1, no_color);
}

int plain(char * text/*unsigned int count_arg, ...*/){
	//echo -e "${COL_BOLD}$*${NO_COLOR}" >&2
	fprintf(stderr,"%s%s%s\n", col_bold,text,no_color);
	PRINT_ALL_ARG(p,count_arg);
	//fprintf(stderr,"%s\n", no_color);
}

int msg(char * text /*unsigned int count_arg, ...*/){
	//echo -e "${COL_GREEN}==> ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
	fprintf(stderr,"%s==> %s%s%s%s\n", col_green, no_color, col_bold, text, no_color);
	//PRINT_ALL_ARG(p,count_arg);
	//fprintf(stderr,"%s\n", no_color);
}

int warning(unsigned int count_arg, ...){
	//echo -e "${COL_YELLOW}==> WARNING: ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
	fprintf(stderr,"%s==> WARNING: %s%s%s%s\n", col_yellow, no_color, col_bold, text, no_color);
	//PRINT_ALL_ARG(p,count_arg);
	//fprintf(stderr,"%s\n", no_color);
}

int prompt(char * text /*unsigned int count_arg, ...*/){
    //echo -e "${COL_ARROW}==>  ${NO_COLOR}${COL_BOLD}$*${NO_COLOR}" >&2
    fprintf(stderr, "%s==> %s%s%s%s\n", col_arrow, no_color, col_bold, text, no_color)
    //PRINT_ALL_ARG(p,count_arg);
	//fprintf(stderr,"%s\n", no_color);
	//echo -e "${COL_ARROW}==>  ${NO_COLOR}${COL_BOLD} ----------------------------------------------${NO_COLOR}" >&2
	fprintf(stderr,"%s==> %s%s ----------------------------------------------%s\n",
		col_arrow,no_color, col_bold, no_color);
	//echo -ne "${COL_ARROW}==>${NO_COLOR}" >&2
	fprintf(stderr, "%s==>%s", col_arrow, no_color);

}

int promptlight(){
	//echo -ne "${COL_ARROW}==>${NO_COLOR}" >&2
	fprintf(stderr, "%s==>%s", col_arrow, no_color);
}

int error(char * text /*unsigned int count_arg, ...*/){
	//echo -e "${COL_RED}""Error""${NO_COLOR}"": $*\n"
	fprintf(stderr, "%sError%s: %s\n", col_red, no_color);
	//PRINT_ALL_ARG(p,count_arg);
	//putchar('\n');
	return 1
}

int _colorizearg(char * arg){

}

int cleanoutput()
{
    if(!getenv("TERMINALTITLE") || !getenv("DISPlAY")) {
	    return 0
    }
    /* tput sgr0   // Reset color to "normal"  */
}

/**
 * @brief prints \cline in colour
 *
 * @param line a line of text to print
 *
 * @return the number of characters printed
 *
 * @note the format must be in the form, repo/pkgname pkgver (groups),
 *  where  (groups) is optional, otherwise a plain-text print occurs
 * @note \cline must the writable
 */
int colorizeoutputline(char * line)
{
	char *s, *p, *v, *g;
	const char *_col_repo = col_repos;
	int ret = 0;
	s = line;
	while (*s && *s != '/' && !isspace(*s)) ++s;
	if (*s == '/') {
		*s = '\0';
		p = s + 1;
		while (*p && !isspace(*p)) ++p;
		v = p + 1;
		if (*p == ' ' && isdigit(*v)) {
			*p = '\0';
			if (strcmp("core", line) == 0) {
				_col_repo = col_core;
			} else if (strcmp("extra", line) == 0) {
				_col_repo = col_extra;
			} else if (strcmp("community", line) == 0) {
				_col_repo = col_community;
			}
			g = v + 1;
			while (*g && !isspace(*g)) ++g;
			if (*g == ' ') {
				*g = '\0';
				ret = fprintf(stderr, "%s%s/%s%s %s%s %s%s%s", _col_repo, line,
				  col_name, (s + 1), col_version, v, col_group, (g + 1), col_reset);
				*g = ' ';
			} else {
				ret = fprintf(stderr, "%s%s/%s%s %s%s%s",
				  _col_repo, line, col_name, (s + 1), col_version, v, col_reset);
			}
			*p = ' ';
		}
		*s = '/';
	}
	return (ret ? ret : fprintf(stderr, "%s", line));
}

