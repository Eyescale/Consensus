#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "database.h"
#include "expression.h"
#include "parser.h"
#include "story.h"

//===========================================================================
//	readStory / bm_read / occurrence_set
//===========================================================================
static void occurrence_set( CNOccurrence *, CNString *, int type );

CNStory *
readStory( char *path )
{
	CNStory *story = bm_read( CN_STORY, path );
	return story;
}
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
	char *def = entry->name;
	if ( strcmp( def, "" ) ) free( def );
	for ( listItem *i=entry->value; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		if (( narrative->proto ))
			free( narrative->proto );
		freeOccurrence( narrative->root );
	}
	freeListItem((listItem **) &entry->value );
}

//===========================================================================
//	proto_set / story_add
//===========================================================================
static void narrative_reorder( CNNarrative * );

int
proto_set( BMStoryData *data )
{
	CNString *s = data->string;
	char *p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	if ( p == NULL ) {
		if (( registryLookup( data->story, "" ) ))
			return 0; // main already exists
	}
	else if ( is_separator( *p ) ) {
		if ( !registryLookup( data->story, "" ) )
			return 0; // no main
		data->narrative->proto = p;
	}
	else {
		if (( registryLookup( data->story, p ) ))
			return 0; // double-def
		data->narrative->proto = p;
	}
	return 1;
}
int
story_add( BMStoryData *data, int finish )
{
	if ( data->story == NULL ) return 0;
	CNNarrative *narrative = data->narrative;
	CNOccurrence *root = narrative->root;
	if ( !root->sub ) return 0; // narrative empty
	narrative_reorder( narrative );
	char *p = narrative->proto;
	Pair *entry = data->entry;
	if ( p == NULL ) {
		if (( entry )) reorderListItem((listItem **) &entry->value );
		data->entry = registryRegister( data->story, "", newItem(narrative) );
	}	
	else if ( is_separator( *p ) ) {
		addItem((listItem **) &entry->value, narrative );
		if ( finish ) reorderListItem((listItem **) &entry->value );
	}
	else {
		if (( entry )) reorderListItem((listItem **) &entry->value );
		data->entry = registryRegister( data->story, p, newItem(narrative) );
		narrative->proto = NULL;
	}
	return 1;
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
		if (( proto )) free( proto );
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative );
	}
}

//===========================================================================
//	narrative_reorder
//===========================================================================
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
//	story_output / narrative_output
//===========================================================================
static int narrative_output( FILE *, CNNarrative *, int );

int
story_output( FILE *stream, CNStory *story )
{
	if ( story == NULL ) return 0;
	for ( listItem *i=story->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		fprintf( stream, ": %s\n", (char *) entry->name );
		for ( listItem *j=entry->value; j!=NULL; j=j->next ) {
			CNNarrative *n = j->ptr;
			narrative_output( stream, n, 0 );
			fprintf( stream, "\n" );
		}
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
	char *proto = narrative->proto;
	if (( proto )) {
		if ( !is_separator( *proto ) )
			fprintf( stream, ": " );
		fprintf( stream, "%s\n", proto );
	}
	CNOccurrence *occurrence = narrative->root;

	listItem *i = newItem( occurrence ), *stack = NULL;
#define TAB( level ) \
	for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = occurrence->data->type;
		char *expression = occurrence->data->expression;

		TAB( level );
		if ( type==ROOT ) ;
		else if ( type==ELSE )
			fprintf( stream, "else\n" );
		else {
			if ( type&ELSE )
				fprintf( stream, "else " );
			if ( type&IN )
				fprintf( stream, "in " );
			else if ( type&ON )
				fprintf( stream, "on " );
			else if ( type&(DO|INPUT|OUTPUT) )
				fprintf( stream, "do " );
			fprintf( stream, "%s\n", expression );
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

