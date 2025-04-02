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
bm_deternarize( char **candidate, int type, BMContext *ctx ) {
	if ( !(type&TERNARIZED) ) return NULL;

	char *expression = *candidate;
	int traverse_mode = TERNARY|INFORMED;
	if ( type&DO ) traverse_mode |= LITERAL;

	DeternarizeData data;
	memset( &data, 0, sizeof(data) );
	data.segment = newPair( expression, NULL );
	data.user_CB = pass_CB;
	data.user_data = ctx;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = traverse_mode;

	char *p = deternarize_traversal( expression, &traversal, FIRST );

	if ((data.stack.sequence)||(data.stack.flags)) {
		errout( DeternarizeMemoryLeak, expression, p, data.stack.sequence, data.stack.flags );
		freeListItem( &data.stack.sequence );
		freeListItem( &data.stack.flags );
		exit( -1 ); }
	else if ( data.ternary ) {
		// finish sequence
		if ( data.segment->name != p ) {
			data.segment->value = p;
			addItem( &data.sequence, data.segment ); }
		else freePair( data.segment );
		reorderListItem( &data.sequence );

		// convert sequence to char *string
		CNString *s = newString();
		s_scan( s, data.sequence );
		*candidate = StringFinish( s, 0 );
		StringReset( s, CNStringMode );
		freeString( s );

		// release sequence
		free_deternarized( data.sequence );
		return expression; }
	else {
		freePair( data.segment );
		return NULL; } }

static int
pass_CB( char *guard, void *user_data ) {
	BMContext *ctx = user_data;
	return !strncmp( guard, "(~.:", 4 ) ?
		!bm_feel( BM_CONDITION, guard+4, ctx ) :
		!!bm_feel( BM_CONDITION, guard, ctx ); }

//---------------------------------------------------------------------------
//	s_scan
//---------------------------------------------------------------------------
static void
s_scan( CNString *s, listItem *sequence )
/*
	scan ternary-operator sequence eg. guard
		Sequence:{
			| NULL // ~.
			| segment:[ char *bgn, *end ]
			| [ NULL, sub:Sequence ]
			}
	into CNString
*/ {
	listItem *stack = NULL;
	listItem *i = sequence;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if ( !item ) s_add( "~." )
		else if ( !item->name ) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // parse sub-sequence
			continue; }
		else {
			char *bgn = item->name;
			char *end = item->value;
			s_append( bgn, end ) }
		// moving on
		if (( i->next ))
			i = i->next;
		else if (( stack ))
			i = popListItem( &stack );
		else break; } }

//---------------------------------------------------------------------------
//	deternarize_traversal
//---------------------------------------------------------------------------
/*
	build Sequence:{
		| NULL // ~.
		| segment:[ char *bgn, *end ]
		| [ NULL, sub:Sequence ]
		}
   Note
	only ternary-operated sequences are pushed onto stack.sequence
*/
static inline char *optimize( Pair *, char *, char * );
static inline char *close_segment( Pair *, char * );

BMTraverseCBSwitch( deternarize_traversal )
case_( open_CB )
	if ( p_ternary(p) ) {
		data->ternary = 1;
		Pair *segment = data->segment;
		// finish current Sequence after '(' - without reordering,
		// which will take place after closing
		segment->value = p+1;
		addItem( &data->sequence, segment );
		// push current Sequence on stack.sequence and start new
		addItem( &data->stack.sequence, data->sequence );
		data->sequence = NULL;
		data->segment = newPair( p+1, NULL ); }
	_break
case_( ternary_operator_CB )
	Pair *segment = data->segment;
	// finish sequence==guard, reordered
	segment->value = p;
	addItem( &data->sequence, segment );
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
		char *next_p, *last_p;
		Pair *segment = data->segment;
		if (( data->sequence )) {
			if (( segment )) { // sequence is not guard
				// finish current sequence, reordered
				last_p = close_segment( segment, p );
				addItem( &data->sequence, segment );
				reorderListItem( &data->sequence ); }
			// add as sub-Sequence to on-going expression
			listItem *sub = data->sequence;
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p, last_p );
			addItem( &data->sequence, newPair( NULL, sub ) ); }
		else if (( segment )) {
			last_p = close_segment( segment, p );
			// add as-is to on-going expression
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p, last_p );
			addItem( &data->sequence, segment ); }
		else { // special case: ~.
			data->sequence = popListItem( &data->stack.sequence );
			next_p = optimize( data->sequence->ptr, p, NULL );
			addItem( &data->sequence, NULL ); }
		data->segment = newPair( next_p, NULL ); }
	_break
BMTraverseCBEnd

static inline char *
optimize( Pair *segment, char *p, char *last_p )
/*
	remove expression's result's enclosing parentheses if possible
*/ {
	if (( last_p ) && *last_p==')' ) {
		segment->value--; return p+1; }
	char *bgn = segment->name;
	char *end = segment->value; // segment ended after '('
	if (( bgn==end-1 ) || !strmatch( "~*%)", *(end-2) )) {
		segment->value--; return p+1; }
	return p; }

static inline char *
close_segment( Pair *segment, char *p ) {
	if ( !segment->value )
		segment->value = p;
	else p = segment->value;
	return p-1; }

//===========================================================================
//	free_deternarized
//===========================================================================
void
free_deternarized( listItem *sequence )
/*
	free Sequence:{
		| NULL // ~.
		| segment:[ char *bgn, *end ]
		| [ NULL, sub:Sequence ]
		}
*/ {
	listItem *stack = NULL;
	listItem *i = sequence;
	while (( i )) {
		Pair *item = i->ptr;
		if (( item )) {
			if ( !item->name ) {
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
			freePair( item ); }
		i = i->next;
		while ( !i && (stack) ) {
			item = popListItem( &stack );
			// free previous Sequence
			freeListItem((listItem**) &item->name );
			// start new one
			i = item->value;
			freePair( item ); } }
	freeListItem( &sequence ); }

