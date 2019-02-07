#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	is_separator
---------------------------------------------------------------------------*/
int
is_separator( int event )
{
	return !( isalnum(event) || event == '_' );
}

/*---------------------------------------------------------------------------
	is_space
---------------------------------------------------------------------------*/
int
is_space( int event )
{
	switch ( event ) {
	case ' ':
	case '\t':
		return 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	isanumber
---------------------------------------------------------------------------*/
int
isanumber( char *string )
{
	for ( char *str = string; *str; str++ )
		if ( !isdigit( *str ) )
			return 0;
	return 1; 
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
	listItem **list = (listItem **) &string->data;
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
	string->mode = CNStringText;
	string->data = str;
	return str;
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
	do { if ( event == *s ) return 1; }
	while ( *s++ );
	return 0;
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

/*---------------------------------------------------------------------------
	strxpand
---------------------------------------------------------------------------*/
static listItem *expand_s( listItem **base, listItem **sub );
static listItem *expand_b( listItem **base, CNString * );

listItem *
strxpand( char *identifier, char **next_pos )
{
	listItem *backlog = NULL;
	listItem *results = NULL;
	listItem *sub_results = NULL;
	CNString *string = newString();
	int inq = 0;
	for ( char *src = identifier; ; src++ ) {
		if ( inq ) {
			switch ( *src ) {
			case '"':
				StringAppend( string, *src++ );
				// no break
			case 0:
				inq = 0;
				break;
			case '\\':
				StringAppend( string, *src++ );
				if ( ! *src ) {
					inq = 0;
					break;
				}
				// no break
			default:
				StringAppend( string, *src );
			}
			if ( inq ) continue;
		}
		switch ( *src ) {
		case '{':
			backlog = expand_b( &backlog, string );
			sub_results = strxpand( src + 1, &src );
			backlog = expand_s( &backlog, &sub_results );
			if ( *src == (char) 0 ) {
				if (( next_pos )) *next_pos = src;
				freeString( string );
				return catListItem( results, backlog );
			}
			break;
		case ',':
			backlog = expand_b( &backlog, string );
			results = catListItem( results, backlog );
			backlog = NULL;
			break;
		case '}':
		case 0:
			backlog = expand_b( &backlog, string );
			if (( next_pos )) *next_pos = src;
			freeString( string );
			return catListItem( results, backlog );
		case ' ':
			break;
		case '"':
			StringAppend( string, *src );
			inq = 1;
			break;
		case '\\':
			switch ( src[ 1 ] ) {
			case 0:
				StringAppend( string, '\\' );
				break;
			default:
				StringAppend( string, *src++ );
				StringAppend( string, *src );
			}
			break;
		default:
			StringAppend( string, *src );
		}
	}
}

static listItem *
expand_s( listItem **base, listItem **sub )
{
	listItem *results = NULL;

	if ( *sub == NULL )
		return *base;
	if ( *base == NULL ) {
		results = *sub;
		*sub = NULL;
		return results;
	}
	for ( listItem *i = *base; i!=NULL; i=i->next ) {
		for ( listItem *j = *sub; j!=NULL; j=j->next ) {
			char *ns;
			asprintf( &ns, "%s%s", i->ptr, j->ptr );
			addItem( &results, ns );
		}
		free( i->ptr );
	}
	freeListItem( base );

	for ( listItem *i = *sub; i!=NULL; i=i->next )
		free( i->ptr );
	freeListItem( sub );

	return results;
}
static listItem *
expand_b( listItem **base, CNString *string )
{
	listItem *results = NULL;

	StringFinish( string, 0 );
	if (( string->data )) {
		listItem *sub = newItem( string->data );
		results = (*base) ? expand_s( base, &sub ) : sub;
		string->data = NULL;
	}
	else results = *base;
	return results;
}

