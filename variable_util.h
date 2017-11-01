#ifndef VARIABLE_UTIL_H
#define VARIABLE_UTIL_H

/*---------------------------------------------------------------------------
	variables utilities	- public
---------------------------------------------------------------------------*/

VariableVA *newVariable( Registry *registry, char *identifier );
void freeVariableValue( VariableVA *variable );
void freeVariables( Registry *variables );
Registry *variable_registry( _context *context );
VariableVA *lookupVariable( _context *context, char *identifier, int up );
VariableVA *fetchVariable( _context *context, char *identifier, int do_create );

int expression_substitute( Expression *expression, char *identifier, listItem *value, int count );
int count_occurrences( Expression *expression, char *identifier, int count );


#endif	// VARIABLE_UTIL_H
