#ifndef EENO_QUERY_TRAVERSAL_H
#define EENO_QUERY_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal eeno_query_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
	EENOFeelData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	filter_CB, verify_CB, open_CB, decouple_CB, close_CB, match_CB, wildcard_CB;

#define BMFilterCB		filter_CB
#define BMNotCB			verify_CB
#define BMDereferenceCB		verify_CB
#define BMSubExpressionCB	verify_CB
#define BMDotExpressionCB	verify_CB
#define BMDotIdentifierCB	verify_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMRegisterVariableCB	match_CB
#define BMModCharacterCB	match_CB
#define BMStarCharacterCB	match_CB
#define BMRegexCB		match_CB
#define BMCharacterCB		match_CB
#define BMIdentifierCB		match_CB
#define BMWildCardCB		wildcard_CB

#include "traversal.h"


#endif	// EENO_QUERY_TRAVERSAL_H
