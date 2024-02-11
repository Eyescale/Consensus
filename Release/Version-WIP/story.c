#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "parser.h"
#include "narrative.h"
#include "story.h"
#include "errout.h"

//===========================================================================
//	cnStoryOutput
//===========================================================================
int
cnStoryOutput( FILE *stream, CNStory *story ) {
	if ( story == NULL ) return 0;
	Registry *narratives = story->narratives;
	for ( listItem *i=narratives->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		char *name = entry->name;
		if ( *name )
			fprintf( stream, ": %s\n", name );
		else	fprintf( stream, ":\n" );
		for ( listItem *j=entry->value; j!=NULL; j=j->next ) {
			CNNarrative *n = j->ptr;
			narrative_output( stream, n, 0 );
			fprintf( stream, "\n" ); } }
	return 1; }

//===========================================================================
//	readStory
//===========================================================================
static inline int indentation_verify( BMParseData *data );
static void free_CB( Registry *, Pair * );
static BMParseCB build_CB;

CNStory *
readStory( char *path, int ignite ) {
	if ( !path ) return ( ignite ? newStory() : NULL );
	FILE *stream = fopen( path, "r" );
	if ( !stream ) return errout( StoryLoad, path );
	CNIO io;
	io_init( &io, stream, path, IOStreamFile );
	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
        data.io = &io;
	bm_parse_init( &data, BM_STORY );
	data.narrative = newNarrative();
	data.occurrence = data.narrative->root;
	data.narratives = newRegistry( IndexedByNameRef );
	addItem( &data.stack.occurrences, data.occurrence );
	//-----------------------------------------------------------------
#define PARSE( event, func ) \
	data.state = func( event, BM_STORY, &data, build_CB );
	int event = 0;
	do {	event = io_read( &io, event );
		if ( !io.errnum ) {
			if ( !data.expr ) PARSE( event, bm_parse_cmd )
			if ( data.expr )  PARSE( event, bm_parse_expr ) }
		else data.errnum = io_report( &io );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	if ( !data.errnum && !build_CB( NarrativeTake, 0, &data ) ) // last take
		errout( StoryUnexpectedEOF );
	if ( data.errnum ) {
		freeNarrative( data.narrative );
		freeRegistry( data.narratives, free_CB );
		data.narratives = NULL; }
	else if ( !registryLookup( data.narratives, "" ) && ignite ) {
		CNNarrative *base = newNarrative();
		registryRegister( data.narratives, "", newItem( base ) ); }
	bm_parse_exit( &data );
	io_exit( &io );
	fclose( stream );
	return ( data.narratives ) ?
		(CNStory *) newPair( data.narratives, newArena() ) :
		NULL; }

static int
build_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	CNNarrative *narrative;
	listItem *base, *term;
	char *proto;
	switch ( op ) {
	case ProtoSet:
		narrative = data->narrative;
		proto = StringFinish( data->string, 0 );
		StringReset( data->string, CNStringMode );
#ifdef DEBUG
fprintf( stderr, "bgn narrative: %s\n", proto );
#endif
		if ( !proto ) { // main narrative definition start
			if (( registryLookup( data->narratives, "" ) )) {
				data->errnum = ErrNarrativeDoubleDef;
				return 0; } } // main already registered
		else if ( is_separator( *proto ) ) {
			if ( !data->entry ) {
				data->errnum = ErrNarrativeNoEntry;
				return 0; }
			else narrative->proto = proto; } // double-def accepted
		else if ( !registryLookup( data->narratives, proto ) )
			narrative->proto = proto;
		else {
			data->errnum = ErrNarrativeDoubleDef;
			free(proto);
			return 0; }
		break;
	case OccurrenceAdd: ;
		if ( !indentation_verify( data ) ) return 0;
		CNOccurrence *occurrence = newOccurrence( data->type );
		CNOccurrence *parent = data->stack.occurrences->ptr;
		addItem( &parent->sub, occurrence );
		addItem( &data->stack.occurrences, occurrence );
		data->occurrence = occurrence;
		break;
	case TagTake:
		// compare current term with registered tags
		base = data->stack.tags;
		term = data->string->data;
		int locale = data->type&LOCALE;
		for ( listItem *i=base; i!=NULL; i=i->next )
		for ( listItem *j=i->ptr, *k=term; (j)&&(k); j=j->next, k=k->next ) {
			int e=cast_i(j->ptr), f=cast_i(k->ptr);
			if ( is_separator(e) || is_separator(f) ) {
				if (( locale && i==base ) ?
					(( e=='^' || e=='%' )&&( f=='^' && f=='%' )) :
					(( e=='^' && f=='%' )||( e=='%' && f=='^' )) )
					return 0;
				break; } }
		listItem **tags = &data->stack.tags;
		if ( !locale ) addItem( tags, term );
		else { popListItem(tags); addItem( tags, term ); addItem(tags,base); }
		return 1;
	case ExpressionTake:
		occurrence = data->stack.occurrences->ptr;
		occurrence->data->type = cast_ptr( data->type );
		occurrence->data->expression = StringFinish( data->string, 0 );
		StringReset( data->string, CNStringMode );
		freeListItem( &data->stack.tags );
		break;
	case NarrativeTake: ; // Assumption: mode==( last take ? 0 : BM_STORY )
		narrative = data->narrative;
		proto = narrative->proto;
#ifdef DEBUG
fprintf( stderr, "end narrative: %s\n", proto );
#endif
		Pair *entry = data->entry;
		if ( !narrative->root->sub ) {
			if ( !mode || entry ) {
				data->errnum = ErrNarrativeEmpty;
				return 0; }
			else break; } // first take
		// register narrative
		narrative_reorder( narrative );
		if ( !proto ) {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->narratives, "", newItem(narrative) ); }	
		else if ( is_separator( *proto ) ) {
			addItem((listItem **) &entry->value, narrative );
			if ( !mode ) // last take
				reorderListItem((listItem **) &entry->value ); }
		else {
			if (( entry )) reorderListItem((listItem **) &entry->value );
			data->entry = registryRegister( data->narratives, proto, newItem(narrative) );
			narrative->proto = NULL; } // first in class
		// allocate new narrative
		if ( mode ) {
			data->narrative = newNarrative();
			freeListItem( &data->stack.occurrences );
			addItem( &data->stack.occurrences, data->narrative->root ); } }
	return 1; }

