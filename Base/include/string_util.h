#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "pair.h"
#include "list.h"
#include "prune.h"

//===========================================================================
//	CNString Interface
//===========================================================================
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

int	StringStart( CNString *, int event );
int	StringAppend( CNString *, int event );
int	StringAffix( CNString *, int event );

int	StringInformed( CNString * );
int	StringAt( CNString * );
int	StringCompare( CNString *, char * );

typedef enum {
	CNStringMode = 0,
	CNStringAll
} CNStringReset;

char *	StringFinish( CNString *, int trim );
void	StringReset( CNString *, CNStringReset );

//---------------------------------------------------------------------------
//	CNString macros
//---------------------------------------------------------------------------

#define s_take \
	StringAppend( s, event ); /* Assumption: event externally defined */
#define s_put( event ) \
	StringAppend( s, event );
#define s_add( str ) \
	for ( char *_c=str; *_c; StringAppend(s,*_c++) );
#define s_append( bgn, end ) \
	for ( char *_c=bgn; _c!=end; StringAppend(s,*_c++) );
#define	s_reset( a ) \
	StringReset( s, a );

#define s_empty \
	!StringInformed(s)
#define	s_at( event ) \
	( StringAt(s)==event )
#define s_cmp( str ) \
	StringCompare( s, str )

//===========================================================================
//	C string utilities
//===========================================================================
typedef union { char s[4]; int value; } char_s;

int	is_separator( int event );
int	is_printable( int event );
int	is_space( int event );
int	is_escapable( int event );
int	is_xdigit( int event );
int	isanumber( char *string );
int	charscan( char *p, char_s *q );
int	rxcmp( char *, int );

char *	strmake( char * );
int	strcomp( char *, char *, int );
char *	strscanid( char *, char ** );
int 	strmatch( char *, int event );
char *	strskip( char *fmt, char *str );
char *	strxchange( char *expression, char *identifier, char *value );


#endif	// STRING_UTIL_H
