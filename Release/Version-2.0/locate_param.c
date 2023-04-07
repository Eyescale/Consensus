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

char *
bm_locate_param( char *expression, listItem **exponent, BMLocateCB param_CB, void *user_data )
/*
	if param_CB is set
		invokes param_CB on each .param found in expression
	Otherwise
		returns first '?' found in expression, together with exponent
	Note that we do not enter the '~' signed and %(...) sub-expressions
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

	if ( data.stack.flags ) {
		freeListItem( &data.stack.flags );
		freeListItem( &data.stack.level ); }

	if ( *p=='?' ) {
		return p; }
	else {
		freeListItem( exponent );
		return NULL; }
}

//---------------------------------------------------------------------------
//	locate_param_traversal
//---------------------------------------------------------------------------
#define pop_exponent( exponent, level ) \
	for ( listItem **exp=exponent; *exp!=level; popListItem( exp ) );

BMTraverseCBSwitch( locate_param_traversal )
case_( not_CB )
	if (( data->param_CB ))
		_prune( BM_PRUNE_FILTER )
	_break
case_( dereference_CB )
	if (( data->param_CB ))
		_prune( BM_PRUNE_FILTER )
	listItem **exponent = data->exponent;
	xpn_add( exponent, AS_SUB, 1 );
	xpn_add( exponent, SUB, 0 );
	xpn_add( exponent, SUB, 1 );
	_break
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
	pop_exponent( data->exponent, data->level );
	_break
case_( decouple_CB )
	pop_exponent( data->exponent, data->level );
	xpn_set( *data->exponent, SUB, 1 );
	_break
case_( close_CB )
	pop_exponent( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( wildcard_CB )
	if ( *p=='?' && !data->param_CB )
		_return( 2 )
	_break
case_( dot_identifier_CB )
	if (( data->param_CB ))
	 	data->param_CB( p+1, *data->exponent, data->user_data );
	_break
BMTraverseCBEnd

