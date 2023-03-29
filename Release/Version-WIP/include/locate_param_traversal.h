#ifndef LOCATE_PARAM_TRAVERSAL_H
#define LOCATE_PARAM_TRAVERSAL_H

static BMTraversal locate_param_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		LocateParamData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	dereference_CB, sub_expression_CB, dot_expression_CB, dot_identifier_CB,
	open_CB, filter_CB, decouple_CB, close_CB;

#define BMNotCB			sub_expression_CB
#define BMDereferenceCB		sub_expression_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMOpenCB		open_CB
#define BMFilterCB		filter_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB

#include "traversal.h"


#endif	// LOCATE_PARAM_TRAVERSAL_H

