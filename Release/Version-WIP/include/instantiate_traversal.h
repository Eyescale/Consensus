#ifndef INSTANTIATE_TRAVERSAL_H
#define	INSTANTIATE_TRAVERSAL_H

static BMTraverseCB
	term_CB, collect_CB, bgn_set_CB, end_set_CB, bgn_pipe_CB, end_pipe_CB,
	open_CB, close_CB, decouple_CB, register_variable_CB, literal_CB, list_CB,
	wildcard_CB, dot_identifier_CB, identifier_CB, signal_CB, end_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMInstantiateData *data = traverse_data->user_data; char *p = *q;


#endif	// INSTANTIATE_TRAVERSAL_H
