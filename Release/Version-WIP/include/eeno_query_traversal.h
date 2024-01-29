#ifndef EENO_QUERY_TRAVERSAL_H
#define EENO_QUERY_TRAVERSAL_H

#include "traversal.h"

static BMTraversal eeno_query_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
	EENOFeelData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	filter_CB, verify_CB, open_CB, decouple_CB, close_CB, identifier_CB, wildcard_CB;

#define BMFilterCB		filter_CB
#define BMNotCB			verify_CB
#define BMDereferenceCB		verify_CB
#define BMSubExpressionCB	verify_CB
#define BMDotExpressionCB	verify_CB
#define BMDotIdentifierCB	verify_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMRegisterVariableCB	identifier_CB
#define BMModCharacterCB	identifier_CB
#define BMStarCharacterCB	identifier_CB
#define BMRegexCB		identifier_CB
#define BMCharacterCB		identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMWildCardCB		wildcard_CB

#include "traversal_template.h"


#endif	// EENO_QUERY_TRAVERSAL_H
