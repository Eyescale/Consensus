#ifndef XL_H
#define XL_H

/*---------------------------------------------------------------------------
	conversion utilities
---------------------------------------------------------------------------*/

Literal	*	l_sub( Literal *, int count );
Entity	*	l_inst( Literal * );
Expression *	s2x( char *, _context * );
Entity	*	l2e( Literal * );
int		lcmp( Literal *, Literal * );
Literal *	ldup( Literal * );
Literal *	e2l( Entity * );
int		literalisable( Expression * );
int		xlisable( Expression * );
listItem *	x2s( Expression *, _context *, int expand );
listItem *	x2l( Expression *, _context * );
Literal *	xl( Expression * );

int		E2L( listItem **entities, listItem *filter, _context *context );
int		L2E( listItem **literals, listItem *filter, _context *context );
listItem *	X2L( listItem *expressions, _context *context );
listItem *	Xt2L( listItem *expressions, _context *context );
listItem *	S2X( listItem *strings, int *type, _context *context );


#endif	// XL_H
