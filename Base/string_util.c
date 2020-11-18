#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "string_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	is_separator, ...
---------------------------------------------------------------------------*/
int
is_separator( int event )
{
	return  ((( event > 96 ) && ( event < 123 )) ||		/* a-z */
		 (( event > 64 ) && ( event < 91 )) ||		/* A-Z */
		 (( event > 47 ) && ( event < 58 )) ||		/* 0-9 */
		  ( event == 95 )) ? 0 : 1;			/* _ */
}
int
is_character( int event )
{
	return ((event > 31 ) && ( event < 127 ));
}
int
is_space( int event )
{
	return strmatch( " \t", event );
}
int
is_escapable( int event )
{
	// Note: includes neither 'x' nor '\"'
	return strmatch( "0tn\'\\", event );
}
int
is_xdigit( int event )
{
	// Note: not including lower case
	return strmatch( "0123456789ABCDEF", event );
}
int
isanumber( char *p )
{
	if (!((p) && *p )) return 0;
	else
		do if (( *p > 47 ) && ( *p < 58 )) { p++; }	/* 0-9 */
		while ( *p );

	return 1; 
}
#define HVAL(c) (c-((c<58)?48:(c<97)?55:87))
int
charscan( char *p, char *q )
{
	q[ MAXCHARSIZE ] = (char)0;
	switch ( *p ) {
	case '\\':
		switch ( p[1] ) {
		case 'x':  q[ 0 ] = HVAL(p[2])<<4 | HVAL(p[3]); return 4;
		case '0':  q[ 0 ] = '\0'; return 2;
		case 't':  q[ 0 ] = '\t'; return 2;
		case 'n':  q[ 0 ] = '\n'; return 2;
		case '\\': q[ 0 ] = '\\'; return 2;
		case '\'': q[ 0 ] = '\''; return 2;
		case '\"': q[ 0 ] = '\"'; return 2;
		default: return 0;
		}
		break;
	default:
		q[ 0 ] = *p; return 1;
	}
}
int
tokcmp( const char *p, const char *q )
{
	if ( is_separator(*p) ) {
		if ( !is_separator(*q) ) return - *(const unsigned char*)q;
		return *(const unsigned char*)p - *(const unsigned char*)q;
	}
	else if ( is_separator(*q) ) {
		return *(const unsigned char*)p;
	}
	else for ( ; ; p++, q++ ) {
		if ( is_separator(*p) )
			return is_separator(*q) ? 0 : - *(const unsigned char*)q;
		else if ( is_separator(*q) )
			return *(const unsigned char*)p;
		else if ( *p != *q )
			return *(const unsigned char*)p - *(const unsigned char*)q;
	}
}

/*---------------------------------------------------------------------------
	newString, freeString
---------------------------------------------------------------------------*/
CNString *
newString( void )
{
	union { int value; char *ptr; } icast;
	icast.value = CNStringBytes;
	return (CNString *) newPair( NULL, icast.ptr );
}
void
freeString( CNString *string )
{
	switch ( string->mode ) {
	case CNStringBytes:
		freeListItem( (listItem **) &string->data );
		break;
	case CNStringText:
		free( string->data );
		break;
	}
	freePair( (Pair *) string );
}
int
StringInformed( CNString *string )
{
	return ( string->data != NULL );
}
int
StringAt( CNString *string )
{
	if ( string->data == NULL ) return 0;
	switch ( string->mode ) {
	case CNStringBytes:
		return (int)((listItem *)string->data)->ptr;
	case CNStringText:
		return *(char *)string->data;
	}
}
/*---------------------------------------------------------------------------
	StringReset
---------------------------------------------------------------------------*/
void
StringReset( CNString *string, int target )
{
	switch ( target ) {
	case CNStringAll:
		StringStart( string, 0 );
		break;
	case CNStringMode:
		string->data = NULL;
		string->mode = CNStringBytes;
		break;
	}
}

