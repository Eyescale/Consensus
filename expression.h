#ifndef EXPRESSION_H
#define EXPRESSION_H

/*---------------------------------------------------------------------------
	expression actions	- public
---------------------------------------------------------------------------*/

_action parse_expression;

/*---------------------------------------------------------------------------
	expression utilities	- public
---------------------------------------------------------------------------*/

int	just_any( Expression *expression, int count );
void	freeLiteral( listItem **list );
int	translateFromLiteral( listItem **list, int as_sub, _context *context );
void	freeExpression( Expression *expression );
void	expression_collapse( Expression *expression );
int	expression_solve( Expression *expression, int as_sub, _context *context );

#endif	// EXPRESSION_H
