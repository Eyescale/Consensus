#ifndef ACTUALIZE_TRAVERSAL_H
#define ACTUALIZE_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal actualize_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	ActualizeData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	sub_expression_CB, dot_expression_CB, dot_identifier_CB,
	open_CB, filter_CB, comma_CB, wildcard_CB, close_CB;

#define BMNotCB			sub_expression_CB
#define BMDereferenceCB		sub_expression_CB
#define BMSubExpressionCB	sub_expression_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMOpenCB		open_CB
#define BMFilterCB		filter_CB
#define BMCommaCB		comma_CB
#define BMWildCardCB		wildcard_CB
#define BMCloseCB		close_CB

#include "traversal.h"

#ifdef DEBUG
#define DBG_ACTUALIZE_TRAVERSAL \
	if (( data.stack.flags )||( data.stack.level )) \
		fprintf( stderr, "Error: context_actualize: Memory Leak\n" ); \
	freeListItem( &data.stack.flags ); \
	freeListItem( &data.stack.level );
#else
#define DBG_ACTUALIZE_TRAVERSAL
#endif		

#endif	// ACTUALIZE_TRAVERSAL_H

