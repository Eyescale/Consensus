#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "database.h"
#include "expression.h"
#include "story.h"
#include "parser.h"

//===========================================================================
//	readStory
//===========================================================================
CNStory *
readStory( char *path )
{
	CNStory *story = bm_read( CN_STORY, path );
	return story;
}

//===========================================================================
//	bm_read
//===========================================================================
static void occurrence_set( CNOccurrence *, CNString *, int type );

void *
bm_read( BMReadMode mode, ... )
/*
   Usage:
	bm_read( CN_INSTANCE, (FILE*) stream );
	bm_read( CN_INI, (CNDB*) db, (char*) inipath );
	bm_read( CN_STORY, (char*) path );
*/
{
	char *path;
	FILE *file;
	CNDB *db;
	va_list ap;
	va_start( ap, mode );
	switch ( mode ) {
	case CN_INSTANCE:
		file = va_arg( ap, FILE * );
		break;
	case CN_INI:
		db = va_arg( ap, CNDB * );
		// no break;
	case CN_STORY:
		path = va_arg( ap, char * );
		file = fopen( path, "r" );
		if ( file == NULL ) {
			fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
			va_end( ap );
			return NULL;
		}
		break;
	}
	va_end( ap );

	BMStoryData data;
	CNParserData parser;
	bm_parser_init( &parser, &data, file, mode );
	do {
		int event = cnParserGetc( &parser );
		if ( event!=EOF && event!='\n' ) {
			parser.column++;
		}
		parser.state = bm_parse( event, &parser, mode );
		if ( !strcmp( parser.state, "" ) || parser.errnum ) {
			break;
		}
		if ( !strcmp( parser.state, "expr_" ) ) {
			switch ( mode ) {
			case CN_INSTANCE:
				goto RETURN;
			case CN_STORY: ;
				CNOccurrence *occurrence = data.stack->ptr;
				occurrence_set( occurrence, data.string, data.type );
				break;
			case CN_INI: ;
				char *expression = StringFinish( data.string, 0 );
				bm_substantiate( expression, db );
				StringReset( data.string, CNStringAll );
			}
		}
		if ( event=='\n' ) {
			parser.column = 0;
			parser.line++;
		}
	} while ( strcmp( parser.state, "" ) );
RETURN:
	if ( parser.errnum ) {
		bm_parser_report( parser.errnum, &parser, mode );
	}
	if ( mode != CN_INSTANCE ) {
		fclose( file );
	}
	return bm_parser_exit( &parser, mode );
}

static void
occurrence_set( CNOccurrence *occurrence, CNString *s, int type )
{
	occurrence->data->type = type;
	occurrence->data->expression = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
}

//===========================================================================
//	freeStory
//===========================================================================
static void free_CB( Registry *, Pair * );

void
freeStory( CNStory *story )
{
	if ( story == NULL ) return;
	freeRegistry( story, free_CB );
}

static void
free_CB( Registry *registry, Pair *entry )
{
	char *proto = entry->name;
	if ( strcmp( proto, "" ) )
		free( proto );
	freeOccurrence( entry->value );
}

//===========================================================================
//	newNarrative / freeNarrative
//===========================================================================
CNNarrative *
newNarrative( void )
{
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) );
}
void
freeNarrative( CNNarrative *narrative )
{
	if (( narrative )) {
		char *proto = narrative->proto;
		if ( (proto) && strcmp( proto, "" ) )
			free( proto );
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative );
	}
}

//===========================================================================
//	newOccurrence / freeOccurrence
//===========================================================================
CNOccurrence *
newOccurrence( int type )
{
	union { int value; void *ptr; } icast;
	icast.value = type;
	Pair *data = newPair( icast.ptr, NULL );
	return (CNOccurrence *) newPair( data, NULL );
}
void
freeOccurrence( CNOccurrence *occurrence )
{
	if ( occurrence == NULL ) return;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i = j; }
		else {
			for ( ; ; ) {
				char *expression = occurrence->data->expression;
				if (( expression )) free( expression );
				freePair((Pair *) occurrence->data );
				freePair((Pair *) occurrence );
				if (( i->next )) {
					i = i->next;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					freeListItem( &occurrence->sub );
				}
				else {
					freeItem( i );
					return;
				}
			}
		}
	}
}

