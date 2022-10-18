#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"
#include "locate.h"

//===========================================================================
//	bm_locate_pivot
//===========================================================================
static BMTraverseCB
	dot_identifier_CB, identifier_CB, character_CB, mod_character_CB,
	star_character_CB, mark_register_CB, dereference_CB, sub_expression_CB,
	dot_expression_CB, open_CB, filter_CB, decouple_CB, close_CB;
typedef struct {
	int target;
	listItem **exponent;
	listItem *level;
	struct { listItem *flags, *level, *premark; } stack;
} BMLocatePivotData;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMLocatePivotData *data = traverse_data->user_data; char *p = *q;

char *
bm_locate_pivot( char *expression, listItem **exponent )
/*
	returns first term (according to prioritization) which
	is not a wildcard and is not negated, with corresponding
	exponent (in reverse order).
*/
{
	int target = bm_scour( expression, THIS|QMARK|IDENTIFIER );
	if ( target == 0 ) return NULL;
	else target =	( target & THIS) ? THIS :
			( target & QMARK ) ? QMARK :
			( target & IDENTIFIER ) ? IDENTIFIER :
			( target & MOD ) ? MOD :
			( target & CHARACTER ) ? CHARACTER :
			( target & STAR ) ? STAR :
			( target & PMARK ) ? PMARK : 0;

	BMLocatePivotData data;
	memset( &data, 0, sizeof(data) );
	data.target = target;
	data.exponent = exponent;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMDotIdentifierCB ]	= dot_identifier_CB;
	table[ BMIdentifierCB ]		= identifier_CB;
	table[ BMCharacterCB ]		= character_CB;
	table[ BMModCharacterCB ]	= mod_character_CB;
	table[ BMStarCharacterCB ]	= star_character_CB;
	table[ BMMarkRegisterCB ]	= mark_register_CB;
	table[ BMDereferenceCB ]	= dereference_CB;
	table[ BMSubExpressionCB ]	= sub_expression_CB;
	table[ BMDotExpressionCB ]	= dot_expression_CB;
	table[ BMOpenCB ]		= open_CB;
	table[ BMFilterCB ]		= filter_CB;
	table[ BMDecoupleCB ]		= decouple_CB;
	table[ BMCloseCB ]		= close_CB;
	bm_traverse( expression, &traverse_data, FIRST );

	if ( data.stack.flags ) {
		freeListItem( &data.stack.flags );
		freeListItem( &data.stack.level );
		freeListItem( &data.stack.premark );
	}
	return ( traverse_data.done==2 ? traverse_data.p : NULL );
}
#define pop_exponent( exponent, level ) \
	for ( listItem **exp=exponent; *exp!=level; popListItem( exp ) );

BMTraverseCBSwitch( bm_locate_pivot_traversal )
case_( dot_identifier_CB )
	listItem **exponent = data->exponent;
	if ( !is_f(NEGATED) && data->target==THIS ) {
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
case_( mark_register_CB )
	if ( !is_f(NEGATED) && data->target==( p[1]=='?' ? QMARK : PMARK ) )
		_return( 2 )
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
	if ( !is_f(NEGATED) && data->target==THIS ) {
		xpn_add( exponent, AS_SUB, 0 );
		_return( 2 )
	}
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

//===========================================================================
//	bm_scour
//===========================================================================
static BMTraverseCB
	sc_dot_expr_CB, sc_identifier_CB, sc_star_character_CB,
	sc_mod_character_CB, sc_character_CB, sc_mark_register_CB;
typedef struct {
	int candidate, target;
} BMScourData;
#undef case_
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMScourData *data = traverse_data->user_data; char *p = *q;

int
bm_scour( char *expression, int target )
{
	BMScourData data;
	memset( &data, 0, sizeof(data) );
	data.target = target;
	listItem *stack = NULL;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMDotIdentifierCB ]	= sc_dot_expr_CB;
	table[ BMDotExpressionCB ]	= sc_dot_expr_CB;
	table[ BMIdentifierCB ]		= sc_identifier_CB;
	table[ BMCharacterCB ]		= sc_character_CB;
	table[ BMModCharacterCB ]	= sc_mod_character_CB;
	table[ BMStarCharacterCB ]	= sc_star_character_CB;
	table[ BMDereferenceCB ]	= sc_star_character_CB;
	table[ BMMarkRegisterCB ]	= sc_mark_register_CB;
	bm_traverse( expression, &traverse_data, FIRST );

	freeListItem( &stack );
	return data.candidate;
}

