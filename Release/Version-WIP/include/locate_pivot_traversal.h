#ifndef LOCATE_PIVOT_TRAVERSAL_H
#define LOCATE_PIVOT_TRAVERSAL_H

#include "traversal.h"

static BMTraversal locate_pivot_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		LocatePivotData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	dot_identifier_CB, identifier_CB, character_CB, mod_character_CB,
	star_character_CB, register_variable_CB, dereference_CB, sub_expression_CB,
	dot_expression_CB, open_CB, filter_CB, decouple_CB, close_CB;

#define BMDotIdentifierCB	dot_identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMCharacterCB		character_CB
#define BMModCharacterCB	mod_character_CB
#define BMStarCharacterCB	star_character_CB
#define BMRegisterVariableCB	register_variable_CB
#define BMDereferenceCB		dereference_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMOpenCB		open_CB
#define BMFilterCB		filter_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB

#include "traversal_template.h"


#endif	// LOCATE_PIVOT_TRAVERSAL_H
