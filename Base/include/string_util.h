#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "pair.h"
#include "list.h"

typedef union { char s[4]; int value; } char_s;

int	is_separator( int event );
int	is_printable( int event );
int	is_space( int event );
int	is_escapable( int event );
int	is_xdigit( int event );
int	isanumber( char *string );
int	charscan( char *p, char_s *q );


typedef enum {
	PRUNE_TERNARY = 1,
	PRUNE_TERM,
	PRUNE_FILTER,
	PRUNE_LITERAL,
	PRUNE_LIST,
	PRUNE_IDENTIFIER
} PruneType;
char *	p_prune( PruneType type, char * );
int	p_filtered( char * );
int	p_ternary( char * );
int	p_list( char * );
int	p_single( char * );

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

int	StringStart( CNString *string, int event );
int	StringAppend( CNString *string, int event );
int	StringAffix( CNString *string, int event );
void	StringReset( CNString *string, int );
char *	StringFinish( CNString *string, int trim );
char *	l2s( listItem **, int trim );
int	StringInformed( CNString *s );
int	StringAt( CNString *s );
int	StringCompare( CNString *, char * );

char *	strmake( char * );
int	strcomp( char *, char *, int );
int	rxcmp( char *, int );
char *	strscanid( char *, char ** );
int 	strmatch( char *, int event );
char *	strskip( char *fmt, char *str );
char *	strxchange( char *expression, char *identifier, char *value );


#endif	// STRING_UTIL_H
