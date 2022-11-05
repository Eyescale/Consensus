#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "locate_param.h"
#include "locate_pivot.h"
#include "scour.h"

//===========================================================================
//	bm_locate_pivot
//===========================================================================
#include "locate_pivot_traversal.h"

typedef struct {
	int target;
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
	int target = bm_scour( expression, EENO|QMARK|EMARK );
	if ( target == 0 ) return NULL;
	else target =	( target & EENO ) ? EENO :
			( target & QMARK ) ? QMARK :
			( target & EMARK ) ? EMARK :
			( target & PMARK ) ? PMARK :
			( target & PARENT ) ? PARENT :
			( target & SELF ) ? SELF :
			( target & PERSO ) ? PERSO :
			( target & IDENTIFIER ) ? IDENTIFIER :
			( target & MOD ) ? MOD :
			( target & CHARACTER ) ? CHARACTER :
			( target & STAR ) ? STAR : 0;

	LocatePivotData data;
	memset( &data, 0, sizeof(data) );
	data.target = target;
	data.exponent = exponent;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;

	char *p = locate_pivot_traversal( expression, &traverse_data, FIRST );

	if ( data.stack.flags ) {
		freeListItem( &data.stack.flags );
		freeListItem( &data.stack.level );
		freeListItem( &data.stack.premark );
	}
	return ( traverse_data.done==2 ? p : NULL );
}

//---------------------------------------------------------------------------
//	locate_pivot_traversal
//---------------------------------------------------------------------------
#define pop_exponent( exponent, level ) \
	for ( listItem **exp=exponent; *exp!=level; popListItem( exp ) );

BMTraverseCBSwitch( locate_pivot_traversal )
case_( dot_identifier_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) && data->target==PERSO ) {
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 )
	}
	// apply dot operator to whatever comes next
	xpn_add( exponent, AS_SUB, 1 );
	if ( !is_f(NEGATED) && data->target==IDENTIFIER )
		{ (*q)++; _return( 2 ) }
	_break
case_( identifier_CB )
	if ( !is_f(NEGATED) && data->target==IDENTIFIER )
		_return( 2 )
	_break
case_( character_CB )
	if ( !is_f(NEGATED) && data->target==CHARACTER )
		_return( 2 )
	_break
case_( mod_character_CB )
	if ( !is_f(NEGATED) && data->target==MOD )
		_return( 2 )
	_break
case_( star_character_CB )
	if ( !is_f(NEGATED) && data->target==STAR )
		_return( 2 )
	_break
case_( register_variable_CB )
	if ( !is_f(NEGATED) )
		switch ( p[1] ) {
		case '<': if ( data->target==EENO ) _return( 2 ) else break;
		case '?': if ( data->target==QMARK ) _return( 2 ) else break;
		case '!': if ( data->target==EMARK ) _return( 2 ) else break;
		case '|': if ( data->target==PMARK ) _return( 2 ) else break;
		case '.': if ( data->target==PARENT ) _return( 2 ) else break;
		case '%': if ( data->target==SELF ) _return( 2 ) else break; }
	_break
case_( dereference_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) && data->target==STAR ) {
		xpn_add( exponent, SUB, 1 );
		xpn_add( exponent, AS_SUB, 0 );
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 ) }
	// apply dereferencing operator to whatever comes next
	xpn_add( exponent, SUB, 1 );
	xpn_add( exponent, AS_SUB, 0 );
	xpn_add( exponent, AS_SUB, 1 );
	_break
case_( sub_expression_CB )
	listItem *mark_exp = NULL;
	bm_locate_mark( p+1, &mark_exp );
	listItem **exponent = data->exponent;
	addItem( &data->stack.premark, *exponent );
	if (( mark_exp )) {
		do addItem( exponent, popListItem(&mark_exp) );
		while (( mark_exp )); }
	_break
case_( dot_expression_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) && data->target==PERSO ) {
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 ) }
	// apply dot operator to whatever comes next
	xpn_add( exponent, AS_SUB, 1 );
	_break
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( data->exponent, AS_SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( filter_CB )
	pop_exponent( data->exponent, data->level );
	_break
case_( decouple_CB )
	pop_exponent( data->exponent, data->level );
	xpn_set( *data->exponent, AS_SUB, 1 );
	_break
case_( close_CB )
	pop_exponent( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	if ( is_f(SUB_EXPR) && !is_f(LEVEL) ) {
		listItem *tag = popListItem( &data->stack.premark );
		if ((tag)) pop_exponent( data->exponent, tag ); }
	_break;
BMTraverseCBEnd