static inline int
indentation_verify( BMParseData *data ) {
	int *tab = data->tab;
	if ( TAB_LAST == -1 ) {
		// very first occurrence
		if ( data->type & ELSE )
			return 0; }
	else if ( TAB_CURRENT == TAB_LAST + 1 ) {
		if ( data->type & ELSE )
			return 0;
		CNOccurrence *parent = data->stack.occurrences->ptr;
		int type = cast_i( parent->data->type );
		switch ( type & ~(ELSE|PER) ) {
		case ROOT: case IN: case ON: case ON_X:
			break;
		default:
			return 0; } }
	else if ( TAB_CURRENT <= TAB_LAST ) {
		CNOccurrence *sibling;
		if ( TAB_CURRENT==TAB_LAST ) {
			sibling = data->stack.occurrences->ptr;
			int type = cast_i( sibling->data->type );
			if ((type&(IN|ON|ON_X)) && !(data->type&ELSE))
				return 0; }
		for ( ; ; ) {
			sibling = popListItem( &data->stack.occurrences );
			int type = cast_i( sibling->data->type );
			if ( TAB_CURRENT==TAB_LAST ) {
				if ((data->type&ELSE) && !(type&(IN|ON|ON_X)))
					return 0; }
			TAB_LAST--;
			if ( type == ROOT )
				return 0;
			else if ( TAB_CURRENT <= TAB_LAST )
				continue;
			else if ( !(data->type&ELSE) || type&(IN|ON|ON_X) )
				break; } }
	else return 0;
	return 1; }

static void
free_CB( Registry *registry, Pair *entry ) {
	char *def = entry->name;
	if ( strcmp( def, "" ) ) free( def );
	for ( listItem *i=entry->value; i!=NULL; i=i->next ) {
		CNNarrative *narrative = i->ptr;
		char *proto = narrative->proto;
		if (( proto ) && strcmp(proto,""))
			free( proto );
		freeOccurrence( narrative->root ); }
	freeListItem((listItem **) &entry->value ); }

//===========================================================================
//	newStory, freeStory
//===========================================================================
CNStory *
newStory( void ) {
	Registry *narratives = newRegistry( IndexedByNameRef );
	CNNarrative *base = newNarrative();
	registryRegister( narratives, "", newItem( base ) );
	return (CNStory *) newPair( narratives, newArena() ); }

void
freeStory( CNStory *story ) {
	if (( story )) {
		freeRegistry( story->narratives, free_CB );
		freeArena( story->arena );
		freePair((Pair *) story ); } }

