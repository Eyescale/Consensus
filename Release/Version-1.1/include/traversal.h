#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "list.h"
#include "btree.h"
#include "database.h"
#include "narrative.h"

//===========================================================================
//	expression traversal
//===========================================================================
typedef struct {
	CNDB *db;
	Registry *registry;
}
BMContext;
BMContext *bm_push( CNNarrative *, CNInstance *, CNDB * );
void bm_pop( BMContext * );
void bm_register( BMContext *, char *, CNInstance * );

typedef enum {
        BM_CONDITION,
        BM_INSTANTIATED,
        BM_RELEASED
} BMLogType;
int bm_feel( char *expression, BMContext *, BMLogType );

enum {
	BM_CONTINUE,
	BM_DONE
};
typedef int BMTraverseCB( CNInstance *, BMContext *, void * );
int bm_traverse( char *expression, BMContext *, BMTraverseCB, void * );

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

	int op;
	int couple;
	struct {
		listItem *exponent;
		listItem *couple;
		listItem *scope;
		listItem *base;
		listItem *not;
		listItem *neg;
		listItem *p;
	} stack;
	listItem *mark_exp, *sub_exp;
	int success;

#ifdef TRIM
	BTreeNode *btree;
#endif
} BMTraverseData;


#endif	// TRAVERSAL_H
