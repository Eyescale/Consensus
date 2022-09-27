#ifndef FEEL_PRIVATE_H
#define FEEL_PRIVATE_H

#include "database.h"
#include "context.h"
#include "feel.h"

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
	BMScanCB *user_CB;
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
} BMScanData;


#endif	// FEEL_PRIVATE_H
