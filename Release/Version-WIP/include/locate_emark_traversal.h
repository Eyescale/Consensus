#ifndef LOCATE_EMARK_TRAVERSAL_H
#define LOCATE_EMARK_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal locate_emark_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	LocateEMarkData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	emark_CB, open_CB, comma_CB, close_CB;

#define BMEMarkCB	emark_CB
#define BMOpenCB	open_CB
#define BMCommaCB	comma_CB
#define BMCloseCB	close_CB

#include "traversal.h"


#endif	// LOCATE_EMARK_TRAVERSAL_H
