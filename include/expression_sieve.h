#ifndef EXPRESSION_SIEVE_H
#define EXPRESSION_SIEVE_H

/*---------------------------------------------------------------------------
	expression_sieve utilities - invoked by expression_solve()
---------------------------------------------------------------------------*/

int sieve( Expression *expression, int as_sub, listItem *filter, _context *context );
int sub_sieve( Expression *, int count, int as_sub, listItem *filter, _context * );
listItem *l_take_sup( Expression *, int as_sup );


#endif	// EXPRESSION_SIEVE_H
