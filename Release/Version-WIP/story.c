#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "expression.h"
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
//	freeStory
//===========================================================================
static void free_CB( Registry *, Pair * );

void
freeStory( CNStory *story )
{
	if ( story == NULL ) return;
	freeRegistry( story, free_CB );
}

static void freeOccurrence( CNOccurrence * );
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
//	cnStoryOutput
//===========================================================================
static int narrative_output( FILE *, CNNarrative *, int );

int
cnStoryOutput( FILE *stream, CNStory *story )
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

//===========================================================================
//	bm_read	- internal
//===========================================================================
static int read_CB( BMParseOp, BMParseMode, void * );
static int bm_read_init( BMStoryData *, BMReadMode, CNDB * );
static void * bm_read_exit( int, BMStoryData *, BMReadMode );

void *
bm_read( BMReadMode mode, ... )
/*
   Usage examples:
	1. CNStory *story = bm_read( CN_STORY, (char*) path );
	2. CNInstance *instance = bm_read( CN_INSTANCE, (FILE*) stream );
	3. union { int value; void *ptr; } icast;
	   icast.ptr = bm_read( CN_INI, (CNDB*) db, (char*) inipath );
	   int errnum = icast.value;
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
	int parse_mode = bm_read_init( &data, mode, db );

	CNParserData parser;
	cn_parser_init( &parser, "base", file, &data );
	bm_parser_init( &parser, parse_mode );
	do {
		int event = cn_parser_getc( &parser );
		if ( event!=EOF && event!='\n' ) {
			parser.column++;
		}
		parser.state = bm_parse( event, &parser, parse_mode, read_CB );
		if ( !strcmp( parser.state, "" ) || parser.errnum ) {
			break;
		}
		if ( event=='\n' ) {
			parser.column = 0;
			parser.line++;
		}
	} while ( strcmp( parser.state, "" ) );
	if (!( mode == CN_INSTANCE ))
		fclose( file );
	return bm_read_exit( parser.errnum, &data, mode );
}

static void narrative_reorder( CNNarrative * );
static CNNarrative * newNarrative( void );
static CNOccurrence * newOccurrence( int );
static int
read_CB( BMParseOp op, BMParseMode mode, void *user_data )
{
	BMStoryData *data = user_data;
	switch ( op ) {
	case NarrativeTake: ;
		CNNarrative *narrative = data->narrative;
		if ( !narrative->root->sub ) return 0; // narrative empty
		narrative_reorder( narrative );
		char *proto = narrative->proto;
		Pair *entry = data->entry;
		if ( !proto ) {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->story, "", newItem(narrative) );
		}	
		else if ( is_separator( *proto ) ) {
			addItem((listItem **) &entry->value, narrative );
			if ( !mode ) reorderListItem((listItem **) &entry->value );
		}
		else {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->story, proto, newItem(narrative) );
			narrative->proto = NULL;
		}
		if ( mode ) {
			data->narrative = newNarrative();
			freeListItem( &data->stack );
			addItem( &data->stack, data->narrative->root );
		}
		break;
	case ProtoSet:
		proto = StringFinish( data->string, 0 );
		StringReset( data->string, CNStringMode );
		if ( !proto ) {
			if (( registryLookup( data->story, "" ) ))
				return 0; // main already exists
		}
		else if ( is_separator( *proto ) ) {
			if ( !registryLookup( data->story, "" ) )
				return 0; // no main
			data->narrative->proto = proto;
		}
		else {
			if (( registryLookup( data->story, proto ) ))
				return 0; // double-def
			data->narrative->proto = proto;
		}
		break;
	case OccurrenceAdd: ;
		int *tab = data->tab;
		if ( TAB_LAST == -1 ) {
			// very first occurrence
			if ( data->type & ELSE )
				return 0;
		}
		else if ( TAB_CURRENT == TAB_LAST + 1 ) {
			if ( data->type & ELSE )
				return 0;
			CNOccurrence *parent = data->stack->ptr;
			switch ( parent->data->type ) {
			case ROOT: case ELSE:
			case IN: case ELSE_IN:
			case ON: case ELSE_ON:
				break;
			default:
				return 0;
			}
		}
		else if ( TAB_CURRENT <= TAB_LAST ) {
			for ( ; ; ) {
				CNOccurrence *sibling = popListItem( &data->stack );
				TAB_LAST--;
				if ( sibling->data->type == ROOT )
					return 0;
				else if ( TAB_CURRENT <= TAB_LAST )
					continue;
				else if ( !(data->type&ELSE) || sibling->data->type&(IN|ON) )
					break;
			}
		}
		else return 0;
		CNOccurrence *occurrence = newOccurrence( data->type );
		CNOccurrence *parent = data->stack->ptr;
		addItem( &parent->sub, occurrence );
		addItem( &data->stack, occurrence );
		data->occurrence = occurrence;
		break;
	case ExpressionPush:
		StringAppend( data->string, '(' );
		for ( char *p = data->narrative->proto,
			*q = (((p) && *p=='.') ? p+1 : "this:");
			*q!=':'; q++ ) { StringAppend( data->string, *q ); }
		StringAppend( data->string, ',' );
		break;
	case ExpressionPop:
		StringAppend( data->string, ')' );
		break;
	case ExpressionTake:
		switch ( mode ) {
		case BM_INSTANCE:
			break;
		case BM_STORY: ;
			occurrence = data->stack->ptr;
			occurrence->data->type = data->type;
			occurrence->data->expression = StringFinish( data->string, 0 );
			StringReset( data->string, CNStringMode );
			break;
		case BM_INI: ;
			char *expression = StringFinish( data->string, 0 );
			bm_substantiate( expression, data->db );
			StringReset( data->string, CNStringAll );
		}
		break;
	}
	return 1;
}

static int
bm_read_init( BMStoryData *data, BMReadMode mode, CNDB *db )
{
	int parse_mode = 0;
	memset( data, 0, sizeof(BMStoryData) );
	data->string = newString();
	switch ( mode ) {
	case CN_STORY:
		parse_mode = BM_STORY;
		data->narrative = newNarrative();
		data->occurrence = data->narrative->root;
		data->story = newRegistry( IndexedByName );
		addItem( &data->stack, data->occurrence );
		break;
	case CN_INI:
		parse_mode = BM_INI;
		data->db = db;
		data->type = DO;
		break;
	case CN_INSTANCE:
		parse_mode = BM_INSTANCE;
		data->type = DO;
		break;
	}
	return parse_mode;
}

static void freeNarrative( CNNarrative * );
static void *
bm_read_exit( int errnum, BMStoryData *data, BMReadMode mode )
{
	switch ( mode ) {
	case CN_STORY:
		freeString( data->string );
		freeListItem( &data->stack );
		CNStory *story = data->story;
		if ( errnum ) {
			freeStory( data->story );
			story = NULL;
		}
		else if ( read_CB( NarrativeTake, 0, data ) )
			return story;
		if (( story )) {
			fprintf( stderr, "Error: read_narrative: unexpected EOF\n" );
			freeStory( data->story );
		}
		freeNarrative( data->narrative );
		return NULL;
	case CN_INI:
		freeString( data->string );
		union { int value; void *ptr; } icast;
		icast.value = errnum;
		return icast.ptr;
	case CN_INSTANCE: ;
		CNString *s = data->string;
		char *expression = StringFinish( s, 0 );
		StringReset( s, CNStringMode );
		freeString( s );
		return expression;
	}
}

//===========================================================================
//	newNarrative / freeNarrative
//===========================================================================
static CNNarrative *
newNarrative( void )
{
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) );
}

static void
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
static CNOccurrence *
newOccurrence( int type )
{
	union { int value; void *ptr; } icast;
	icast.value = type;
	Pair *data = newPair( icast.ptr, NULL );
	return (CNOccurrence *) newPair( data, NULL );
}

static void
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

