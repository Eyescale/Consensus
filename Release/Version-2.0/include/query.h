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

typedef struct {
	BMQueryType type;
	BMQueryCB *user_CB;
	void *user_data;
	BMContext *ctx;
	CNDB *db;
	Pair *pivot;
	int privy;
	listItem *exponent;
	int op, success;
	CNInstance *instance;
	listItem *mark_exp;
	listItem *base;
	listItem *OOS;
	struct {
		listItem *flags;
		listItem *scope;
		listItem *exponent;
		listItem *base;
	} stack;
} BMQueryData;

int bm_verify( CNInstance *, char *, BMQueryData * );



#endif	// QUERY_H
