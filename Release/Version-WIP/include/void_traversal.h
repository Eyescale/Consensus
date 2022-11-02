#ifndef VOID_TRAVERSAL_H
#define VOID_TRAVERSAL_H

static BMTraverseCB
	feel_CB, sound_CB, touch_CB;
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMContext *ctx = traverse_data->user_data; char *p = *q;


#endif	// VOID_TRAVERSAL_H
