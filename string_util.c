#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "string_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	is_separator
---------------------------------------------------------------------------*/
int
is_separator( int event )
{
	switch ( event ) {
	case 0:
	case -1:
	case ' ':
	case '\t':
	case '\n':
	case '.':
	case ',':
	case ';':
	case ':':
	case '(':
	case ')':
	case '{':
	case '}':
	case '[':
	case ']':
	case '<':
	case '>':
	case '-':
	case '=':
	case '/':
	case '\\':
	case '%':
	case '?':
	case '!':
		return event;
	default:
		return !event;
	}
}

/*---------------------------------------------------------------------------
	string_start
---------------------------------------------------------------------------*/
int
string_start( IdentifierVA *string, int event )
{
#ifdef DEBUG_2
	output( Debug, "string_start: '%c'", (char) event );
#endif
	listItem **list = &string->list;
	union {
		void *ptr;
		int event;
	} data;
	data.event = event;

	freeListItem( list );
	listItem *item = newItem( data.ptr );
	item->next = NULL;
	*list = item;

	return 0;
}

/*---------------------------------------------------------------------------
	string_append
---------------------------------------------------------------------------*/
int
string_append( IdentifierVA *string, int event )
{
#ifdef DEBUG_2
	output( Debug, "string_append: '%c'", (char) event );
#endif
	listItem **list = &string->list;
	union {
		void *ptr;
		int event;
	} data;
	data.event = event;

	listItem *item = newItem( data.ptr );
	item->next = *list;
	*list = item;

	return 0;
}

/*---------------------------------------------------------------------------
	string_finish
---------------------------------------------------------------------------*/
char *
string_finish( IdentifierVA *string, int cleanup )
{
#ifdef DEBUG_2
	output( Debug, "string_finish: '%c'", (char) event );
#endif
	listItem **list = &string->list;
	listItem *i, *next_i;
	int count = 0;

	// remove trailing white space
	if ( cleanup )
	{
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
				freeItem( i );
				*list = next_i;
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
	if ( cleanup )
	{
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
	string->ptr = (char *) malloc( count + 1 );
	for ( i = *list, count=0; i!=NULL; i=i->next, count++ ) {
		string->ptr[ count ] = (char) i->ptr;
	}
	string->ptr[ count ] = 0;

	freeListItem( list );
	return string->ptr;
}

/*---------------------------------------------------------------------------
	string_assign
---------------------------------------------------------------------------*/
int
string_assign( char **target, int id, _context *context )
{
	*target = context->identifier.id[ id ].ptr;
	context->identifier.id[ id ].ptr = NULL;
	return 0;
}

/*---------------------------------------------------------------------------
	string_extract
---------------------------------------------------------------------------*/
char *
string_extract( char *identifier )
{
	if ( identifier[0] != '\"' )
	 	return NULL;

	int size = strlen( identifier ) - 1;
	if ( size < 2 )
		return NULL;

	char *string = (char *) malloc( size );
	size--; identifier++;
	bcopy( identifier, string, size );
	string[ size ] = '\0';
	return string;
}

/*---------------------------------------------------------------------------
	string_replace
---------------------------------------------------------------------------*/
char *
string_replace( char *expression, char *identifier, char *value )
{
	IdentifierVA result;
	result.list = NULL;
	result.ptr = NULL;
	char *ptr1=expression, *ptr2, *ptr3, *ptr4;
	int i, pos, m = strlen( value ), n = strlen( identifier ), maxpos = strlen( expression ) - n;
	for ( pos=0; ( pos < maxpos ) && *ptr1; )
	{
		pos++;
		if ( *ptr1 != '%' ) {
			if ( result.list != NULL )
				string_append( &result, *ptr1 );
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
			if ( result.list != NULL )
				string_append( &result, '%' );
			continue;
		}

		if ( pos == 1 ) {
			ptr4 = value;
			string_start( &result, *ptr4++ );
			for ( i=1; i<m; i++ )
				string_append( &result, *ptr4++ );
		}
		else {
			if ( result.list == NULL ) {
				ptr3 = expression;
				string_start( &result, *ptr3++ );
				for ( i=2; i<pos; i++ )
					string_append( &result, *ptr3++ );
			}
			ptr4 = value;
			for ( i=0; i<m; i++ ) {
				string_append( &result, *ptr4++ );
			}
		}
		pos += n; ptr1 += n;
	}
	if ( result.list != NULL )
	{
		n = strlen( expression );
		for ( ; pos<n; pos++ ) {
			string_append( &result, *ptr1++ );
		}
		return string_finish( &result, 0 );
	}
	else return expression;
}

