#ifndef LOCATE_PARAM_TRAVERSAL_H
#define LOCATE_PARAM_TRAVERSAL_H

#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		LocateParamData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	not_CB, dereference_CB, sub_expression_CB, dot_expression_CB, dot_identifier_CB,
	open_CB, filter_CB, decouple_CB, close_CB, wildcard_CB;

#define BMNotCB			not_CB
#define BMDereferenceCB		dereference_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMOpenCB		open_CB
#define BMFilterCB		filter_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMWildCardCB		wildcard_CB

#include "traversal.h"

static BMTraversal locate_param_traversal;


#endif	// LOCATE_PARAM_TRAVERSAL_H

