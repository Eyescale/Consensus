#ifndef INSTANTIATE_TRAVERSAL_H
#define	INSTANTIATE_TRAVERSAL_H

#include "traversal.h"

static BMTraversal instantiate_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		InstantiateData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	filter_CB, bgn_set_CB, end_set_CB, bgn_pipe_CB, end_pipe_CB, open_CB,
	close_CB, newborn_CB, loop_CB, activate_CB, collect_CB, decouple_CB,
	wildcard_CB, register_variable_CB, literal_CB, list_CB, dot_expression_CB,
	dot_identifier_CB, identifier_CB, signal_CB;

#define BMFilterCB		filter_CB
#define BMBgnSetCB		bgn_set_CB
#define BMEndSetCB		end_set_CB
#define BMBgnPipeCB		bgn_pipe_CB
#define BMEndPipeCB		end_pipe_CB
#define BMOpenCB		open_CB
#define BMCloseCB		close_CB
#define BMNewBornCB		newborn_CB
#define BMLoopCB		loop_CB
#define BMActivateCB		activate_CB
#define BMDereferenceCB		collect_CB
#define BMNotCB			collect_CB
#define BMSubExpressionCB	collect_CB
#define BMDecoupleCB		decouple_CB
#define BMWildCardCB		wildcard_CB
#define BMRegisterVariableCB	register_variable_CB
#define BMLiteralCB		literal_CB
#define BMListCB		list_CB
#define BMDotExpressionCB	dot_expression_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMModCharacterCB	identifier_CB
#define BMStarCharacterCB	identifier_CB
#define BMCharacterCB		identifier_CB
#define BMSignalCB		signal_CB

#include "traversal_template.h"


#endif	// INSTANTIATE_TRAVERSAL_H
