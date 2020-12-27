#ifndef TRAVERSAL_PRIVATE_H
#define TRAVERSAL_PRIVATE_H

#include "btree.h"
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
	BMContext *ctx;
	int privy;
	char *expression;
	int empty;
	CNInstance *star;
	Pair *pivot;
	listItem *exponent;
	BMTraverseCB *user_CB;
	void *user_data;
	listItem *mark_exp;
	listItem *sub_exp;
	int success;
	int couple;
	int not;
	struct {
		listItem *exponent;
		listItem *couple;
		listItem *scope;
		listItem *base;
		listItem *not;
	} stack;
} BMTraverseData;


#endif	// TRAVERSAL_PRIVATE_H
