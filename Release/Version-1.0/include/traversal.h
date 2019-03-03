#ifndef TRAVERSAL_H
#define TRAVERSAL_H

#include "list.h"
#include "btree.h"
#include "database.h"

//===========================================================================
//	DB traversal
//===========================================================================
enum {
	DB_CONTINUE,
	DB_DONE
};
typedef int DBTraverseCB( CNInstance *, CNDB *, void * );

int db_traverse( char *expression, CNDB *, DBTraverseCB, void * );
int db_is_empty( CNDB * );
CNInstance *db_first( CNDB *, listItem ** );
CNInstance *db_next( CNDB *, CNInstance *, listItem ** );

enum {
	SUB_NONE,
	SUB_START,
	SUB_FINISH
};
typedef struct {
	struct {
		listItem *exponent;
		listItem *couple;
		listItem *level;
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
	listItem *level;
	int couple;
} VerifyData;
int xp_verify( int privy, CNInstance *x, char *expression, CNDB *, VerifyData * );
int xp_match( int privy, CNInstance *x, char *p, CNInstance *star,
	listItem *exponent, listItem *base, CNDB * );

//===========================================================================
//	utilities
//===========================================================================
#define	AS_SUB	0
#define	SUB	2

void	push_exponent( int as_sub, int position, listItem ** );
void	output_exponent( FILE *stream, listItem * );


#endif	// TRAVERSAL_H
