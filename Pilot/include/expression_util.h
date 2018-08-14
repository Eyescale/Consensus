#ifndef EXPRESSION_UTIL_H
#define EXPRESSION_UTIL_H

/*---------------------------------------------------------------------------
	expression utilities	- most of them private to expression.c
---------------------------------------------------------------------------*/

Expression *newExpression( _context * );
void	freeExpression( Expression * );
int	just_mark( Expression * );
int	just_blank( Expression * );
int	just_any( Expression *, int count );
int	flagged( Expression *, int count );
void	freeExpressions( listItem ** );
void	freeLiteral( Literal * );
void	freeLiterals( listItem **list );
int	trace_mark( Expression * );
int	mark_negated( Expression *, int negated );
void	expression_collapse( Expression * );

int	expression_solve( Expression *, int as_sub, listItem *filter, _context * );

#endif	// EXPRESSION_UTIL_H
