#ifndef QUERY_H
#define QUERY_H

#include "context.h"

typedef enum {
        BM_CONDITION = 0,
        BM_RELEASED,
        BM_INSTANTIATED
} BMQueryType;

typedef enum {
	BM_DONE = 0,
	BM_CONTINUE
} BMCB_;

typedef BMCB_ BMQueryCB( CNInstance *, BMContext *, void * );
CNInstance *bm_query( BMQueryType, char *expression, BMContext *, BMQueryCB, void * );

#endif	// QUERY_H
