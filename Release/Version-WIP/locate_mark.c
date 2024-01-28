#include <stdio.h>
#include <stdlib.h>

#include "locate_mark.h"

//===========================================================================
//	locate_mark_traversal
//===========================================================================
#include "locate_mark_traversal.h"
typedef struct {
	listItem **exponent;
	listItem *level;
	struct { listItem *flags, *level; } stack;
	} LocateMarkData;

BMTraverseCBSwitch( locate_mark_traversal )
case_( dereference_CB )
	listItem **exponent = data->exponent;
	xpn_add( exponent, AS_SUB, 1 );
	xpn_add( exponent, SUB, 0 );
	xpn_add( exponent, SUB, 1 );
	_break
case_( dot_identifier_CB )
	if ( p[1]=='?' ) {
		xpn_add( data->exponent, SUB, 1 );
		_return( 2 ); }
	_break
case_( sub_expression_CB )
	_prune( BM_PRUNE_FILTER, p+1 )
case_( dot_expression_CB )
	xpn_add( data->exponent, SUB, 1 );
	_break
case_( open_CB )
	if ( f_next & ASSIGN )
		xpn_add( data->exponent, SUB, 1 );
	else if ( f_next & COUPLE )
		xpn_add( data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( filter_CB )
	xpn_free( data->exponent, data->level );
	_break
case_( decouple_CB )
	xpn_free( data->exponent, data->level );
	xpn_set( *data->exponent, SUB, 1 );
	_break
case_( close_CB )
	xpn_free( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	else if is_f( ASSIGN )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( wildcard_CB )
	if (( *p=='?' )||is_f( ELLIPSIS ))
		_return( 2 )
	_break
BMTraverseCBEnd

//===========================================================================
//	bm_locate_mark
//===========================================================================
char *
bm_locate_mark( char *expression, listItem **exponent )
/*
	returns first '?' found in sub-expression, together with exponent
	Assumption: negated mark is ruled out at expression creation time
	Note that we do not enter %(...) sub-sub-expressions
	Note also that the caller is expected to reorder the list of exponents
*/ {
	LocateMarkData data;
	memset( &data, 0, sizeof(data) );
	data.exponent = exponent;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = SUB_EXPR;

	char *p = locate_mark_traversal( expression, &traverse_data, FIRST );

	freeListItem( &data.stack.level );
	freeListItem( &data.stack.flags );
	switch ( traverse_data.done ) {
	case 2:
		return p;
	default:
		freeListItem( exponent );
		return NULL; } }

