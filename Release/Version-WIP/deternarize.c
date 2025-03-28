#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "expression.h"
#include "errout.h"

#include "deternarize.h"
#include "deternarize_traversal.h"

//===========================================================================
//	bm_deternarize
//===========================================================================
static char * deternarize( char *, listItem **, BMTraverseData *, char * );
static void s_scan( CNString *, listItem * );
typedef int BMTernaryCB( char *, void * );
typedef struct {
	BMTernaryCB *user_CB;
	void *user_data;
	int ternary;
	struct { listItem *sequence, *flags; } stack;
	listItem *sequence;
	Pair *segment;
	} DeternarizeData;
static BMTernaryCB pass_CB;

char *
bm_deternarize( char **candidate, int type, BMContext *ctx )
/*
	build Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 

	and convert into string

   Note
	only ternary-operated sequences are pushed on stack.sequence
*/ {
	if ( !type || type&LOCALE ) return NULL;
#ifdef DEBUG
	fprintf( stderr, "bm_deternarize: bgn - candidate: %s\n", *candidate );
#endif
	char *expression = *candidate;
	if ( !expression || !strcmp( expression, ":<" ) )
		return NULL;

	char *deternarized = NULL;
	int traverse_mode = TERNARY|INFORMED;
	if ( type&DO ) traverse_mode |= LITERAL;

	DeternarizeData data;
	memset( &data, 0, sizeof(data) );
	data.user_CB = pass_CB;
	data.user_data = ctx;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = traverse_mode;

	if ( *expression!=':' ) {
		deternarize( expression, NULL, &traversal, expression );
		if (( data.sequence )) {
			// convert sequence to char *string
			CNString *s = newString();
			s_scan( s, data.sequence );
			deternarized = StringFinish( s, 0 );
			StringReset( s, CNStringMode );
			freeString( s );
			// release sequence
			free_deternarized( data.sequence ); } }
	else {
		char *p = expression + 1, *q;
		listItem *sequence[ 2 ] = { NULL, NULL };
		p = deternarize( p, &sequence[0], &traversal, expression );
		if ( *(q=p) ) {
			data.sequence = NULL;
			traversal.done = traverse_mode;
			q = deternarize( p+1, &sequence[1], &traversal, expression ); }
		if ((sequence[ 0 ])||(sequence[ 1 ])) {
			CNString *s = newString();
			if (( sequence[ 0 ] )) {
				StringAppend( s, ':' );
				s_scan( s, sequence[ 0 ] ); }
			else s_append( expression, p )
			if (( sequence[ 1 ] )) {
				StringAppend( s, *p );
				s_scan( s, sequence[ 1 ] ); }
			else s_append( p, q )
			deternarized = StringFinish( s, 0 );
			StringReset( s, CNStringMode );
			freeString( s );
			// release sequences
			free_deternarized( sequence[ 0 ] );
			free_deternarized( sequence[ 1 ] ); } }
#ifdef DEBUG
	fprintf( stderr, "bm_deternarize: end - expression: %s\n", expression );
#endif
	return ( !!deternarized ? ((*candidate=deternarized),expression) : NULL ); }

static int
pass_CB( char *guard, void *user_data ) {
	BMContext *ctx = user_data;
	return !strncmp( guard, "(~.:", 4 ) ?
		!bm_feel( BM_CONDITION, guard+4, ctx ) :
		!!bm_feel( BM_CONDITION, guard, ctx ); }

static void
s_scan( CNString *s, listItem *sequence )
/*
	scan ternary-operator guard
		Sequence:{
			| [ NULL, sub:Sequence ]
			| [ segment:Segment, NULL ]
			| [ NULL, NULL ] // ~.
			}
		where
			Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
	into CNString
*/ {
	listItem *stack = NULL;
	listItem *i = sequence;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // parse sub-sequence
			continue; }
		else if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			s_append( segment->name, segment->value ) }
		else s_add( "~." )
		// moving on
		if (( i->next ))
			i = i->next;
		else if (( stack ))
			i = popListItem( &stack );
		else break; } }

//---------------------------------------------------------------------------
//	deternarize
//---------------------------------------------------------------------------
static char *
deternarize( char *p, listItem **s, BMTraverseData *traversal, char *expression ) {
	DeternarizeData *data = traversal->user_data;
	data->segment = newPair( p, NULL );

	p = deternarize_traversal( p, traversal, FIRST );

	if ((data->stack.sequence)||(data->stack.flags)) {
		errout( DeternarizeMemoryLeak, expression, p, data->stack.sequence, data->stack.flags );
		freeListItem( &data->stack.sequence );
		freeListItem( &data->stack.flags );
		exit( -1 ); }

	if ( !data->ternary )
		freePair( data->segment );
	else {
		// finish sequence
		listItem **sequence = &data->sequence;
		Pair *segment = data->segment;
		if ( segment->name != p ) {
			segment->value = p;
			addItem( sequence, newPair( segment, NULL ) ); }
		else freePair( segment );
		reorderListItem( sequence );
		if (( s )) *s = *sequence; }
	return p; }

