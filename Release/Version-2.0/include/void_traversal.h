#ifndef VOID_TRAVERSAL_H
#define VOID_TRAVERSAL_H

#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMContext *ctx = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	term_CB, dereference_CB, register_variable_CB;

#define BMTermCB		term_CB
#define BMNotCB			dereference_CB
#define BMDereferenceCB		dereference_CB
#define BMSubExpressionCB	dereference_CB
#define BMRegisterVariableCB	register_variable_CB

#include "traversal.h"

static BMTraversal void_traversal;


#endif	// VOID_TRAVERSAL_H
