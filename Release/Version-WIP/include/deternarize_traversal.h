#ifndef DETERNARIZE_TRAVERSAL_H
#define DETERNARIZE_TRAVERSAL_H

static BMTraverseCB
	open_CB, ternary_operator_CB, filter_CB, close_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		DeternarizeData *data = traverse_data->user_data; char *p = *q;


#endif	// DETERNARIZE_TRAVERSAL_H
