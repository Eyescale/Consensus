#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "expression.h"
#include "traverse.h"

typedef int BMTernaryCB( char *, void * );

//===========================================================================
//	bm_deternarize
//===========================================================================
static char *deternarize( char *expression, BMTernaryCB, void *user_data );
static BMTernaryCB pass_CB;

char *
bm_deternarize( char **expression, BMContext *ctx )
{
	char *backup = *expression;
	char *deternarized = deternarize( backup, pass_CB, ctx );
	if (( deternarized )) {
		*expression = deternarized;
		return backup;
	}
	else return NULL;
}
static int
pass_CB( char *guard, void *user_data )
{
	BMContext *ctx = user_data;
	return !!bm_feel( BM_CONDITION, guard, ctx );
}

//===========================================================================
//	deternarize
//===========================================================================
static void free_deternarized( listItem *sequence );
static void s_scan( CNString *, listItem * );
static BMTraverseCB
	sub_expression_CB, dot_expression_CB, open_CB, ternary_operator_CB,
	filter_CB, close_CB;
typedef struct {
	BMTernaryCB *user_CB;
	void *user_data;
	int ternary;
	struct { listItem *sequence, *flags; } stack;
	listItem *sequence;
	Pair *segment;
} DeternarizeData;
#define case_( func ) \
	} static BMCB_take func( BMTraverseData *traverse_data, char *p, int flags ) { \
		DeternarizeData *data = traverse_data->user_data;

static char *
deternarize( char *expression, BMTernaryCB user_CB, void *user_data )
/*
	build Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/
{
	char *deternarized = NULL;

	DeternarizeData data;
	memset( &data, 0, sizeof(data) );
	data.segment = newPair( expression, NULL );
	data.user_CB = user_CB;
	data.user_data = user_data;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMSubExpressionCB ]	= open_CB;
	table[ BMDotExpressionCB ]	= open_CB;
	table[ BMOpenCB ]		= open_CB;
	table[ BMTernaryOperatorCB ]	= ternary_operator_CB;
	table[ BMFilterCB ]		= filter_CB;
	table[ BMCloseCB ]		= close_CB;
	bm_traverse( expression, &traverse_data, &data.stack.flags, FIRST );
	
	if ( !data.ternary )
		freePair( data.segment );
	else {
		// finish current sequence, reordered
		if ( data.segment->name != traverse_data.p ) {
			data.segment->value = traverse_data.p;
			addItem( &data.sequence, newPair( data.segment, NULL ) );
		} else freePair( data.segment );
		reorderListItem( &data.sequence );

		// convert sequence to char *string
		CNString *s = newString();
		s_scan( s, data.sequence );
		deternarized = StringFinish( s, 0 );
		StringReset( s, CNStringMode );
		freeString( s );

		// release sequence
		free_deternarized( data.sequence );
	}
	// return deternarized
	if ((data.stack.sequence)||(data.stack.flags)) {
		fprintf( stderr, ">>>>> B%%: Error: deternarize: Memory Leak\n" );
		freeListItem( &data.stack.sequence );
		freeListItem( &data.stack.flags );
		free( deternarized );
		exit( -1 ); }
	return deternarized;
}

BMTraverseCBSwitch( deternarize_traversal )
/*
   Note: only pre-ternary-operated sequences are pushed on stack.sequence
*/
case_( open_CB )
	if ( *p=='.' || *p=='%' ) p++;
	if ( p_ternary(p) ) {
		data->ternary = 1;
		Pair *segment = data->segment;
		// finish current Sequence after '(' - without reordering,
		// which will take place on return
		segment->value = p+1;
		addItem( &data->sequence, newPair( segment, NULL ) );
		// push current Sequence on stack.sequence and start new
		addItem( &data->stack.sequence, data->sequence );
		data->sequence = NULL;
		data->segment = newPair( p+1, NULL ); }
	_break
case_( ternary_operator_CB )
	f_set( TERNARY )
	f_clr( NEGATED|FILTERED|INFORMED )
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
		// proceed to alternative
		p = p_prune( PRUNE_TERNARY, p ); // ":"
		if ( p[1]==')' || p[1]==':' ) {
			// ~. is our current candidate
			data->segment = NULL;
			// proceed to ")"
			_prune( BM_PRUNE_TERNARY ) }
		else {
			// resume past ':'
			data->segment = newPair( p+1, NULL );
			_continue( p+1 ) } }
	else if ( p[1]==':' ) {
		// sequence==guard is our current candidate
		// sequence is already informed and completed
		data->segment = NULL;
		// proceed to ")"
		p++; _prune( BM_PRUNE_TERNARY ) }
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
		_prune( BM_PRUNE_TERNARY ) }
	else _break
case_( close_CB )
	if is_f( TERNARY ) {
		Pair *segment = data->segment;
		if (( data->sequence )) {
			if (( segment )) { // sequence is not guard
				// finish current sequence, reordered
				if ( !segment->value ) segment->value = p;
				addItem( &data->sequence, newPair( segment, NULL ) );
				reorderListItem( &data->sequence );
			}
			// add as sub-Sequence to on-going expression
			listItem *sub = data->sequence;
			data->sequence = popListItem( &data->stack.sequence );
			addItem( &data->sequence, newPair( NULL, sub ) ); }
		else if (( segment )) {
			if ( !segment->value ) segment->value = p;
			// add as-is to on-going expression
			data->sequence = popListItem( &data->stack.sequence );
			addItem( &data->sequence, newPair( segment, NULL ) ); }
		else { // special case: ~.
			data->sequence = popListItem( &data->stack.sequence );
			addItem( &data->sequence, newPair( NULL, NULL ) ); }
		data->segment = newPair( p, NULL ); }
	_break
BMTraverseCBEnd

//===========================================================================
//	free_deternarized
//===========================================================================
static void
free_deternarized( listItem *sequence )
/*
	free Sequence:{
		| [ segment:Segment, NULL ]
		| [ NULL, sub:Sequence ]
		| [ NULL, NULL ] // ~.
		}
	where
		Segment:[ name, value ]	// a.k.a. { char *bgn, *end; } 
*/
{
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
				freePair( item );
			}
			while ( !i && (stack) );
		}
		else break;
	}
	freeListItem( &sequence );
}

//===========================================================================
//	s_scan
//===========================================================================
#define s_append( bgn, end ) \
	for ( char *p=bgn; p!=end; StringAppend(s,*p++) );
#define s_add( str ) \
	for ( char *p=str; *p; StringAppend(s,*p++) );

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
*/
{
	listItem *stack = NULL;
	listItem *i = sequence;
	for ( ; ; ) {
		Pair *item = i->ptr;
		if (( item->value )) {
			// item==[ NULL, sub:Sequence ]
			if (( i->next )) addItem( &stack, i->next );
			i = item->value; // parse sub-sequence
			continue;
		}
		else if (( item->name )) {
			// item==[ segment:Segment, NULL ]
			Pair *segment = item->name;
			s_append( segment->name, segment->value )
		}
		else s_add( "~." )
		// moving on
		if (( i->next ))
			i = i->next;
		else if (( stack ))
			i = popListItem( &stack );
		else break;
	}
}

