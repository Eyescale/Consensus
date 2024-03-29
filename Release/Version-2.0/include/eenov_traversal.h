#ifndef EENOV_TRAVERSAL_H
#define EENOV_TRAVERSAL_H

static BMTraversal eenov_traversal;
#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		EEnovData *data = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	identifier_CB, open_CB, decouple_CB, close_CB, wildcard_CB, end_CB;

#define BMStarCharacterCB	identifier_CB
#define BMModCharacterCB	identifier_CB
#define BMCharacterCB		identifier_CB
#define BMRegexCB		identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMWildCardCB		wildcard_CB
#define BMCloseCB		close_CB
#define BMEEnovEndCB		end_CB

#include "traversal.h"


#endif	// EENOV_TRAVERSAL_H
