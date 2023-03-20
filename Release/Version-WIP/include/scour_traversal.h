#ifndef SCOUR_TRAVERSAL_H
#define SCOUR_TRAVERSAL_H

static BMTraversal scour_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		ScourData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	dot_expr_CB, identifier_CB, star_character_CB,
	mod_character_CB, character_CB, register_variable_CB;

#define BMDotIdentifierCB	dot_expr_CB
#define BMDotExpressionCB	dot_expr_CB
#define BMIdentifierCB		identifier_CB
#define BMCharacterCB		character_CB
#define BMModCharacterCB	mod_character_CB
#define BMStarCharacterCB	star_character_CB
#define BMDereferenceCB		star_character_CB
#define BMRegisterVariableCB	register_variable_CB

#include "traversal.h"


#endif // SCOUR_TRAVERSAL_H
