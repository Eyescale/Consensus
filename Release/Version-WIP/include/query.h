#ifndef QUERY_H
#define QUERY_H

#include "context.h"

#define BM_EVENT	1
#define BM_VISIBLE	2
#define BM_AS_PER	4

#define	BM_RELEASED	BM_EVENT
#define	BM_CONDITION	BM_VISIBLE
#define	BM_INSTANTIATED (BM_EVENT|BM_VISIBLE)

typedef enum {
	BMQ_CONTINUE = 0,
	BMQ_DONE
	} BMQTake;
typedef BMQTake \
	BMQueryCB( CNInstance *, BMContext *, void * );

CNInstance *bm_query( int, char *, BMContext *, BMQueryCB, void * );
CNInstance *bm_query_assignee( int type, char *, BMContext * );
CNInstance *bm_query_assigner( char *, BMContext * );

typedef struct {
	BMContext *ctx;
	CNDB *db;
	int type, privy, op, success, list_expr;
	Pair *pivot, *list;
	CNInstance *instance, *star;
	listItem *exponent, *mark_exp, *mark_sel, *base, *OOS;
	struct { listItem *flags, *scope, *exponent, *base; } stack;
	BMQueryCB *user_CB;
	void *user_data;
	} BMQueryData;

int xp_verify( CNInstance *, char *, BMQueryData * );


#endif	// QUERY_H
