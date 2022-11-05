#ifndef VOID_TRAVERSAL_H
#define VOID_TRAVERSAL_H

#define case_( CB ) \
	} static BMCBTake CB( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMContext *ctx = traverse_data->user_data; char *p = *q;

static BMTraverseCB
	feel_CB, sound_CB, touch_CB;

#define BMTermCB		feel_CB
#define BMNotCB			sound_CB
#define BMDereferenceCB		sound_CB
#define BMSubExpressionCB	sound_CB
#define BMRegisterVariableCB	touch_CB

#include "traversal.h"

static BMTraversal void_traversal;


#endif	// VOID_TRAVERSAL_H
