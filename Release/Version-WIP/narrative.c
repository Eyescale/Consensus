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
//	bm_read
//===========================================================================
static int read_CB( BMParseOp, BMParseMode, void * );
static CNNarrative * newNarrative( void );
static void freeNarrative( CNNarrative * );

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
	FILE *stream;
	int type;
	BMContext *ctx;
	va_list ap;
	va_start( ap, mode );
	switch ( mode ) {
	case BM_INPUT:
		stream = va_arg( ap, FILE * );
		type = IOStreamStdin;
		break;
	case BM_LOAD:
		ctx = va_arg( ap, BMContext * );
		// no break;
	case BM_STORY:
		path = va_arg( ap, char * );
		stream = fopen( path, "r" );
		type = IOStreamFile;
		if ( !stream ) {
			fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
			va_end( ap );
			return NULL; }
		break; }
	va_end( ap );

	CNIO io;
	io_init( &io, stream, type );

	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
        data.io = &io;
	bm_parse_init( &data, mode );

	if ( mode==BM_LOAD ) {
		data.ctx = ctx; }
	else if ( mode==BM_STORY ) {
		data.narrative = newNarrative();
		data.occurrence = data.narrative->root;
		data.story = newRegistry( IndexedByCharacter );
		addItem( &data.stack.occurrences, data.occurrence ); }
	//-----------------------------------------------------------------

	int event = 0;
	do {
		event = io_getc( &io, event );
		data.state = bm_parse( event, mode, &data, read_CB );
	} while ( strcmp( data.state, "" ) && !data.errnum );

	//-----------------------------------------------------------------
	union { int value; void *ptr; } retval;
	switch ( mode ) {
	case BM_LOAD:
		fclose( stream );
		retval.value = data.errnum;
		break;
	case BM_INPUT:
		if ( data.errnum )
			retval.ptr = NULL;
		else {
			retval.ptr = StringFinish( data.string, 0 );
			StringReset( data.string, CNStringMode ); }
		break;
	case BM_STORY:
		fclose( stream );
		freeListItem( &data.stack.occurrences );
		if ( data.errnum )
			retval.ptr = NULL;
		else if ( read_CB( NarrativeTake, 0, &data ) )
			retval.ptr = data.story;
		else {
			retval.ptr = NULL;
			fprintf( stderr, "Error: read_narrative: unexpected EOF\n" ); }
		if ( !retval.ptr ) {
			freeNarrative( data.narrative );
			freeStory( data.story ); } }

	bm_parse_exit( &data );
	io_exit( &io );
	return retval.ptr;
}

//---------------------------------------------------------------------------
//	read_CB	- invoked by bm_parse()
//---------------------------------------------------------------------------
static CNOccurrence * newOccurrence( int );
static void narrative_reorder( CNNarrative * );

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
			switch ( parent->data->type & ~(ELSE|PER) ) {
			case ROOT:
			case IN:
			case ON:
			case ON_X:
				break;
			default:
				return 0; } }
		else if ( TAB_CURRENT <= TAB_LAST ) {
			CNOccurrence *sibling;
			if ( TAB_CURRENT==TAB_LAST ) {
				sibling = data->stack.occurrences->ptr;
				if ((sibling->data->type&(IN|ON|ON_X)) && !(data->type&ELSE))
					return 0; }
			for ( ; ; ) {
				sibling = popListItem( &data->stack.occurrences );
				if ( TAB_CURRENT==TAB_LAST ) {
					if ((data->type&ELSE) && !(sibling->data->type&(IN|ON|ON_X)))
						return 0; }
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

//---------------------------------------------------------------------------
//	narrative_reorder
//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
//	newNarrative / freeNarrative
//---------------------------------------------------------------------------
static CNNarrative *
newNarrative( void )
{
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) );
}

static void freeOccurrence( CNOccurrence * );
static void
freeNarrative( CNNarrative *narrative )
{
	if (( narrative )) {
		char *proto = narrative->proto;
		if (( proto )) free( proto );
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative ); }
}

//---------------------------------------------------------------------------
//	newOccurrence / freeOccurrence
//---------------------------------------------------------------------------
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
			if ( type&PER )
				fprintf( stream, "per " );
			else if ( type&IN )
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

