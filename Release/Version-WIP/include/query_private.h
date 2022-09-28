#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

#include "database.h"
#include "context.h"
#include "query.h"

//===========================================================================
//	Interface with bm_verify
//===========================================================================
typedef enum {
	BM_INIT,
	BM_BGN,
	BM_END
} BMVerifyOp;

typedef struct {
	char *expression;
	BMContext *ctx;
	int privy;
	CNDB *db;
	int empty;
	CNInstance *star;
	Pair *pivot;
	listItem *exponent;
	BMQueryCB *user_CB;
	void *user_data;
	listItem *mark_exp;
	listItem *sub_exp;
	int success;
	int flags;
	struct {
		listItem *flags;
		listItem *exponent;
		listItem *scope;
		listItem *base;
	} stack;
} BMQueryData;


#endif	// QUERY_PRIVATE_H
