#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "parser.h"
#include "narrative.h"
#include "story.h"
#include "errout.h"

//===========================================================================
//	newStory, freeStory
//===========================================================================
CNStory *
newStory( void ) {
	Registry *narratives = newRegistry( IndexedByNameRef );
	CNNarrative *base = newNarrative();
	registryRegister( narratives, "", newItem( base ) );
	return (CNStory *) newPair( narratives, newArena() ); }

static freeRegistryCB free_CB;
void
freeStory( CNStory *story ) {
	if (( story )) {
		freeRegistry( story->narratives, free_CB );
		freeArena( story->arena );
		freePair((Pair *) story ); } }
static void
free_CB( Registry *registry, Pair *entry ) {
	char *def = entry->name;
	if ( strcmp( def, "" ) ) free( def );
	listItem **narratives = (listItem **) &entry->value;
	CNNarrative *narrative;
	while (( narrative=popListItem(narratives) ))
		freeNarrative( narrative ); }

//===========================================================================
//	readStory
//===========================================================================
static inline int indentation_check( BMParseData *data );
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
	if ( !data.errnum && !build_CB( NarrativeTake, 0, &data ) ) { // last take
		data.errnum = ErrUnexpectedEOF;
		errout( StoryUnexpectedEOF ); }
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

#define BAR_DANGLING( o ) \
	if ((o) && cast_i(o->data->type)&(IN|ON|ON_X)) return 0;
static int
build_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	CNNarrative *narrative;
	listItem *base, *term;
	char *proto;
	switch ( op ) {
	case ProtoSet:
		BAR_DANGLING( data->occurrence )
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
	case OccurrenceTake: ;
		if ( !indentation_check( data ) ) return 0;
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
	case NarrativeTake: // Assumption: mode==( last take ? 0 : BM_STORY )
		BAR_DANGLING( data->occurrence )
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

static inline int l_case( CNOccurrence *sibling, BMParseData *data );
static inline int
indentation_check( BMParseData *data )
/*
	if data->type is informed, then
		verify data->type against parent->type
	otherwise if parent->type & SWITCH, then
		inform data->type as [ELSE]|CASE|{IN,ON}
		Note that ELSE is set if parent's last sub
		is either ELSE|... or has sub
		return 1
	otherwise return 0
*/ {
	int *tab = data->tab;
	int type = data->type;
	int elsing = type & ELSE;
	if ( TAB_LAST==-1 ) // very first occurrence
		return type ? !elsing : 0;
	CNOccurrence *parent, *sibling;
	int parent_type, sibling_type;
	int tab_diff = TAB_LAST - TAB_CURRENT;
	if ( type ) {
		if ( tab_diff < 0 ) {
			if ( elsing || tab_diff < -1 ) return 0;
			parent = data->stack.occurrences->ptr;
			parent_type = cast_i( parent->data->type );
			return ( parent_type==ROOT || parent_type==ELSE ||
			         parent_type&(PER|IN|ON|ON_X) ); }
		else if ( !tab_diff ) {
			/* We do support
				in expression
				else ...
			   but we do not support e.g.
				in expression
				do expression	// no indentation
			   nor
				else ...	// out of the blue
			*/
			sibling = popListItem( &data->stack.occurrences );
			sibling_type = cast_i( sibling->data->type );
			sibling_type &= (IN|ON|ON_X);
			if (( sibling_type && !elsing )||( elsing && !sibling_type ))
				return 0; }
		else {
			listItem **stack = &data->stack.occurrences;
			while ( tab_diff-- ) {
				popListItem( stack );
				if ( !*stack ) return 0; }
			sibling = popListItem( stack );
			sibling_type = cast_i( sibling->data->type );
			sibling_type &= (IN|ON|ON_X);
			if ( elsing && !sibling_type )
				return 0; }
		return 1; }
	else {
		if ( tab_diff < 0 ) {
			if ( tab_diff < -1 ) return 0;
			parent = data->stack.occurrences->ptr;
			parent_type = cast_i( parent->data->type );
			if ( parent_type & SWITCH ) {
				if (( parent->sub )) {
					sibling = parent->sub->ptr;
					return l_case( sibling, data ); }
				else {
					data->type = CASE|(parent_type&(IN|ON));
					return 1; } } }
		else if ( !tab_diff ) {
			sibling = data->stack.occurrences->ptr;
			if ( l_case( sibling, data ) ) {
				popListItem( &data->stack.occurrences );
				return 1; } }
		else {
			listItem *stack = data->stack.occurrences;
			while ( tab_diff-- ) {
				stack = stack->next;
				if ( !stack ) return 0; }
			sibling = stack->ptr;
			if ( l_case( sibling, data ) ) {
				listItem **s = &data->stack.occurrences;
				do popListItem( s ); while ( *s!=stack );
				popListItem( s );
				return 1; } }
		return 0; } }

static inline int
l_case( CNOccurrence *sibling, BMParseData *data ) {
	int sibling_type = cast_i( sibling->data->type );
	if ( sibling_type&CASE ) {
		data->type = CASE|(sibling_type&(IN|ON));
		if ( sibling_type&ELSE || ( sibling->sub ))
			data->type |= ELSE;
		return 1; }
	else return 0; }

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

