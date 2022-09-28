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
	BMContext *ctx;
	int privy;
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
		listItem *scope;
		listItem *exponent;
		listItem *base;
	} stack;
} BMQueryData;


#endif	// QUERY_PRIVATE_H
