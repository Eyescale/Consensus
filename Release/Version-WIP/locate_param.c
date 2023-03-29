#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "locate_param.h"

//===========================================================================
//	bm_locate_param
//===========================================================================
#include "locate_param_traversal.h"

typedef struct {
	listItem **exponent;
	BMLocateCB *param_CB;
	void *user_data;
	listItem *level;
	struct { listItem *flags, *level; } stack;
} LocateParamData;

void
bm_locate_param( char *expression, listItem **exponent, BMLocateCB param_CB, void *user_data )
/*
	invokes param_CB on each .param found in expression
	Note that we do not enter negated, starred or %(...) expressions
	Note also that the caller is expected to reorder the list of exponents
*/
{
	LocateParamData data;
	memset( &data, 0, sizeof(data) );
	data.exponent = exponent;
	data.param_CB = param_CB;
	data.user_data = user_data;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;

	char *p = locate_param_traversal( expression, &traverse_data, FIRST );

	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
}

//---------------------------------------------------------------------------
//	locate_param_traversal
//---------------------------------------------------------------------------

BMTraverseCBSwitch( locate_param_traversal )
case_( sub_expression_CB )
	_prune( BM_PRUNE_FILTER )
case_( dot_expression_CB )
	xpn_add( data->exponent, SUB, 1 );
	_break
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( filter_CB )
	xpn_pop( data->exponent, data->level );
	_break
case_( decouple_CB )
	xpn_pop( data->exponent, data->level );
	xpn_set( *data->exponent, SUB, 1 );
	_break
case_( close_CB )
	xpn_pop( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( dot_identifier_CB )
	 data->param_CB( p+1, *data->exponent, data->user_data );
	_break
BMTraverseCBEnd

