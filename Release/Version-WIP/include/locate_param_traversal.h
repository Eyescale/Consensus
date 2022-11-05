#ifndef LOCATE_PARAM_TRAVERSAL_H
#define LOCATE_PARAM_TRAVERSAL_H

#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		LocateParamData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	not_CB, deref_CB, sub_expr_CB, dot_push_CB, push_CB, sift_CB, sep_CB,
	pop_CB, wildcard_CB, parameter_CB;

#define BMNotCB			not_CB
#define BMDereferenceCB		deref_CB
#define BMSubExpressionCB	sub_expr_CB
#define BMDotExpressionCB	dot_push_CB
#define BMOpenCB		push_CB
#define BMFilterCB		sift_CB
#define BMDecoupleCB		sep_CB
#define BMCloseCB		pop_CB
#define BMWildCardCB		wildcard_CB
#define BMDotIdentifierCB	parameter_CB

#include "traversal.h"

static BMTraversal locate_param_traversal;


#endif	// LOCATE_PARAM_TRAVERSAL_H

