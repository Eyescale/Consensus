#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "list.h"
#include "database.h"
#include "traversal.h"
#include "wildcard.h"

#define AS_SUB	0
#define SUB	2

void	bm_substantiate( char *expression, CNDB * );
listItem * bm_fetch( char *expression, CNDB * );
void	bm_release( char *expression, CNDB * );
void	bm_outputf( char *format, char *expression, CNDB * );
void	bm_output( char *expression, CNDB * );
int     bm_verify( int op, int success, CNInstance **, char **position,
        	listItem **mark_exp, listItem **mark_pos, VerifyData * );
CNInstance * bm_lookup( int privy, char *position, CNDB *db );

typedef enum {
	PRUNE_DEFAULT,
	PRUNE_IDENTIFIER,
	PRUNE_COLON
} PruneType;
char *	p_prune( PruneType type, char *p );
char *	p_locate( char *expression, char *fmt, listItem **exponent );
int	p_filtered( char *position );


#endif	// EXPRESSION_H
