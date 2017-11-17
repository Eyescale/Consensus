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
	newIdentifier, freeIdentifier
---------------------------------------------------------------------------*/
IdentifierVA *
newIdentifier( void )
{
	return newRegistryEntry( IndexedByName, NULL, NULL );
}

void
freeIdentifier( IdentifierVA *string )
{
	freeListItem( (listItem **) &string->value );
	free( string->index.name );
	freeRegistryEntry( IndexedByName, string );
}

/*---------------------------------------------------------------------------
	string_expand
---------------------------------------------------------------------------*/
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
expand_b( listItem **base, IdentifierVA *string )
{
	listItem *results = NULL;

	string_finish( string, 0 );
	if (( string->index.name )) {
		listItem *sub = newItem( string->index.name );
		results = (*base) ? expand_s( base, &sub ) : sub;
		string->index.name = NULL;
	}
	else results = *base;
	return results;
}

listItem *
string_expand( char *identifier, char **next_pos )
{
	listItem *backlog = NULL;
	listItem *results = NULL;
	listItem *sub_results = NULL;
	IdentifierVA *string = newIdentifier();
	for ( char *src = identifier; ; src++ ) {
		switch ( *src ) {
		case 0:
		case '}':
			backlog = expand_b( &backlog, string );
			if (( next_pos )) *next_pos = src;
			freeIdentifier( string );
			return catListItem( results, backlog );
		case ',':
			backlog = expand_b( &backlog, string );
			results = catListItem( results, backlog );
			backlog = NULL;
			break;
		case '\\':
			switch ( src[ 1 ] ) {
			case 0:
				string_append( string, '\\' );
				break;
			case '"':
			case 'n':
			case 't':
				string_append( string, '\\' );
				// no break
			default:
				src++;
				string_append( string, *src );
			}
			break;
		case ' ':
			break;
		case '{':
			backlog = expand_b( &backlog, string );
			sub_results = string_expand( src + 1, &src );
			backlog = expand_s( &backlog, &sub_results );
			if ( *src == (char) 0 ) {
				if (( next_pos )) *next_pos = src;
				freeIdentifier( string );
				return catListItem( results, backlog );
			}
			break;
		default:
			string_append( string, *src );
		}
	}
}

/*---------------------------------------------------------------------------
	stringify
---------------------------------------------------------------------------*/
void
slist_close( listItem **slist, IdentifierVA *dst, int cleanup )
{
	if (( dst->value )) {
		char *str = string_finish( dst, 0 );
		if ( strcmp( str, "\n" ) ) {
			addItem( slist, str );
		}
		else free( str );
		dst->index.name = NULL;
	}
}
void
slist_append( listItem **slist, IdentifierVA *dst, int event, int nocr, int cleanup )
{
	if ( dst->value == NULL ) {
		string_start( dst, event );
	} else if (( event != '\n' ) || !nocr ) {
		string_append( dst, event );
	}
	if ( event == '\n' ) slist_close( slist, dst, cleanup );
}
listItem *
stringify( listItem *src, int cleanup )
{
	listItem *results = NULL;
	IdentifierVA *dst = newIdentifier();
	for ( listItem *i = src; i!=NULL; i=i->next )
		for ( char *ptr = i->ptr; *ptr; ptr++ )
			slist_append( &results, dst, *ptr, 1, cleanup );
	slist_close( &results, dst, cleanup );
	freeIdentifier( dst );
	return results;
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
	freeListItem( (listItem **) &string->value );
	free( string->index.name );
	string->index.name = NULL;
	if ( event ) {
		string_append( string, event );
	}
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
	union { void *ptr; int event; } data;
	data.ptr = NULL;
	data.event = event;
	addItem( (listItem**) &string->value, data.ptr );
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
	listItem **list = (listItem **) &string->value;
	if ( *list == NULL ) return NULL;

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
	string->index.name = (char *) malloc( count + 1 );
	char *ptr = string->index.name;
	for ( i = *list, count=0; i!=NULL; i=i->next, count++ ) {
		*ptr++ = (char) i->ptr;
	}
	*ptr = 0;

	freeListItem( list );
	return string->index.name;
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
	int i, pos, m = strlen( value ), n = strlen( identifier ), maxpos = strlen( expression ) - n;
	char *ptr1=expression, *ptr2, *ptr3, *ptr4;

	IdentifierVA *result = newIdentifier();
	for ( pos=0; ( pos < maxpos ) && *ptr1; )
	{
		pos++;
		if ( *ptr1 != '%' ) {
			if ( result->value != NULL )
				string_append( result, *ptr1 );
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
			if ( result->value != NULL )
				string_append( result, '%' );
			continue;
		}

		if ( pos == 1 ) {
			ptr4 = value;
			string_start( result, *ptr4++ );
			for ( i=1; i<m; i++ )
				string_append( result, *ptr4++ );
		}
		else {
			if ( result->value == NULL ) {
				ptr3 = expression;
				string_start( result, *ptr3++ );
				for ( i=2; i<pos; i++ )
					string_append( result, *ptr3++ );
			}
			ptr4 = value;
			for ( i=0; i<m; i++ ) {
				string_append( result, *ptr4++ );
			}
		}
		pos += n; ptr1 += n;
	}

	char *retval;
	if ( result->value != NULL )
	{
		n = strlen( expression );
		for ( ; pos<n; pos++ ) {
			string_append( result, *ptr1++ );
		}
		char *retval = string_finish( result, 0 );
		result->index.name = NULL;
	}
	else retval = expression;

	freeIdentifier( result );
	return retval;
}

