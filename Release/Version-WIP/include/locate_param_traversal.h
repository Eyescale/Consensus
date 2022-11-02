#ifndef LOCATE_PARAM_TRAVERSAL_H
#define LOCATE_PARAM_TRAVERSAL_H

static BMTraverseCB
	not_CB, deref_CB, sub_expr_CB, dot_push_CB, push_CB, sift_CB, sep_CB,
	pop_CB, wildcard_CB, parameter_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMLocateParamData *data = traverse_data->user_data; char *p = *q;


#endif	// LOCATE_PARAM_TRAVERSAL_H

