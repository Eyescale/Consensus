#ifndef DEPARAMETERIZE_TRAVERSAL_H
#define DEPARAMETERIZE_TRAVERSAL_H

#include "traversal.h"

static BMTraversal deparameterize_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
	DeparameterizeData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	sub_expression_CB, dot_identifier_CB,
	open_CB, filter_CB, decouple_CB, wildcard_CB, close_CB;

#define BMNotCB			sub_expression_CB
#define BMDereferenceCB		sub_expression_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMOpenCB 		open_CB
#define BMFilterCB		filter_CB
#define BMDecoupleCB		decouple_CB
#define BMWildCardCB		wildcard_CB
#define BMCloseCB		close_CB

#include "traversal_template.h"


#endif	// DEPARAMETERIZE_TRAVERSAL_H
