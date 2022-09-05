#ifndef TRAVERSAL_PRIVATE_H
#define TRAVERSAL_PRIVATE_H

#include "database.h"
#include "context.h"

//===========================================================================
//	Interface with bm_verify
//===========================================================================
enum {
	BM_INIT,
	BM_BGN,
	BM_END
};
typedef struct {
	char *expression;
	BMContext *ctx;
	int privy;
	CNDB *db;
	int empty;
	CNInstance *star;
	Pair *pivot;
	listItem *exponent;
	BMTraverseCB *user_CB;
	void *user_data;
	listItem *mark_exp;
	listItem *sub_exp;
	int success;
#if 1
	int flags;
#else
	int couple, not;
#endif
	struct {
		listItem *flags;
		listItem *exponent;
		listItem *scope;
		listItem *base;
	} stack;
} BMTraverseData;


#endif	// TRAVERSAL_PRIVATE_H
