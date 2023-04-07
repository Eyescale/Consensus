#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "scour.h"
#include "locate_mark.h"
#include "locate_pivot.h"

//===========================================================================
//	bm_locate_pivot
//===========================================================================
#include "locate_pivot_traversal.h"

typedef struct {
	char *expression;
	int primary, secondary;
	listItem **exponent;
	listItem *level;
	struct { listItem *flags, *level, *premark; } stack;
} LocatePivotData;

char *
bm_locate_pivot( char *expression, listItem **exponent )
/*
	returns first term (according to prioritization) which
	is not a wildcard and is not negated, with corresponding
	exponent (in reverse order).
*/
{
	listItem *base = *exponent;

	LocatePivotData data;
	memset( &data, 0, sizeof(data) );
	data.expression = expression;
	data.primary = EENOK|EMARK|QMARK;
	data.exponent = exponent;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;

	char *p = locate_pivot_traversal( expression, &traverse_data, FIRST );

	switch ( traverse_data.done ) {
	case 2:
		break;
	default:
		if ( data.secondary ) {
			xpn_free( exponent, base );
			traverse_data.done = 0;
			data.primary = data.secondary;
			p = locate_pivot_traversal( expression, &traverse_data, FIRST ); }
		else {
			xpn_free( exponent, base );
			return NULL; } }

	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
	freeListItem( &data.stack.premark );
	return p;
}

//---------------------------------------------------------------------------
//	locate_pivot_traversal
//---------------------------------------------------------------------------
#define CHECK( this ) \
	(!( data->primary & this )) { \
		if ( !data->secondary || ( this < data->secondary )) \
			data->secondary = this; } \
	else

BMTraverseCBSwitch( locate_pivot_traversal )
case_( dot_expression_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) ) {
		if CHECK( PERSO ) {
			xpn_add( exponent, AS_SUB, 0 );
			_return( 2 ) } }

	// apply dot operator to whatever comes next
	xpn_add( exponent, AS_SUB, 1 );
	_break
case_( dot_identifier_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) ) {
		if CHECK( PERSO ) {
			xpn_add( exponent, AS_SUB, 0 );
			_return( 2 ); } }

	// apply dot operator to whatever comes next
	xpn_add( exponent, AS_SUB, 1 );
	if ( !is_f(NEGATED) ) {
		if CHECK( IDENTIFIER ) {
			(*q)++;
			_return( 2 ) } }
	_break
case_( identifier_CB )
	if ( !is_f(NEGATED) ) {
		if CHECK( IDENTIFIER )
			_return( 2 ) }
	_break
case_( character_CB )
	if ( !is_f(NEGATED) ) {
		if CHECK( CHARACTER )
			_return( 2 ) }
	_break
case_( mod_character_CB )
	if ( !is_f(NEGATED) ) {
		if CHECK( MOD )
			_return( 2 ) }
	_break
case_( star_character_CB )
	if ( !is_f(NEGATED) ) {
		if CHECK( STAR )
			_return( 2 ) }
	_break
case_( register_variable_CB )
	if ( !is_f(NEGATED) ) {
		int mark;
		switch ( p[1] ) {
		case '<': mark = EENOK; break;
		case '?': mark = QMARK; break;
		case '!': mark = EMARK; break;
		case '|': mark = PMARK; break;
		case '.': mark = PARENT; break;
		case '%': mark = SELF; break; }
		if CHECK( mark )
			_return( 2 ) }
	_break
case_( dereference_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) ) {
		if CHECK( STAR ) {
			xpn_add( exponent, SUB, 1 );
			xpn_add( exponent, AS_SUB, 0 );
			xpn_add( exponent, AS_SUB, 0 );
			_return( 2 ) } }
	// apply dereferencing operator to whatever comes next
	xpn_add( exponent, SUB, 1 );
	xpn_add( exponent, AS_SUB, 0 );
	xpn_add( exponent, AS_SUB, 1 );
	_break
case_( sub_expression_CB )
	listItem *mark_exp = NULL;
	char *mark = bm_locate_mark( p+1, &mark_exp );
	if ((mark_exp) && !strncmp( mark+1, ":...", 4 )) {
		freeListItem( &mark_exp );
		if ( !is_f(NEGATED) ) {
			if CHECK( LIST_EXPR ) {
				Pair * list_expr = newPair( p, mark+6 );
				addItem( data->exponent, list_expr );
				_return( 2 ) } }
		_continue( mark+6 ) } // _prune( BM_PRUNE_FILTER )
	listItem **exponent = data->exponent;
	addItem( &data->stack.premark, *exponent );
	if (( mark_exp )) {
		do addItem( exponent, popListItem(&mark_exp) );
		while (( mark_exp )); }
	_break
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( data->exponent, AS_SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( filter_CB )
	xpn_free( data->exponent, data->level );
	_break
case_( decouple_CB )
	xpn_free( data->exponent, data->level );
	xpn_set( *data->exponent, AS_SUB, 1 );
	_break
case_( close_CB )
	xpn_free( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	if ( is_f(SUB_EXPR) && !is_f(LEVEL) ) {
		listItem *tag = popListItem( &data->stack.premark );
		if ((tag)) xpn_free( data->exponent, tag ); }
	_break;
BMTraverseCBEnd

