#ifndef EXPRESSION_UTIL_H
#define EXPRESSION_UTIL_H

/*---------------------------------------------------------------------------
	expression utilities	- most of them private to expression.c
---------------------------------------------------------------------------*/

Expression *newExpression( _context *context );
void	freeExpression( Expression *expression );
int	flagged( Expression *expression, int count );
int	just_any( Expression *expression, int count );
int	just_blank( Expression *expression );
void	freeExpressions( listItem **list );
void	freeLiteral( Literal *l );
void	freeLiterals( listItem **list );
int	trace_mark( Expression *expression );
int	mark_negated( Expression *expression, int negated );
void	expression_collapse( Expression *expression );

int	expression_solve( Expression *expression, int as_sub, listItem *filter, _context *context );

#endif	// EXPRESSION_UTIL_H
