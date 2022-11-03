#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "locate_pivot.h"
#include "locate_param.h"
#include "locate_param_traversal.h"

//===========================================================================
//	bm_locate_param
//===========================================================================
typedef struct {
	listItem **exponent;
	BMLocateCB *param_CB;
	void *user_data;
	listItem *level;
	struct { listItem *flags, *level; } stack;
} BMLocateParamData;

static BMTraversal bm_locate_param_traversal;

#define BMNotCB			not_CB
#define BMDereferenceCB		deref_CB
#define BMSubExpressionCB	sub_expr_CB
#define BMDotExpressionCB	dot_push_CB
#define BMOpenCB		push_CB
#define BMFilterCB		sift_CB
#define BMDecoupleCB		sep_CB
#define BMCloseCB		pop_CB
#define BMWildCardCB		wildcard_CB
#define BMDotIdentifierCB	parameter_CB

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
	BMLocateParamData data;
	memset( &data, 0, sizeof(data) );
	data.exponent = exponent;
	data.param_CB = param_CB;
	data.user_data = user_data;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = 0;

	char *p = bm_locate_param_traversal( expression, &traverse_data, FIRST );

	if ( data.stack.flags ) {
		freeListItem( &data.stack.flags );
		freeListItem( &data.stack.level ); }

	if ( *p=='?' )
		return p;
	else {
		freeListItem( exponent );
		return NULL; }
}

//---------------------------------------------------------------------------
//	bm_locate_param_traversal
//---------------------------------------------------------------------------
#include "traversal.h"

#define pop_exponent( exponent, level ) \
	for ( listItem **exp=exponent; *exp!=level; popListItem( exp ) );

BMTraverseCBSwitch( bm_locate_param_traversal )
case_( not_CB )
	if (( data->param_CB ))
		_prune( BM_PRUNE_FILTER )
	else _break
case_( deref_CB )
	if (( data->param_CB ))
		_prune( BM_PRUNE_FILTER )
	listItem **exponent = data->exponent;
	xpn_add( exponent, AS_SUB, 1 );
	xpn_add( exponent, SUB, 0 );
	xpn_add( exponent, SUB, 1 );
	_break
case_( sub_expr_CB )
	_prune( BM_PRUNE_FILTER )
case_( dot_push_CB )
	xpn_add( data->exponent, SUB, 1 );
	_break
case_( push_CB )
	if ( f_next & COUPLE )
		xpn_add( data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( sift_CB )
	pop_exponent( data->exponent, data->level );
	_break
case_( sep_CB )
	pop_exponent( data->exponent, data->level );
	xpn_set( *data->exponent, SUB, 1 );
	_break
case_( pop_CB )
	pop_exponent( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	if is_f( DOT )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( wildcard_CB )
	if ( *p=='?' && !data->param_CB ) _return( 2 )
	else _break
case_( parameter_CB )
	if (( data->param_CB ))
	 	data->param_CB( p+1, *data->exponent, data->user_data );
	_break
BMTraverseCBEnd