//---------------------------------------------------------------------------
//	deternarize_traversal
//---------------------------------------------------------------------------
static char *optimize( Pair *, char * );

BMTraverseCBSwitch( deternarize_traversal )
case_( open_CB )
	if ( p_ternary(p) ) {
		data->ternary = 1;
		Pair *segment = data->segment;
		// finish current Sequence after '(' - without reordering,
		// which will take place after closing
		segment->value = p+1;
		addItem( &data->sequence, newPair( segment, NULL ) );
		// push current Sequence on stack.sequence and start new
		addItem( &data->stack.sequence, data->sequence );
		data->sequence = NULL;
		data->segment = newPair( p+1, NULL ); }
	_break
case_( ternary_operator_CB )
	Pair *segment = data->segment;
	// finish sequence==guard, reordered
	segment->value = p;
	addItem( &data->sequence, newPair( segment, NULL ) );
	reorderListItem( &data->sequence );
	// convert & evaluate guard
	CNString *s = newString();
	s_scan( s, data->sequence );
	char *guard = StringFinish( s, 0 );
	int failed = !data->user_CB( guard, data->user_data );
	freeString( s );
	if ( failed ) {
		// release guard sequence
		free_deternarized( data->sequence );
		data->sequence = NULL;
		// proceed to alternative, i.e. ':'
		p = *q = p_prune( PRUNE_TERNARY, p );
		if ( p[1]==')' ) {
			// ~. is our current candidate
			data->segment = NULL;
			_continue( p+1 ) }
		else {
			// resume past ':'
			data->segment = newPair( p+1, NULL );
			_break } }
	else if ( p[1]==':' ) {
		// sequence==guard is our current candidate
		// sequence is already informed and completed
		data->segment = NULL;
		// proceed to ")"
		p = p_prune( PRUNE_TERNARY, p+1 );
		_continue( p ) }
	else {
		// release guard sequence
		free_deternarized( data->sequence );
		data->sequence = NULL;
		// resume past '?'
		data->segment = newPair( p+1, NULL );
		_break }
case_( filter_CB )
	if is_f( TERNARY ) {
		// option completed
		data->segment->value = p;
		// proceed to ")"
		p = p_prune( PRUNE_TERNARY, p );
		_continue( p ) }
	else _break
case_( close_CB )
	if is_f( TERNARY ) {
		char *next_p;
		Pair *segment = data->segment;
		if (( data->sequence )) {
			if (( segment )) { // sequence is not guard
				// finish current sequence, reordered
				if ( !segment->value ) segment->value = p;
				addItem( &data->sequence, newPair( segment, NULL ) );
				reorderListItem( &data->sequence ); }
			// add as sub-Sequence to on-going expression
			listItem *sub = data->sequence;
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p );
			addItem( &data->sequence, newPair( NULL, sub ) ); }
		else if (( segment )) {
			if ( !segment->value ) segment->value = p;
			// add as-is to on-going expression
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p );
			addItem( &data->sequence, newPair( segment, NULL ) ); }
		else { // special case: ~.
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p );
			addItem( &data->sequence, newPair( NULL, NULL ) ); }
		data->segment = newPair( next_p, NULL ); }
	_break
BMTraverseCBEnd

static char *
optimize( Pair *segment, char *p )
/*
	remove expression's result's enclosing parentheses if possible
*/ {
	segment = segment->name;
	char *bgn = segment->name;
	char *end = segment->value; // segment ended after '('
	if (( bgn==end-1 ) || !strmatch( "~*%)", *(end-2) )) {
		segment->value--; return p+1; }
	return p; }

//===========================================================================
//	free_deternarized
//===========================================================================
void
free_deternarized( listItem *sequence )
/*
	free Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/ {
	listItem *stack = NULL;
	listItem *i = sequence;
	while (( i )) {
		Pair *item = i->ptr;
		if (( item->value )) {
			/* item==[ NULL, sub:Sequence ]
			   We want the following on the stack:
			  	[ item->value, i->next ]
			   as we start traversing item->value==sub
			*/
			item->name = item->value;
			item->value = i->next;
			addItem( &stack, item );
			i = item->name; // traverse sub
			continue; }
		else if (( item->name ))
			freePair( item->name ); // segment
		freePair( item );
		if (( i->next ))
			i = i->next;
		else if (( stack )) {
			do {
				item = popListItem( &stack );
				// free previous Sequence
				freeListItem((listItem**) &item->name );
				// start new one
				i = item->value;
				freePair( item ); }
			while ( !i && (stack) ); }
		else break; }
	freeListItem( &sequence ); }

