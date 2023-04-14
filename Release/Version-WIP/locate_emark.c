#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "scour.h"
#include "locate_emark.h"

//===========================================================================
//	bm_locate_emark
//===========================================================================
#include "locate_emark_traversal.h"

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
*/
{
	listItem *base = *exponent;

	LocateEMarkData data;
	memset( &data, 0, sizeof(data) );
	data.expression = expression;
	data.exponent = exponent;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;
	traverse_data.done = SUB_EXPR;
	char *p = locate_emark_traversal( expression, &traverse_data, FIRST );

	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
	return p;
}

//---------------------------------------------------------------------------
//	locate_emark_traversal
//---------------------------------------------------------------------------

BMTraverseCBSwitch( locate_emark_traversal )
case_( emark_CB )
	_return( 2 )
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( data->exponent, AS_SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = *data->exponent;
	_break
case_( decouple_CB )
	xpn_free( data->exponent, data->level );
	xpn_set( *data->exponent, AS_SUB, 1 );
	_break
case_( close_CB )
	xpn_free( data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
BMTraverseCBEnd

