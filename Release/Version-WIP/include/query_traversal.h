#ifndef QUERY_TRAVERSAL_H
#define QUERY_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal query_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
	BMQueryData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	filter_CB, match_CB, dot_identifier_CB, dereference_CB, dot_expression_CB,
	sub_expression_CB, open_CB, decouple_CB, close_CB, wildcard_CB;

#define BMBgnPipeCB		filter_CB
#define BMFilterCB		filter_CB
#define BMRegisterVariableCB	match_CB
#define BMStarCharacterCB	match_CB
#define BMModCharacterCB	match_CB
#define BMCharacterCB		match_CB
#define BMRegexCB		match_CB
#define BMIdentifierCB		match_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMDereferenceCB		dereference_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMWildCardCB		wildcard_CB

#include "traversal.h"


#endif	// QUERY_TRAVERSAL_H
