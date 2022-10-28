#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

#include "query.h"

//===========================================================================
//	Interface with xp_verify
//===========================================================================
typedef enum {
	BM_INIT,
	BM_BGN,
	BM_END
} BMVerifyOp;

static BMTraverseCB
	match_CB, dot_identifier_CB, dereference_CB, dot_expression_CB,
	sub_expression_CB, open_CB, filter_CB, decouple_CB, close_CB, wildcard_CB;

#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMQueryData *data = traverse_data->user_data; char *p = *q;


#endif	// QUERY_PRIVATE_H