/*---------------------------------------------------------------------------
	StringStart
---------------------------------------------------------------------------*/
int
StringStart( CNString *string, int event )
{
#ifdef DEBUG_2
	output( Debug, "StringStart: '%c'", (char) event );
#endif
	switch ( string->mode ) {
	case CNStringBytes:
		freeListItem( (listItem **) &string->data );
		break;
	case CNStringText:
		free( string->data );
		string->data = NULL;
		string->mode = CNStringBytes;
		break;
	}
	if ( event ) {
		StringAppend( string, event );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	StringAppend
---------------------------------------------------------------------------*/
int
StringAppend( CNString *string, int event )
/*
	Assumption: string->mode == CNStringBytes
*/
{
#ifdef DEBUG_2
	output( Debug, "StringAppend: '%c'", (char) event );
#endif
	union { void *ptr; int event; } icast;
	icast.event = event;
	addItem( (listItem**) &string->data, icast.ptr );
	return 0;
}

/*---------------------------------------------------------------------------
	StringFinish
---------------------------------------------------------------------------*/
char *
StringFinish( CNString *string, int trim )
/*
	Assumption: string->mode == CNStringBytes
*/
{
#ifdef DEBUG_2
	output( Debug, "StringFinish: '%c'", (char) event );
#endif
	char *str = l2s((listItem **) &string->data, trim );
	string->mode = CNStringText;
	string->data = str;
	return str;
}

char *
l2s( listItem **list, int trim )
{
	if ( *list == NULL ) return NULL;
	listItem *i, *next_i;
	int count = 0;

	// remove trailing white space
	if ( trim ) {
		listItem *backup = NULL;
		i = *list;
		if ((char) i->ptr == '\n' ) {
			backup = i;
			*list = i->next;
		}	
		for ( i = *list; i!=NULL; i=next_i ) {
			switch ((char) i->ptr) {
			case ' ':
			case '\t':
				next_i = i->next;
				if (( next_i ) && ((char) next_i->ptr == '\\' ))
					next_i = NULL;
				else {
					freeItem( i );
					*list = next_i;
				}
				break;
			default:
				next_i = NULL;
			}
		}
		if ( backup ) {
			backup->next = *list;
			*list = backup;
		}
	}

	count = reorderListItem( list );

	// remove leading white space
	if ( trim ) {
		for ( i = *list; i!=NULL; i=next_i ) {
			switch ((char) i->ptr) {
			case ' ':
			case '\t':
				count--;
				next_i = i->next;
				freeItem( i );
				*list = next_i;
				break;
			default:
				next_i = NULL;
			}
		}
	}

	// allocate string
	char *str = (char *) malloc( count + 1 );
	char *ptr = str;
	for ( i = *list; i!=NULL; i=i->next ) {
		*ptr++ = (char) i->ptr;
	}
	*ptr = 0;
	freeListItem( list );
	return str;
}

/*---------------------------------------------------------------------------
	strmake
---------------------------------------------------------------------------*/
char *
strmake( char *p )
{
	CNString *s = newString();
	if ( is_separator(*p) )
		StringAppend( s, *p );
	else do StringAppend( s, *p++ );
		while ( !is_separator(*p) );

	p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return p;
}

/*---------------------------------------------------------------------------
	strscanid
---------------------------------------------------------------------------*/
char *
strscanid( char *str, char **identifier )
{
	*identifier = NULL;

	// measure identifier size
	char *ptr = strskip( "%.", str++ );
	if ( ptr == NULL ) return NULL;

	int n = (uintptr_t) ptr - (uintptr_t) str;
	if ( !n ) return NULL;

	// allocate and inform identifier
	ptr = malloc( n + 1 );
	memcpy( ptr, str, n );
	ptr[ n ] = (char) 0;

	*identifier = ptr;
	return str + n;
}

/*---------------------------------------------------------------------------
	strmatch
---------------------------------------------------------------------------*/
int
strmatch( char *s, int event )
{
	if ( *s ) {
		do { if ( event == *s++ ) return 1; } while ( *s );
		return 0;
	}
	return event ? 0 : 1;
}

/*---------------------------------------------------------------------------
	strskip
---------------------------------------------------------------------------*/
char *
strskip( char *fmt, char *str )
{
	if ( str == NULL ) return NULL;
	for ( ; *fmt; fmt++ ) {
		switch ( *fmt ) {
		case ' ': // skip space: { \ \t }
			for ( int done=0; *str && !done; )
				switch ( *str ) {
				case ' ':
				case '\t':
					str++;
					break;
				default:
					done = 1;
				}
			break;
		case '.': // skip identifier: { [A-Za-z0-9_] }
			for ( int done=0; *str && !done; ) {
				if ( !is_separator( *str ) )
					str++;
				else done = 1;
			}
			break;
		case '\\': // to allow the above in the format
			fmt++;
			switch ( *str ) {
			case '\0': if ( *fmt == '0' )
					str++;
				else return NULL;
				break;
			default:
				switch ( *fmt ) {
				case '0':
					return NULL;
				case 't': if (*str++!='\t')
						return NULL;
					break;
				case 'n': if (*str++ != '\n' )
						return NULL;
					break;
				case '\\': if (*str++ != '\\' )
						return NULL;
					break;
				default: if ( *str++ != *fmt )
						return NULL;
				}
				break;
			}
			break;
		default: // must match
			switch ( *str ) {
			case '\0':
				return NULL;
			default: if ( *str++ != *fmt )
				return NULL;
			}
		}
	}
	return str;
}

/*---------------------------------------------------------------------------
	strxchange
---------------------------------------------------------------------------*/
char *
strxchange( char *expression, char *identifier, char *value )
{
	int i, pos, m = strlen( value ), n = strlen( identifier ), maxpos = strlen( expression ) - n;
	char *ptr1=expression, *ptr2, *ptr3, *ptr4;

	CNString *result = newString();
	for ( pos=0; ( pos < maxpos ) && *ptr1; )
	{
		pos++;
		if ( *ptr1 != '%' ) {
			if ( result->data != NULL )
				StringAppend( result, *ptr1 );
			ptr1++;
			continue;
		}
		ptr1++;

		int found = 1;
		ptr2 = identifier;
		for ( i=0; (i<n) && found; i++ ) {
			// output( Debug, "\ncomparing %c %c\n", ptr1[i], *ptr2 );
			if ( ptr1[ i ] != *ptr2++ )
				found = 0;
		}
		if ( !found ) {
			if ( result->data != NULL )
				StringAppend( result, '%' );
			continue;
		}

		if ( pos == 1 ) {
			ptr4 = value;
			StringStart( result, *ptr4++ );
			for ( i=1; i<m; i++ )
				StringAppend( result, *ptr4++ );
		}
		else {
			if ( result->data == NULL ) {
				ptr3 = expression;
				StringStart( result, *ptr3++ );
				for ( i=2; i<pos; i++ )
					StringAppend( result, *ptr3++ );
			}
			ptr4 = value;
			for ( i=0; i<m; i++ ) {
				StringAppend( result, *ptr4++ );
			}
		}
		pos += n; ptr1 += n;
	}

	char *retval;
	if ( result->data != NULL )
	{
		n = strlen( expression );
		for ( ; pos<n; pos++ ) {
			StringAppend( result, *ptr1++ );
		}
		char *retval = StringFinish( result, 0 );
	}
	else retval = expression;

	freeString( result );
	return retval;
}
