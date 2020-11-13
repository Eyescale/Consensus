#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "pair.h"

#define MAXCHARSIZE 1

int	is_separator( int event );
int	is_character( int event );
int	is_space( int event );
int	is_escapable( int event );
int	is_xdigit( int event );
int	isanumber( char *string );
int	charscan( char *p, char *q );
int	tokcmp( const char *p, const char *q );

enum {
	CNStringMode = 0,
	CNStringAll
};

typedef enum {
	CNStringBytes = 0,
	CNStringText
} CNStringState;

typedef struct {
	void *data;
	CNStringState mode;
} CNString;

CNString *newString( void );
void	freeString( CNString * );

int	StringInformed( CNString *s );
int	StringAt( CNString *s );
void	StringReset( CNString *string, int );
int	StringStart( CNString *string, int event );
int	StringAppend( CNString *string, int event );
char *	StringFinish( CNString *string, int trim );
char *	l2s( listItem **, int trim );

char *	strmake( char * );
char *	strscanid( char *, char ** );
int 	strmatch( char *, int event );
char *	strskip( char *fmt, char *str );
char *	strxchange( char *expression, char *identifier, char *value );
listItem *strxpand( char *string, char **next_pos );


#endif	// STRING_UTIL_H
