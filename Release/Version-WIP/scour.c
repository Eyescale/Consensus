#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "scour.h"
#include "scour_traversal.h"

//===========================================================================
//	bm_scour
//===========================================================================
typedef struct {
	int candidate, target;
} BMScourData;

static BMTraversal bm_scour_traversal;

#define BMDotIdentifierCB	sc_dot_expr_CB
#define BMDotExpressionCB	sc_dot_expr_CB
#define BMIdentifierCB		sc_identifier_CB
#define BMCharacterCB		sc_character_CB
#define BMModCharacterCB	sc_mod_character_CB
#define BMStarCharacterCB	sc_star_character_CB
#define BMDereferenceCB		sc_star_character_CB
#define BMRegisterVariableCB	sc_register_variable_CB

int
bm_scour( char *expression, int target )
{
	BMScourData data;
	data.candidate = 0;
	data.target = target;
	listItem *stack = NULL;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED;

	char *p = bm_scour_traversal( expression, &traverse_data, FIRST );

	freeListItem( &stack );
	return data.candidate;
}

//---------------------------------------------------------------------------
//	bm_scour_traversal
//---------------------------------------------------------------------------
#include "traversal.h"

BMTraverseCBSwitch( bm_scour_traversal )
case_( sc_dot_expr_CB )
	if ( !is_f(NEGATED) ) {
		data->candidate |= PERSO;
		if ( data->target & PERSO )
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
case_( sc_register_variable_CB )
	if ( !is_f(NEGATED) ) {
		int mark;
		switch ( p[1] ) {
		case '<': mark = EENO; break;
		case '?': mark = QMARK; break;
		case '!': mark = EMARK; break;
		case '|': mark = PMARK; break;
		case '.': mark = PARENT; break;
		case '%': mark = SELF; break; }
		data->candidate |= mark;
		if ( data->target & mark )
			_return( 1 ) }
	_break
BMTraverseCBEnd

