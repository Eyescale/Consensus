#ifndef EXPRESSION_UTIL_H
#define EXPRESSION_UTIL_H

/*---------------------------------------------------------------------------
	expression utilities	- private
---------------------------------------------------------------------------*/

int	trace_mark( Expression *expression );
int	mark_negated( Expression *expression, int negated );
void	invert_results( ExpressionSub *sub, int as_sub, listItem *results );
listItem *take_all( Expression *expression, int as_sub, listItem *results );
void	take_sub_results( ExpressionSub *sub, int as_sub );
void	extract_sub_results( ExpressionSub *sub, ExpressionSub *sub3, int as_sub, listItem *results );
int	test_as_sub( Entity *e, int as_sub );
int	instantiable( Expression *e, _context *context );

#endif	// EXPRESSION_UTIL_H
