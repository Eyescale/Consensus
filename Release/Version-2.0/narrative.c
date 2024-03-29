#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "traverse.h"
#include "parser.h"
#include "expression.h"
#include "instantiate.h"

//===========================================================================
//	readStory
//===========================================================================
CNStory *
readStory( char *path )
{
	CNStory *story = bm_read( BM_STORY, path );
	return story;
}

//===========================================================================
//	bm_read	- internal
//===========================================================================
static int read_CB( BMParseOp, BMParseMode, void * );
static BMParseMode bm_read_init( CNParser *, BMParseData *, BMReadMode, BMContext *, FILE * );
static void * bm_read_exit( int, BMParseData *, BMReadMode );

void *
bm_read( BMReadMode mode, ... )
/*
   Usage examples:
	3. union { int value; void *ptr; } icast;
	   icast.ptr = bm_read( BM_LOAD, (BMContext*) ctx, (char*) inipath );
	   int errnum = icast.value;
	2. CNInstance *instance = bm_read( BM_INPUT, (FILE*) stream );
	1. CNStory *story = bm_read( BM_STORY, (char*) path );
*/
{
	char *path;
	FILE *file;
	BMContext *ctx;
	va_list ap;
	va_start( ap, mode );
	switch ( mode ) {
	case BM_INPUT:
		file = va_arg( ap, FILE * );
		break;
	case BM_LOAD:
		ctx = va_arg( ap, BMContext * );
		// no break;
	case BM_STORY:
		path = va_arg( ap, char * );
		file = fopen( path, "r" );
		if ( file == NULL ) {
			fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
			va_end( ap );
			return NULL; }
		break;
	}
	va_end( ap );

	CNParser parser;
	BMParseData data;
	bm_read_init( &parser, &data, mode, ctx, file );
	do {
		int event = cn_parser_getc( &parser );
		if ( event!=EOF && event!='\n' ) {
			parser.column++; }
		parser.state = bm_parse( event, &data, mode, read_CB );
		if ( !strcmp( parser.state, "" ) || parser.errnum ) {
			break; }
		if ( event=='\n' ) {
			parser.column = 0;
			parser.line++; }
	} while ( strcmp( parser.state, "" ) );
	if (!( mode == BM_INPUT ))
		fclose( file );
	return bm_read_exit( parser.errnum, &data, mode );
}

static void narrative_reorder( CNNarrative * );
static CNNarrative * newNarrative( void );
static CNOccurrence * newOccurrence( int );
static int
read_CB( BMParseOp op, BMParseMode mode, void *user_data )
{
	BMParseData *data = user_data;
	switch ( op ) {
	case NarrativeTake: ;
		CNNarrative *narrative = data->narrative;
		if ( !narrative->root->sub ) return 0; // narrative empty
		narrative_reorder( narrative );
		char *proto = narrative->proto;
		Pair *entry = data->entry;
		if ( !proto ) {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->story, "", newItem(narrative) ); }	
		else if ( is_separator( *proto ) ) {
			addItem((listItem **) &entry->value, narrative );
			if ( !mode ) reorderListItem((listItem **) &entry->value ); }
		else {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->story, proto, newItem(narrative) );
			narrative->proto = NULL; }
		if ( mode ) {
			data->narrative = newNarrative();
			freeListItem( &data->stack.occurrences );
			addItem( &data->stack.occurrences, data->narrative->root ); }
		break;
	case ProtoSet:
		proto = StringFinish( data->string, 0 );
		StringReset( data->string, CNStringMode );
		if ( !proto ) {
			if (( registryLookup( data->story, "" ) ))
				return 0; } // main already exists
		else if ( is_separator( *proto ) ) {
			if ( !registryLookup( data->story, "" ) )
				return 0; // no main
			data->narrative->proto = proto; }
		else {
			if (( registryLookup( data->story, proto ) ))
				return 0; // double-def
			data->narrative->proto = proto; }
		break;
	case OccurrenceAdd: ;
		int *tab = data->tab;
		if ( TAB_LAST == -1 ) {
			// very first occurrence
			if ( data->type & ELSE )
				return 0; }
		else if ( TAB_CURRENT == TAB_LAST + 1 ) {
			if ( data->type & ELSE )
				return 0;
			CNOccurrence *parent = data->stack.occurrences->ptr;
			switch ( parent->data->type ) {
			case ROOT: case ELSE:
			case IN: case ELSE_IN:
			case ON: case ELSE_ON:
			case ON_X: case ELSE_ON_X:
				break;
			default:
				return 0; } }
		else if ( TAB_CURRENT <= TAB_LAST ) {
			for ( ; ; ) {
				CNOccurrence *sibling = popListItem( &data->stack.occurrences );
				TAB_LAST--;
				if ( sibling->data->type == ROOT )
					return 0;
				else if ( TAB_CURRENT <= TAB_LAST )
					continue;
				else if ( !(data->type&ELSE) || sibling->data->type&(IN|ON|ON_X) )
					break; } }
		else return 0;
		CNOccurrence *occurrence = newOccurrence( data->type );
		CNOccurrence *parent = data->stack.occurrences->ptr;
		addItem( &parent->sub, occurrence );
		addItem( &data->stack.occurrences, occurrence );
		data->occurrence = occurrence;
		break;
	case ExpressionTake:
		switch ( mode ) {
		case BM_LOAD: ;
			char *expression = StringFinish( data->string, 0 );
			bm_instantiate( expression, data->ctx, NULL );
			StringReset( data->string, CNStringAll );
			break;
		case BM_INPUT:
			// will take on bm_read_exit
			break;
		case BM_STORY: ;
			occurrence = data->stack.occurrences->ptr;
			occurrence->data->type = data->type;
			occurrence->data->expression = StringFinish( data->string, 0 );
			StringReset( data->string, CNStringMode );
			break; }
		break; }
	return 1;
}

