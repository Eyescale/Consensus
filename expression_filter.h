#ifndef EXPRESSION_FILTER_H
#define EXPRESSION_FILTER_H

/*---------------------------------------------------------------------------
	filter utilities	- private to expression_solve
---------------------------------------------------------------------------*/

ExpressionMode set_filter( Expression *expression, _context *context );
int	remove_filter( Expression *expression, int mode, int as_sub, _context *context );
int	rebuild_filtered_results( listItem **results, _context *context );
void	free_filter_results( Expression *expression, _context *context );

#endif	// EXPRESSION_FILTER_H