BMTraverseCBSwitch( bm_scour_traversal )
case_( sc_dot_expr_CB )
	if ( !is_f(NEGATED) ) {
		data->candidate |= THIS;
		if ( data->target & THIS )
			_return( 1 ) }
	_break
case_( sc_identifier_CB )
	if ( !is_f(NEGATED) ) {
		data->candidate |= IDENTIFIER;
		if ( data->target & IDENTIFIER )
			_return( 1 ) }
	_break
case_( sc_character_CB )
	if ( !is_f(NEGATED) )
		data->candidate |= CHARACTER;
	_break
case_( sc_mod_character_CB )
	if ( !is_f(NEGATED) )
		data->candidate |= MOD;
	_break
case_( sc_star_character_CB )
	if ( !is_f(NEGATED) )
		data->candidate |= STAR;
	_break
case_( sc_mark_register_CB )
	if ( !is_f(NEGATED) )
		switch ( p[1] ) {
		case '?':
			data->candidate |= QMARK;
			if ( data->target & QMARK )
				_return( 1 )
			break;
		default:
			data->candidate |= PMARK;
		}
	_break
BMTraverseCBEnd

//===========================================================================
//	bm_locate_mark
//===========================================================================
char *
bm_locate_mark( char *expression, listItem **exponent )
{
	return bm_locate_param( expression, exponent, NULL, NULL );
}

//===========================================================================
//	bm_locate_param
//===========================================================================
static BMTraverseCB
	not_CB, deref_CB, sub_expr_CB, dot_push_CB, push_CB, sift_CB, sep_CB,
	pop_CB, wildcard_CB, parameter_CB;
typedef struct {
	listItem **exponent;
	BMLocateCB *param_CB;
	void *user_data;
	listItem *level;
	struct { listItem *flags, *level; } stack;
} BMLocateParamData;
#undef case_
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMLocateParamData *data = traverse_data->user_data; char *p = *q;

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
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMNotCB ]		= not_CB;
	table[ BMDereferenceCB ]	= deref_CB;
	table[ BMSubExpressionCB ]	= sub_expr_CB;
	table[ BMDotExpressionCB ]	= dot_push_CB;
	table[ BMOpenCB ]		= push_CB;
	table[ BMFilterCB ]		= sift_CB;
	table[ BMDecoupleCB ]		= sep_CB;
	table[ BMCloseCB ]		= pop_CB;
	table[ BMWildCardCB ]		= wildcard_CB;
	table[ BMDotIdentifierCB ]	= parameter_CB;
	char *p = bm_traverse( expression, &traverse_data, FIRST );

	if ( data.stack.flags ) {
		freeListItem( &data.stack.flags );
		freeListItem( &data.stack.level );
	}
	return ( *p=='?' ? p : NULL );
}

BMTraverseCBSwitch( bm_locate_param_traversal )
case_( not_CB )
	if (( data->param_CB )) {
		_prune( BM_PRUNE_FILTER )
	}
	else _break
case_( deref_CB )
	if (( data->param_CB )) {
		_prune( BM_PRUNE_FILTER )
	}
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

//===========================================================================
//	xpn_add, xpn_set
//===========================================================================
void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
}
void
xpn_set( listItem *xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	xp->ptr = icast.ptr;
}

//===========================================================================
//	xpn_out
//===========================================================================
void
xpn_out( FILE *stream, listItem *xp )
{
	union { int value; void *ptr; } icast;
	while ( xp ) {
		icast.ptr = xp->ptr;
		fprintf( stream, "%d", icast.value );
		xp = xp->next;
	}
}

