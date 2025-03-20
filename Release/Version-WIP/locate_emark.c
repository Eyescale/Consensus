#include <stdio.h>
#include <stdlib.h>

#include "scour.h"
#include "locate_emark.h"
#include "locate_emark_traversal.h"

//===========================================================================
//	bm_locate_emark
//===========================================================================
typedef struct {
	char *expression;
	listItem **exponent;
	listItem *level;
	struct { listItem *flags, *level; } stack;
	} LocateEMarkData;

char *
bm_locate_emark( char *expression, listItem **exponent )
/*
	returns position of '!' in %<(...)>, together with exponent
		expression ----------^
	Also called on expression in %!/(...):%(_)/
		where expression is assumed to contain '!'
*/ {
	listItem *base = *exponent;

	LocateEMarkData data;
	memset( &data, 0, sizeof(data) );
	data.expression = expression;
	data.exponent = exponent;

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = SUB_EXPR;
	char *p = locate_emark_traversal( expression, &traversal, FIRST );

	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
	return p; }

//---------------------------------------------------------------------------
//	locate_emark_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( locate_emark_traversal )
case_( emark_CB )
	_return( 2 )
case_( open_CB )
	if is_f_next( COUPLE )
		xpn_add( data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( comma_CB )
	xpn_free( data->exponent, data->level );
	xpn_set( *data->exponent, SUB, 1 );
	_break
case_( close_CB )
	xpn_free( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( prune_CB )
	_prune( BMT_PRUNE_TERM, p+1 )
case_( end_CB )
	_return( 1 )
BMTraverseCBEnd

