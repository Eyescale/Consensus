#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "list.h"
#include "database.h"
#include "traversal.h"
#include "wildcard.h"

int	db_verify( int privy, CNInstance *, char *expression, CNDB * );
int     db_verify_sub( int op, int success, CNInstance **, char **position,
        listItem **mark_exp, listItem **mark_pos, VerifyData * );
void	db_substantiate( char *expression, CNDB * );
listItem * db_fetch( char *expression, CNDB * );
void	db_release( char *expression, CNDB * );
void	db_outputf( char *format, char *expression, CNDB * );
void	db_output( char *expression, CNDB * );

char *	p_locate( char *expression, char *fmt, listItem **exponent );
CNInstance *p_lookup( int privy, char *position, CNDB *db );


#endif	// EXPRESSION_H
