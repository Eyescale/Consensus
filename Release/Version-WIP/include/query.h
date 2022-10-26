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
	CNDB *db_x;
	BMQueryType type;
	BMQueryCB *user_CB;
	void *user_data;
	BMContext *ctx;
	Pair *pivot;
	int privy;
	CNInstance *star;
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

typedef int XPTraverseCB( CNInstance *, char *, BMQueryData * );
CNInstance *xp_traverse( char *, BMQueryData *, XPTraverseCB );
int xp_verify( CNInstance *, char *, BMQueryData * );

#endif	// QUERY_H
