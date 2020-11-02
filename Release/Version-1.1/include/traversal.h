#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "list.h"
#include "btree.h"
#include "database.h"

//===========================================================================
//	DB traversal
//===========================================================================
typedef enum {
        DB_CONDITION,
        DB_INSTANTIATED,
        DB_RELEASED
} DBLogType;
int db_feel( char *expression, CNDB *, DBLogType );

enum {
	DB_CONTINUE,
	DB_DONE
};
typedef int DBTraverseCB( CNInstance *, CNDB *, void * );
int db_traverse( char *expression, CNDB *, DBTraverseCB, void * );

// utility
void xpn_out( FILE *stream, listItem *xp );

//===========================================================================
//	Interface with bm_verify
//===========================================================================
enum {
	INIT,
	SUB_BGN,
	SUB_END
};
typedef struct {
	int op, privy, success;
	CNDB *db;
	int empty;
	CNInstance *star;
	int couple;
	Pair *pivot;
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
#ifdef TRIM
	BTreeNode *btree;
#endif
} VerifyData;


#endif	// TRAVERSAL_H
