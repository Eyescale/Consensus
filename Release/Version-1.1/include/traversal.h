#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "context.h"

//===========================================================================
//	expression traversal
//===========================================================================
typedef enum {
        BM_CONDITION,
        BM_INSTANTIATED,
        BM_RELEASED
} BMLogType;
int bm_feel( char *expression, BMContext *, BMLogType );

enum {
	BM_CONTINUE,
	BM_DONE
};
typedef int BMTraverseCB( CNInstance *, BMContext *, void * );
int bm_traverse( char *expression, BMContext *, BMTraverseCB, void * );


#endif	// TRAVERSAL_H
