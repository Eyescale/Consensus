#ifndef QUERY_H
#define QUERY_H

#include "context.h"
#include "traverse.h"

typedef enum {
        BM_CONDITION = 0,
        BM_RELEASED,
        BM_INSTANTIATED
} BMQueryType;

typedef BMCBTake BMQueryCB( CNInstance *, BMContext *, void * );
CNInstance *bm_query( BMQueryType, char *expression, BMContext *, BMQueryCB, void * );

#endif	// QUERY_H
