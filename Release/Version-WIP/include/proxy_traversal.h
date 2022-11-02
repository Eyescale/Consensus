#ifndef PROXY_TRAVERSAL_H
#define PROXY_TRAVERSAL_H

static BMTraverseCB
	term_CB, verify_CB, open_CB, decouple_CB, close_CB, identifier_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMProxyFeelData *data = traverse_data->user_data; char *p = *q;


#endif	// PROXY_TRAVERSAL_H
