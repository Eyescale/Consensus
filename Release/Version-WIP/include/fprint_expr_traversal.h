#ifndef FPRINT_EXPR_TRAVERSAL_H
#define FPRINT_EXPR_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal fprint_expr_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	fprintExprData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	bgn_pipe_CB, bgn_set_CB, end_set_CB, comma_CB, filter_CB, format_CB,
	open_CB, close_CB;

#define BMBgnPipeCB		bgn_pipe_CB
#define BMBgnSetCB		bgn_set_CB
#define BMEndSetCB		end_set_CB
#define BMCommaCB		comma_CB
#define BMFilterCB		filter_CB
#define BMLoopCB		filter_CB
#define BMOpenCB		open_CB
#define BMCloseCB		close_CB

#include "traversal.h"


#endif	// FPRINT_EXPR_TRAVERSAL_H