static BMParseMode
bm_read_init( CNParser *parser, BMParseData *data, BMReadMode mode, BMContext *ctx, FILE *file )
{
	memset( data, 0, sizeof(BMParseData) );
	data->string = newString();
	switch ( mode ) {
	case BM_LOAD:
		data->ctx = ctx;
		data->type = DO;
		break;
	case BM_INPUT:
		data->type = DO;
		break;
	case BM_STORY:
		data->narrative = newNarrative();
		data->occurrence = data->narrative->root;
		data->story = newRegistry( IndexedByNameRef );
		addItem( &data->stack.occurrences, data->occurrence );
		break; }
	bm_parse_init( data, parser, mode, "base", file );
	return mode;
}

static void freeNarrative( CNNarrative * );
static void *
bm_read_exit( int errnum, BMParseData *data, BMReadMode mode )
{
	switch ( mode ) {
	case BM_LOAD:
		freeString( data->string );
		union { int value; void *ptr; } icast;
		icast.value = errnum;
		return icast.ptr;
	case BM_INPUT: ;
		CNString *s = data->string;
		char *expression = StringFinish( s, 0 );
		StringReset( s, CNStringMode );
		freeString( s );
		return expression;
	case BM_STORY:
		freeString( data->string );
		freeListItem( &data->stack.occurrences );
		CNStory *take = NULL;
		if ( !errnum ) {
			if ( read_CB( NarrativeTake, 0, data ) )
				take = data->story;
			else {
				fprintf( stderr, "Error: read_narrative: unexpected EOF\n" );
			} }
		if ( !take ) {
			freeNarrative( data->narrative );
			freeStory( data->story ); }
		return take; }
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
		freeOccurrence( narrative->root ); }
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
			fprintf( stream, "\n" ); } }
	return 1;
}
static int
narrative_output( FILE *stream, CNNarrative *narrative, int level )
{
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0; }
	char *proto = narrative->proto;
	if (( proto )) {
		if ( !is_separator( *proto ) )
			fprintf( stream, ": " );
		fprintf( stream, "%s\n", proto ); }
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
			else if ( type&(ON|ON_X) )
				fprintf( stream, "on " );
			else if ( type&(DO|INPUT|OUTPUT) )
				fprintf( stream, "do " );
			fprintf( stream, "%s\n", expression ); }

		listItem *j = occurrence->sub;
		if (( j )) { addItem( &stack, i ); i = j; level++; }
		else {
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					occurrence = i->ptr;
					break; }
				else if (( stack )) {
					i = popListItem( &stack );
					level--; }
				else {
					freeItem( i );
					return 0; } } } }
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
		freePair((Pair *) narrative ); }
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
					break; }
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					reorderListItem( &occurrence->sub ); }
				else {
					freeItem( i );
					return; } } } }
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
					break; }
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					freeListItem( &occurrence->sub ); }
				else {
					freeItem( i );
					return; } } } }
}

