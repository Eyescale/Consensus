#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "output.h"
#include "string_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	isanumber
---------------------------------------------------------------------------*/
int
isanumber( char *string )
{
	int len = strlen( string );
	for ( int i=0; i<len; i++ )
		if ( !isdigit( string[i] ) )
			return 0;
	return 1; 
}

/*---------------------------------------------------------------------------
	newString, freeString
---------------------------------------------------------------------------*/
StringVA *
newString( void )
{
	int mode = STRING_EVENTS;
	StringVA *string = newRegistryEntry( IndexedByNumber, &mode, NULL );
	return string;
}

void
freeString( StringVA *string )
{
	switch ( string->index.value ) {
	case STRING_EVENTS:
		freeListItem( (listItem **) &string->value );
		break;
	case STRING_TEXT:
		free( string->value );
		break;
	}
	freeRegistryEntry( IndexedByNumber, string );
}

/*---------------------------------------------------------------------------
	string_start
---------------------------------------------------------------------------*/
int
string_start( StringVA *string, int event )
{
#ifdef DEBUG_2
	output( Debug, "string_start: '%c'", (char) event );
#endif
	switch ( string->index.value ) {
	case STRING_EVENTS:
		freeListItem( (listItem **) &string->value );
		break;
	case STRING_TEXT:
		free( string->value );
		string->value = NULL;
		string->index.value = STRING_EVENTS;
		break;
	}
	if ( event ) {
		string_append( string, event );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	string_append
---------------------------------------------------------------------------*/
int
string_append( StringVA *string, int event )
/*
	Assumption: string->index.value == STRING_EVENTS
*/
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
string_finish( StringVA *string, int cleanup )
/*
	Assumption: string->index.value == STRING_EVENTS
*/
{
#ifdef DEBUG_2
	output( Debug, "string_finish: '%c'", (char) event );
#endif
	listItem **list = (listItem **) &string->value;
	if ( *list == NULL ) return NULL;

	listItem *i, *next_i;
	int count = 0;

	// remove trailing white space
	if ( cleanup ) {
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
	if ( cleanup ) {
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
	for ( i = *list, count=0; i!=NULL; i=i->next, count++ ) {
		*ptr++ = (char) i->ptr;
	}
	*ptr = 0;

	freeListItem( list );
	string->index.value = STRING_TEXT;
	string->value = str;
	return str;
}

/*---------------------------------------------------------------------------
	stringify
---------------------------------------------------------------------------*/
void
slist_close( listItem **slist, StringVA *dst, int cleanup )
{
	if (( dst->value )) {
		char *str = string_finish( dst, 0 );
		if ( strcmp( str, "\n" ) ) {
			addItem( slist, str );
		}
		else free( str );
		dst->value = NULL;
	}
}
void
slist_append( listItem **slist, StringVA *dst, int event, int nocr, int cleanup )
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
	StringVA *dst = newString();
	for ( listItem *i = src; i!=NULL; i=i->next )
		for ( char *ptr = i->ptr; *ptr; ptr++ )
			slist_append( &results, dst, *ptr, 1, cleanup );
	slist_close( &results, dst, cleanup );
	freeString( dst );
	return results;
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
	memcpy( string, identifier, size );
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

	StringVA *result = newString();
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

	freeString( result );
	return retval;
}

/*---------------------------------------------------------------------------
	string_expand
---------------------------------------------------------------------------*/
static listItem *expand_s( listItem **base, listItem **sub );
static listItem *expand_b( listItem **base, StringVA * );

listItem *
string_expand( char *identifier, char **next_pos )
{
	listItem *backlog = NULL;
	listItem *results = NULL;
	listItem *sub_results = NULL;
	StringVA *string = newString();
	int inq = 0;
	for ( char *src = identifier; ; src++ ) {
		if ( inq ) {
			switch ( *src ) {
			case '"':
				string_append( string, *src++ );
				// no break
			case 0:
				inq = 0;
				break;
			case '\\':
				string_append( string, *src++ );
				if ( ! *src ) {
					inq = 0;
					break;
				}
				// no break
			default:
				string_append( string, *src );
			}
			if ( inq ) continue;
		}
		switch ( *src ) {
		case '{':
			backlog = expand_b( &backlog, string );
			sub_results = string_expand( src + 1, &src );
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
			string_append( string, *src );
			inq = 1;
			break;
		case '\\':
			switch ( src[ 1 ] ) {
			case 0:
				string_append( string, '\\' );
				break;
			default:
				string_append( string, *src++ );
				string_append( string, *src );
			}
			break;
		default:
			string_append( string, *src );
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
expand_b( listItem **base, StringVA *string )
{
	listItem *results = NULL;

	string_finish( string, 0 );
	if (( string->value )) {
		listItem *sub = newItem( string->value );
		results = (*base) ? expand_s( base, &sub ) : sub;
		string->value = NULL;
	}
	else results = *base;
	return results;
}

/*---------------------------------------------------------------------------
	freeLines
---------------------------------------------------------------------------*/
void
freeLines( listItem **lines, int nostr )
{
	// free the entire lines structure
	for ( listItem *line = *lines; line!=NULL; line=line->next )
	{
		listItem **segments = (listItem **) &line->ptr;
		for ( listItem *segment = *segments; segment!=NULL; segment=segment->next )
		{
			if ( nostr ) {
				freeListItem( (listItem **) &segment->ptr );
				continue;
			}
			listItem **options = (listItem **) &segment->ptr;
			for ( listItem *option = *options; option!=NULL; option=option->next )
			{
				StringVA *string = option->ptr;
				freeString( string );
			}
			freeListItem( options );
		}
		freeListItem( segments );
	}
	freeListItem( lines );
}

/*---------------------------------------------------------------------------
	add_ and pop_ line utilities
---------------------------------------------------------------------------*/
listItem *
addLine( listItem **lines )
	{ return addItem( lines, NULL ); }

listItem *
addSegment( listItem *lines )
	{ return addItem( (listItem **) &lines->ptr, NULL ); }

listItem *
addOption( listItem *lines, int nostr )
{
	listItem *segments = lines->ptr;
	return addItem( (listItem **) &segments->ptr, nostr ? NULL : newString() );
}

void *
popOption( listItem *lines )
{
	listItem *segments = lines->ptr;
	return popListItem( (listItem **) &segments->ptr );
}

void *
popSegment( listItem *lines )
	{ return popListItem( (listItem **) &lines->ptr ); }

void *
popLine( listItem **lines )
	{ return popListItem( lines ); }

/*---------------------------------------------------------------------------
	outputLines	- output all lines
---------------------------------------------------------------------------*/
void
outputLines( listItem *lines )
{
	for ( listItem *line = lines; line != NULL; line = line->next )
	{
		// initialize option path to baseline - the latter being in reverse order
		listItem *s, *p, *path = NULL;
		for ( s = line->ptr; s!=NULL; s=s->next ) {
			listItem *option = s->ptr;
			addItem( &path, option );
		}

		// output all combinations of path
		int again;
		if ((path)) do {
			// output current option path
			for ( p = path; p!=NULL; p=p->next ) {
				listItem *option = p->ptr;
				StringVA *string = option->ptr;
				output( Text, string->index.name );
			}
			// update option path
			again = 0;
			reorderListItem( &path );
			for ( p=path, s=line->ptr; (p!=NULL) && !again; p=p->next, s=s->next )
			{
				listItem *option = p->ptr;
				if ( option->next != NULL ) {
					p->ptr = option->next;
					reorderListItem( &path );
					again = 1;
				}
				else p->ptr = s->ptr;
			}
		}
		while ( again );
		freeListItem( &path );
	}
}