//===========================================================================
//	story_add
//===========================================================================
static void narrative_reorder( CNNarrative * );

int
story_add( CNStory *story, CNNarrative *narrative )
/*
	Assumption: narrative proto is new to story (cf. proto_set below)
*/
{
	if ( story == NULL ) return 0;
	CNOccurrence *root = narrative->root;
	if ( !root->sub ) return 0; // narrative empty

	narrative_reorder( narrative );
	char *proto = narrative->proto;
	if (( proto ))
		registryRegister( story, proto, root );
	else {
		registryRegister( story, "", root );
	}
	freePair((Pair *) narrative );
	return 1;
}
static void
narrative_reorder( CNNarrative *narrative )
{
	if ( narrative == NULL ) return;
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i = j; }
		else {
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					reorderListItem( &occurrence->sub );
				}
				else {
					freeItem( i );
					return;
				}
			}
		}
	}
}

//===========================================================================
//	story_output
//===========================================================================
static int narrative_output( FILE *, CNNarrative *, int );

int
story_output( FILE *stream, CNStory *story )
{
	if ( story == NULL ) return 0;
	for ( listItem *i=story->entries; i!=NULL; i=i->next ) {
		CNNarrative *n = i->ptr;
		narrative_output( stream, n, 0 );
		fprintf( stream, "\n" );
	}
	return 1;
}

static int
narrative_output( FILE *stream, CNNarrative *narrative, int level )
{
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0;
	}
	if ( strcmp( narrative->proto, "" )) {
		fprintf( stream, ": %s\n", narrative->proto );
	}
	else fprintf( stream, ":\n" );
	CNOccurrence *occurrence = narrative->root;

	listItem *i = newItem( occurrence ), *stack = NULL;
#define TAB( level ) \
	for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = occurrence->data->type;
		char *expression = occurrence->data->expression;

		TAB( level );
		switch ( type ) {
		case ELSE:
			fprintf( stream, "else\n" );
			break;
		case ELSE_IN:
		case ELSE_ON:
		case ELSE_DO:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "else " );
			break;
		default:
			break;
		}
		switch ( type ) {
		case ROOT:
		case ELSE:
			break;
		case IN:
		case ELSE_IN:
			fprintf( stream, "in %s\n", expression );
			break;
		case ON:
		case ELSE_ON:
			fprintf( stream, "on %s\n", expression );
			break;
		case DO:
		case ELSE_DO:
			fprintf( stream, "do " );
			listItem *base=NULL; int count;
#if 1
			for ( char *p=expression; *p; p++ )
				fprintf( stream, "%c", *p );
#else
			for ( char *p=expression; *p; p++ ) {
				switch ( *p ) {
				case '{':
					add_item( &base, level );
					level++; count=0;
					fprintf( stream, "{\n" );
					TAB( level );
					break;
				case '}':
					level = pop_item( &base );
					fprintf( stream, "}" );
					break;
				case '(':
					count++;
					fprintf( stream, "%c", *p );
					break;
				case ')':
					count--;
					fprintf( stream, "%c", *p );
					break;
				case ',':
					fprintf( stream, "%c", *p );
					if ( base && !count ) {
						fprintf( stream, "\n" );
						TAB( level );
					}
					break;
				default:
					fprintf( stream, "%c", *p );
				}
			}
#endif
			fprintf( stream, "\n" );
			break;
		case INPUT:
		case OUTPUT:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "do %s\n", expression );
			break;
		}
		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i = j; level++; }
		else {
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					occurrence = i->ptr;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					level--;
				}
				else {
					freeItem( i );
					return 0;
				}
			}
		}
	}
}

//===========================================================================
//	proto_set
//===========================================================================
int
proto_set( CNNarrative *narrative, CNStory *story, CNString *s )
{
	char *p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	if ( !p ) p = "";
	if (( storyLookup( story, p ) ))
		return 0; // narrative already registered
	narrative->proto = p;
	return 1;
}

