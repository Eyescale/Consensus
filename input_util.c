#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "output.h"

// #define DEBUG

struct {
	int real, fake;
}
separator_table[] = {
	{ '\t', 't' },
	{ '\n', 'n' },
#if 1
	{ '\\', '\\' },
#endif
	{ '\0', 0 },
	{ -1, 0 },
	{ ' ', 0 },
	{ '-', 0 },
	{ '|', 0 },
	{ '>', 0 },
	{ '<', 0 },
	{ '[', 0 },
	{ ']', 0 },
	{ '%', 0 },
	{ '.', 0 },
	{ ',', 0 },
	{ ';', 0 },
	{ ':', 0 },
	{ '(', 0 },
	{ ')', 0 },
	{ '{', 0 },
	{ '}', 0 },
	{ '[', 0 },
	{ ']', 0 },
	{ '<', 0 },
	{ '>', 0 },
	{ '-', 0 },
	{ '=', 0 },
	{ '/', 0 },
	{ '%', 0 },
	{ '?', 0 },
	{ '!', 0 }
};

/*---------------------------------------------------------------------------
	is_separator
---------------------------------------------------------------------------*/
int
num_separators( void )
{
	return sizeof( separator_table ) / sizeof( separator_table[ 0 ] );
}
int
is_separator( int event )
{
	for ( int i =0; i< num_separators(); i++ ) {
		if ( event == separator_table[ i ].real ) {
			return event;
		}
	}
	return !event;
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
	freeListItem( &string->list );
	free( string->ptr );
	string->ptr = NULL;
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
	data.event = event;
	addItem( &string->list, data.ptr );
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
	string->ptr = (char *) malloc( count + 1 );
	for ( i = *list, count=0; i!=NULL; i=i->next, count++ ) {
		string->ptr[ count ] = (char) i->ptr;
	}
	string->ptr[ count ] = 0;

	freeListItem( list );
	return string->ptr;
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
expand_b( listItem **base, IdentifierVA *buffer )
{
	listItem *results = NULL;

	string_finish( buffer, 0 );
	if (( buffer->ptr )) {
		listItem *sub = newItem( buffer->ptr );
		results = (*base) ? expand_s( base, &sub ) : sub;
		buffer->ptr = NULL;
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
	IdentifierVA buffer = { NULL, NULL };
	for ( char *src = identifier; ; src++ ) {
		switch ( *src ) {
		case 0:
		case '}':
		case ',':
			backlog = expand_b( &backlog, &buffer );
			results = catListItem( results, backlog );
			switch ( *src ) {
			case 0:
			case '}':
				if (( next_pos )) *next_pos = src;
				return results;
			}
			backlog = NULL;
			break;
		case '\\':
			switch ( src[ 1 ] ) {
			case 0:
				string_append( &buffer, '\\' );
				break;
			case '"':
			case 'n':
			case 't':
				string_append( &buffer, '\\' );
				// no break
			default:
				src++;
				string_append( &buffer, *src );
			}
			break;
		case ' ':
			break;
		case '{':
			backlog = expand_b( &backlog, &buffer );
			sub_results = string_expand( src + 1, &src );
			backlog = expand_s( &backlog, &sub_results );
			if ( *src == (char) 0 ) {
				if (( next_pos )) *next_pos = src;
				return catListItem( results, backlog );
			}
			break;
		default:
			string_append( &buffer, *src );
		}
	}
}

/*---------------------------------------------------------------------------
	stringify
---------------------------------------------------------------------------*/
void
slist_close( listItem **slist, IdentifierVA *dst, int cleanup )
{
	if (( dst->list )) {
		char *str = string_finish( dst, 0 );
		if ( strcmp( str, "\n" ) ) {
			addItem( slist, str );
		}
		else free( str );
		dst->ptr = NULL;
	}
}
void
slist_append( listItem **slist, IdentifierVA *dst, int event, int as_string, int cleanup )
{
	if ( dst->list == NULL ) {
		string_start( dst, event );
	} else if (( event != '\n' ) || !as_string ) {
		string_append( dst, event );
	}
	if ( event == '\n' ) slist_close( slist, dst, cleanup );
}
listItem *
stringify( listItem *src, int cleanup )
{
	listItem *results = NULL;
	IdentifierVA dst = { NULL, NULL };
	for ( listItem *i = src; i!=NULL; i=i->next )
		for ( char *ptr = i->ptr; *ptr; ptr++ )
			slist_append( &results, &dst, *ptr, 1, cleanup );
	slist_close( &results, &dst, cleanup );
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
	IdentifierVA result = { NULL, NULL };
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

/*---------------------------------------------------------------------------
	read_0, read_1, read_2
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = read_execute( a, &state, event, s, context );

static int
read_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) )
		*state = next_state;
	return event;
}

static int
identifier_start( char *state, int event, char **next_state, _context *context )
{
	return string_start( &context->identifier.id[ context->identifier.current ], event );
}

static int
identifier_append( char *state, int event, char **next_state, _context *context )
{
	return string_append( &context->identifier.id[ context->identifier.current ], event );
}

static int
identifier_finish( char *state, int event, char **next_state, _context *context )
{
	string_finish( &context->identifier.id[ context->identifier.current ], 0 );
	return event;
}

int
on_token( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) ) {
		do_( flush_input, same );
		*next_state = *next_state + strlen( *next_state ) + 1;
		return event;
	}
	char *ptr = *next_state;
	for ( ; *ptr; event=0, ptr++ ) {
		event = read_input( state, event, NULL, context );
		if ( event != *ptr ) return -1;
	}
	*next_state = ++ptr;
	return 0;
}

int
read_0( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) ) {
		do_( flush_input, same );
		return event;
	}

	state = base;
	do {
		event = read_input( state, event, NULL, context );
		bgn_
		in_( base ) bgn_
			on_( '\\' )	do_( nop, "\\" )
			on_( '\"' )	do_( nop, "\"" )
			on_separator	do_( error, "" )
			on_other	do_( identifier_start, "identifier" )
			end
			in_( "\\" ) bgn_
				on_any	do_( identifier_start, "identifier" )
				end
			in_( "\"" ) bgn_
				on_( '\\' )	do_( nop, "\"\\" )
				on_other	do_( identifier_start, "\"identifier" )
				end
				in_( "\"\\" ) bgn_
					on_( 't' )	event = '\t';
							do_( identifier_start, "\"identifier" )
					on_( 'n' )	event = '\n';
							do_( identifier_start, "\"identifier" )
					on_other	do_( identifier_start, "\"identifier" )
					end
				in_( "\"identifier" ) bgn_
					on_( '\\' )	do_( nop, "\"identifier\\" )
					on_( '\"' )	do_( nop, "\"identifier\"" )
					on_other	do_( identifier_append, same )
					end
					in_( "\"identifier\\" ) bgn_
						on_( 't' )	event = '\t';
								do_( identifier_append, "\"identifier" )
						on_( 'n' )	event = '\n';
								do_( identifier_append, "\"identifier" )
						on_other	do_( identifier_append, "\"identifier" )
						end
					in_( "\"identifier\"" ) bgn_
						on_any do_( identifier_finish, "" )
						end
		in_( "identifier" ) bgn_
			on_( '\\' )	do_( nop, "identifier\\" )
			on_( '\"' )	do_( identifier_finish, "" )
			on_separator	do_( identifier_finish, "" )
			on_other	do_( identifier_append, same )
			end
			in_( "identifier\\" ) bgn_
				on_( 't' )	event = '\t';
						do_( identifier_append, "identifier" )
				on_( 'n' )	event = '\n';
						do_( identifier_append, "identifier" )
				on_other	do_( identifier_append, "identifier" )
				end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

int
read_1( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 1;
	event = read_0( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

int
read_2( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 2;
	event = read_0( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

