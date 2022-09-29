#ifndef QUERY_PRIVATE_H
#define QUERY_PRIVATE_H

#include "context.h"
#include "query.h"

//===========================================================================
//	Interface with xp_verify and verify
//===========================================================================
typedef enum {
	BM_INIT,
	BM_BGN,
	BM_END
} BMVerifyOp;

typedef struct {
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
	listItem *sub_exp;
	listItem *base;
	listItem *OOS;
	int flags;
	struct {
		listItem *flags;
		listItem *scope;
		listItem *exponent;
		listItem *base;
	} stack;
} BMQueryData;

#define case_( func ) \
	} static BMCB_take func( BMTraverseData *traverse_data, char *p, int flags ) { \
		BMQueryData *data = traverse_data->user_data;


#endif	// QUERY_PRIVATE_H
