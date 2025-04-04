#ifndef EENOV_TRAVERSAL_H
#define EENOV_TRAVERSAL_H

#include "user_traversal.h"

static BMTraversal eenov_traversal;
#define case_( CB ) \
} static BMCBTake CB( BMTraverseData *traversal, char **q, int flags, int f_next ) { \
	EEnovData *data = traversal->user_data; char *p = *q;

static BMTraverseCB
	identifier_CB, open_CB, comma_CB, close_CB, wildcard_CB, end_CB;

#define BMStarCharacterCB	identifier_CB
#define BMModCharacterCB	identifier_CB
#define BMCharacterCB		identifier_CB
#define BMRegexCB		identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMOpenCB		open_CB
#define BMCommaCB		comma_CB
#define BMWildCardCB		wildcard_CB
#define BMCloseCB		close_CB
#define BMEEnovEndCB		end_CB

#include "traversal.h"


#endif	// EENOV_TRAVERSAL_H
