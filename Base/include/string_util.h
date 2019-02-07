#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "pair.h"

int	is_separator( int event );
int	is_space( int event );
int	isanumber( char *string );

typedef struct {
	void *data;
	int mode;
} CNString;

enum {
	CNStringBytes = 0,
	CNStringText
};
enum {
	CNStringMode = 0,
	CNStringAll
};

CNString *newString( void );
void	freeString( CNString * );

void	StringReset( CNString *string, int );
int	StringStart( CNString *string, int event );
int	StringAppend( CNString *string, int event );
char *	StringFinish( CNString *string, int trim );

char *	strscanid( char *, char ** );
int 	strmatch( char *, int event );
char *	strskip( char *fmt, char *str );
char *	strxchange( char *expression, char *identifier, char *value );
listItem *strxpand( char *string, char **next_pos );


#endif	// STRING_UTIL_H
