#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>

#include "common.h"

#define LOGBUFSZ 65536

#define COL_RED "\033[31;01m[ACCT]" //Invalid login
#define COL_YELLOW "\033[33;01m[ACCT]" //Locked login
#define COL_GREEN "\033[32;01m[ACCT]" //Working login
#define COL_GRAY "\033[30;01m[INFO]" //Debug/general
#define COL_RES "\033[0m" //Reset

pthread_mutex_t logkey;
pthread_mutex_t filekey;



void log_locks(void) {
	pthread_mutex_init(&logkey, NULL);
	pthread_mutex_init(&filekey, NULL);
}

void dest_log_locks(void) {
	pthread_mutex_destroy(&logkey);
	pthread_mutex_destroy(&filekey);
}

/*
	Simply write 'log' into 'loc', with locking. Does NOT put a newline afterwards.
*/
static void do_write(const char *log, FILE *loc) {
	pthread_mutex_lock(&logkey);
	fputs(log, loc);
	fflush(loc);
	pthread_mutex_unlock(&logkey);
}

/*
	Variable do_write function
	This uses vfprintf, however.

	Make sure 'fmt' is a fmt, and not user-input..
*/
static void fdo_write(FILE *loc, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	pthread_mutex_lock(&logkey);
	vfprintf(loc, fmt, ap);
	fflush(loc);
	pthread_mutex_unlock(&logkey);

	va_end(ap);
}


/*
	Return appropriate color/tag for the logtype.
*/
static const char* levtag(int logtype) {
	switch(logtype) {
		case VALIDLOG: case VALIDLOGMB: return COL_GREEN;
		case LOCKEDLOG: return COL_YELLOW;
		case INVALIDLOG: return COL_RED;
		case GENLOG: case DBGLOG: return COL_GRAY;
	}
	return 0;
}

/*
	This function is used to output when an account has been tried, and its success/failure, or just for general debug info.
	char* log is the username:password! Unless log=GENLOG||DBGLOG.
	stderr ONLY.
*/
static void logserr(int logtype, const char *log) {
	const char *fmt;

	switch(logtype) {
	case VALIDLOG:
		fmt = "%s[TS:%lu] Login works: %s%s\n";
		break;
	case VALIDLOGMB:
		fmt = "%s[TS:%lu] Login works: %s  --  Account is Members%s\n";
		break;
	case LOCKEDLOG:
		fmt = "%s[TS:%lu] Login works, however it is locked: %s%s\n";
		break;
	case INVALIDLOG:
		fmt = "%s[TS:%lu] Login doesn\'t work: %s%s\n";
		break;
	case DBGLOG:
	case GENLOG:
		fmt = "%s[TS:%lu] %s%s\n";
		break;
	default:
		fmt = "\n"; //Clang warning
		break;
	}

	fdo_write(stderr, fmt, levtag(logtype), time(NULL), log, COL_RES);
}

/*
	Output logins to stdout, without any debugging information.
	Format:    Works: account:password, Locked: account:password, Invalid: account:password
*/
static void logsout(int logtype, const char *login) {

	const char *fmt;

	switch(logtype) {
	case VALIDLOG:
		fmt = "Works: %s\n";
		break;
	case VALIDLOGMB:
		fmt = "Works: %s  --  Is Members\n";
		break;
	case LOCKEDLOG:
		fmt = "Locked: %s\n";
		break;
	case INVALIDLOG:
		fmt = "Invalid: %s\n";
		break;
	case GENLOG:
		fmt = "%s[TS:%lu] %s%s\n";
		break;
	case DBGLOG:
		return;
	default:
		fmt = "\n"; //Clang warning
		break;
	}

	(logtype == GENLOG) ? fdo_write(stdout, fmt, levtag(logtype), time(NULL), login, COL_RES) : fdo_write(stdout, fmt, login);


}
/*
	Write to 'O.basename_xx' files.
	This relies on using the O. struct, with all the files, which have  ~~~ALREADY BEEN OPENED~~~.
*/
static void logacctofile(int logtype, const char *login) {

	switch(logtype) {
	case VALIDLOG:
		fdo_write(O.Valid, "%s\n", login);
		break;
	case VALIDLOGMB:
		fdo_write(O.ValidMb, "%s --  Account is Members\n", login);
		fdo_write(O.Valid, "%s -- Account is Members\n", login);
		break;
	case LOCKEDLOG:
		fdo_write(O.Locked, "%s\n", login);
		break;
	case INVALIDLOG:
		fdo_write(O.Invalid, "%s\n", login);
		break;
	case DBGLOG:
	case GENLOG:
		break;
	}

}

/*
	Do logging. Always use these funtions.
	Inner functions will determine where things need to go.

*/
void do_log(int logtype, const char *log) {


	if(O.verbose && !O.std && logtype == DBGLOG)
		logserr(logtype, log);

	if(logtype != DBGLOG) {
		if((O.validonly && logtype != INVALIDLOG) || !O.validonly)
			O.std ? logsout(logtype, log) : logserr(logtype, log);
	}

	if(O.basename) logacctofile(logtype, log);
}

void fdo_log(int logtype, const char *fmt, ...) {

	va_list ap;
	va_start(ap, fmt);

	int len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	len += 1;
	char *log = malloc((size_t)len);
	if(log == NULL) {
		fprintf(stderr, "\nError in malloc - fdo_log()(%s).", strerror(errno));
		return;
	}

	va_start(ap, fmt);
	len = vsnprintf(log, (size_t)len, fmt, ap);

	if(len >  0) {
		do_log(logtype, log);
	} else {
		fprintf(stderr, "\nError in vsnprintf - fdo_log()(%s).", strerror(errno));
	}

	va_end(ap);

	free(log); log = NULL;

}
