#ifndef DEPARAMETERIZE_TRAVERSAL_H
#define DEPARAMETERIZE_TRAVERSAL_H

static BMTraversal deparameterize_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		DeparameterizeData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	open_CB, close_CB, decouple_CB, filter_CB, wildcard_CB, dot_identifier_CB;

#define BMOpenCB 		open_CB
#define BMCloseCB		close_CB
#define BMFilterCB		filter_CB
#define BMDecoupleCB		decouple_CB
#define BMWildCardCB		wildcard_CB
#define BMDotIdentifierCB	dot_identifier_CB

#include "traversal.h"


#endif	// DEPARAMETERIZE_TRAVERSAL_H
