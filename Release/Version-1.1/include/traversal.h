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

enum {
	SUB_NONE,
	SUB_START,
	SUB_FINISH
};
typedef struct {
	struct {
		listItem *exponent;
		listItem *couple;
		listItem *scope;
		listItem *base;
		listItem *not;
		listItem *neg;
		listItem *p;
	} stack;
#ifdef TRIM
	BTreeNode *btree;
#endif
	CNDB *db;
	int privy;
	char *pivot;
	int empty;
	CNInstance *star;
	int couple;
} VerifyData;
int xp_match( int privy, CNInstance *x, char *p, CNInstance *star,
	listItem *exponent, listItem *base, CNDB * );

//===========================================================================
//	utilities
//===========================================================================
#define	AS_SUB	0
#define	SUB	2

void	xpn_add( listItem **, int as_sub, int position );
void	xpn_out( FILE *stream, listItem * );


#endif	// TRAVERSAL_H
