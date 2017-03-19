#ifndef FILTER_UTIL_H
#define FILTER_UTIL_H

/*---------------------------------------------------------------------------
	filter utilities	- private to expression_solve
---------------------------------------------------------------------------*/

int	set_filter_variable( _context *context );
int	rebuild_filtered_results( listItem **results, _context *context );
int	free_filter_results( Expression *expression, _context *context );
int	remove_filter( Expression *expression, int mode, int as_sub, _context *context );

#endif	// FILTER_UTIL_H
