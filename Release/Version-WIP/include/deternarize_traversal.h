#ifndef DETERNARIZE_TRAVERSAL_H
#define DETERNARIZE_TRAVERSAL_H

#include "user_traversal.h"
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	DeternarizeData *data = traversal->user_data; char *p = *q;

static BMTraversal deternarize_traversal;
static BMTraverseCB
	open_CB, ternary_operator_CB, filter_CB, close_CB;

#define BMOpenCB 		open_CB
#define BMTernaryOperatorCB	ternary_operator_CB
#define BMFilterCB		filter_CB
#define BMCloseCB		close_CB

#include "traversal.h"


#endif	// DETERNARIZE_TRAVERSAL_H
