#ifndef QUERY_H
#define QUERY_H

#include "context.h"
#include "traverse.h"

#define BM_EVENT	1
#define BM_VISIBLE	2
#define BM_AS_PER	4

#define	BM_RELEASED	BM_EVENT
#define	BM_CONDITION	BM_VISIBLE
#define	BM_INSTANTIATED (BM_EVENT|BM_VISIBLE)

typedef BMCBTake BMQueryCB( CNInstance *, BMContext *, void * );
CNInstance *bm_query( int, char *expression, BMContext *, BMQueryCB, void * );

typedef struct {
	int type;
	BMQueryCB *user_CB;
	void *user_data;
	BMContext *ctx;
	CNDB *db;
	Pair *pivot;
	int privy;
	listItem *exponent;
	int op, success;
	int list_exp;
	Pair *list;
	CNInstance *star;
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

int xp_verify( CNInstance *, char *, BMQueryData * );


#endif	// QUERY_H
