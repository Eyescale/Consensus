#ifndef EXPRESSION_SOLVE_H
#define EXPRESSION_SOLVE_H

/*---------------------------------------------------------------------------
	expression_solve utilities - shared with expression_sieve()
---------------------------------------------------------------------------*/

enum {
	IdentifierValueType = 1,
	EntityValueType,
	EntitiesValueType,
	LiteralsValueType,
	StringsValueType,
	ExpressionsValueType,
	SubValueType
};

int sub_type( Expression *, int count, void **value, _context * );
int set_active2( Expression *expression, int *not );
int set_as_sub2( Expression *expression, int as_sub );


#endif	// EXPRESSION_SOLVE_H
