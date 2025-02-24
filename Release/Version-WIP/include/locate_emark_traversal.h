#ifndef LOCATE_EMARK_TRAVERSAL_H
#define LOCATE_EMARK_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal locate_emark_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	LocateEMarkData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	emark_CB, prune_CB, open_CB, filter_CB, comma_CB, close_CB, end_CB;

#define BMEMarkCB		emark_CB
#define BMNotCB			prune_CB
#define BMDereferenceCB		prune_CB
#define BMDotIdentifierCB	prune_CB
#define BMDotExpressionCB	prune_CB
#define BMSubExpressionCB	prune_CB
#define BMFilterCB		prune_CB
#define BMOpenCB		open_CB
#define BMCommaCB		comma_CB
#define BMCloseCB		close_CB
#define BMEEnovEndCB		end_CB

#include "traversal.h"


#endif	// LOCATE_EMARK_TRAVERSAL_H
