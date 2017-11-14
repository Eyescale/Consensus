#ifndef EXPRESSION_UTIL_H
#define EXPRESSION_UTIL_H

/*---------------------------------------------------------------------------
	expression utilities	- private
---------------------------------------------------------------------------*/

int	just_any( Expression *expression, int count );
int	just_blank( Expression *expression );
Expression *makeLiteral( Expression *expression );
void	freeLiteral( listItem **list );
int	translateFromLiteral( listItem **list, int as_sub, _context *context );
void	freeExpression( Expression *expression );
void	expression_collapse( Expression *expression );
int	expression_solve( Expression *expression, int as_sub, _context *context );

int	trace_mark( Expression *expression );
int	mark_negated( Expression *expression, int negated );
void	invert_results( ExpressionSub *sub, int as_sub, listItem *results );
listItem *take_all( Expression *expression, int as_sub, listItem *results );
void	take_sup( Expression *expression );
void	take_sub_results( ExpressionSub *sub, int as_sub );
void	extract_sub_results( ExpressionSub *sub, ExpressionSub *sub3, int as_sub, listItem *results );
int	test_as_sub( void *e, int read_mode, int as_sub );
int	instantiable( Expression *e, _context *context );

#endif	// EXPRESSION_UTIL_H
