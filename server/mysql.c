#include <ctype.h>
#include <my_global.h>
#include <mysql.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned short ID;
    unsigned char AccountTypeID;
    unsigned char Suspended;
    char Username[33];
    char Email[65];
    char Passwd[33];
    char Realname[65];
    char Langpreference[3];
    char IRCNick[33];
    unsigned long LastVoted;
    unsigned char NewPkgNotify;
} User;

/* Trim whitespace and newlines from a string */  // includolni !!!
char *strtrim(char *str)
{
    char *pch = str;
    if(str == NULL || *str == '\0') {
        /* string is empty, so we're done. */
	return(str);
    }
    while(isspace(*pch)) {
	pch++;
    }
    if(pch != str) {
	memmove(str, pch, (strlen(pch) + 1));
    }
    /* check if there wasn't anything but whitespace in the string. */
        if(*str == '\0') {
	    return(str);
	}
    pch = (str + (strlen(str) - 1));
    while(isspace(*pch)) {
	pch--;
    }
    *++pch = '\0';
    return(str);
}

int am_insert(MYSQL *conn, User *user_data)
{
    char tmp_str[300];
    snprintf(tmp_str, sizeof(tmp_str),
	     "INSERT INTO Users VALUES(%d,%d,%d,'%s','%s','%s','%s','%s','%s',%lu,%d);",
	     user_data->ID, user_data->AccountTypeID, user_data->Suspended,
	     user_data->Username, user_data->Email, user_data->Passwd,
	     user_data->Realname, user_data->Langpreference, user_data->IRCNick,
	     user_data->LastVoted, user_data->NewPkgNotify);
    return mysql_query(conn, tmp_str);
}

MYSQL *am_connect()
{
    MYSQL *conn = NULL;
    char *server = "localhost";
    int port = 0;
    char *user = "aur";
    char *password = "aur";
    char *database = "AUR";
    
    conn = mysql_init(NULL);
    if (!conn) {
	fprintf(stderr, "Out of memory!\n");
	exit(1);
    }

    /* Connect to database */
    if (!mysql_real_connect(conn, server,
	    user, password, database, port, NULL, 0)) {
	fprintf(stderr, "%s\n", mysql_error(conn));
	exit(1);
    }
    return conn;
}

char *am_fgets(char *s, int size, FILE *stream)
{
    char *tmp_str=NULL;
    int n;
    getline(&tmp_str, &n, stream);
    strtrim(tmp_str);
    n=strlen(tmp_str);
    if (n > 0) {
        strncpy(s, tmp_str, size-1);
	if (n > size) s[size-1]=0;
    }
    if (tmp_str != NULL) free(tmp_str);
    return s;
}

int am_get_user_data(User *user)
{
#define PASS_MAX 100
    char *passwd;
    printf("Login name for new user: [%s]", user->Username);
    am_fgets(user->Username, sizeof(user->Username), stdin);

    printf("Enter a valid e-mail address for the new user: [%s]", user->Email);
    am_fgets(user->Email, sizeof(user->Email), stdin);

    passwd=getpass("Password: ");
    strncpy(user->Passwd, passwd, sizeof(user->Passwd));

    passwd=getpass("Re-type the password: ");

    if (strncmp(user->Passwd, passwd, sizeof(user->Passwd))) {
	memset(passwd, 0, PASS_MAX);
	printf("bad password");
	return -1;
    }
    memset(passwd, 0, PASS_MAX);

    printf("Real name: [%s]", user->Realname);
    am_fgets(user->Realname, sizeof(user->Realname), stdin);

    printf("IRCNick: [%s]", user->IRCNick);
    am_fgets(user->IRCNick, sizeof(user->IRCNick), stdin);

    printf("Langpreference: [%s]", user->Langpreference);
    am_fgets(user->Langpreference, sizeof(user->Langpreference), stdin);

//    printf("Notify for new packages [%c]: ", ...); !!!
    return 0;
}

int am_print_user_data(User *user)
{
    printf("\nLogin name: %s\n", user->Username);
    printf("e-mail address: %s\n", user->Email);
    printf("Real name: %s\n", user->Realname);
    printf("IRCNick: %s\n", user->IRCNick);
    printf("Langpreference: %s\n", user->Langpreference);
}

int main(int argc, char *argv[])
{
    char tmp_str[200];

    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    conn = am_connect();

    User aur_user;
    memset(&aur_user, 0, sizeof(User));
    do {
        am_get_user_data(&aur_user);
	am_print_user_data(&aur_user);
        printf("Are these informations correct? [N]: ");
	am_fgets(tmp_str, 3, stdin);
    } while (strcasecmp(tmp_str, "Y"));

    if (am_insert(conn, &aur_user)) {
	fprintf(stderr, "%s\n", mysql_error(conn));
	exit(1);
    }
    
    /* close connection */
    mysql_close(conn);
}
