#ifndef DEPARAMETERIZE_TRAVERSAL_H
#define DEPARAMETERIZE_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal deparameterize_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	DeparameterizeData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	sub_expression_CB, dot_identifier_CB,
	open_CB, filter_CB, comma_CB, wildcard_CB, close_CB;

#define BMNotCB			sub_expression_CB
#define BMDereferenceCB		sub_expression_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMOpenCB 		open_CB
#define BMFilterCB		filter_CB
#define BMCommaCB		comma_CB
#define BMWildCardCB		wildcard_CB
#define BMCloseCB		close_CB

#include "traversal.h"


#endif	// DEPARAMETERIZE_TRAVERSAL_H
