#ifndef USER_TRAVERSAL_H
#define USER_TRAVERSAL_H

#include "entity.h"
#include "string_util.h"

#define AS_SUB 		0
#define SUB     	2
#define STAR_SUB	4

//===========================================================================
//	custom user_traversal interface
//===========================================================================
#define is_f_next(b) (f_next&(b))
#define BMTraverseError -1
typedef struct {
	void *user_data;
	listItem **stack;
	int flags, done;
	char *p;
	} BMTraverseData;

typedef enum {
	BMT_CONTINUE = 0,
	BMT_DONE,
	BMT_PRUNE_FILTER,
	BMT_PRUNE_TERM,
	BMT_PRUNE_LEVEL
	} BMCBTake;

typedef char *BMTraversal( char *, BMTraverseData *, int );
typedef BMCBTake BMTraverseCB( BMTraverseData *, char **p, int flags, int f_next );

#endif	// USER_TRAVERSAL_H
