#ifndef VARIABLE_UTIL_H
#define VARIABLE_UTIL_H

/*---------------------------------------------------------------------------
	variable utilities	- public
---------------------------------------------------------------------------*/

VariableVA *newVariable( Registry *registry, char *identifier );
VariableVA *fetchVariable( _context *context, char *identifier, int do_create );
VariableVA *lookupVariable( _context *context, char *identifier );
void	resetVariable( char *identifier, _context *context );
void	resetVariables( _context *context );
void	transferVariables( Registry *dst, Registry *src, int override );
void	freeVariables( Registry *variables );
void	freeVariableValue( VariableVA *variable );
void	setVariableValue( VariableVA *, void *value, ValueType type, _context * );
void	addVariableValue( VariableVA *, void *value, ValueType type, _context * );
void *  getVariableValue( VariableVA *, _context *context, int duplicate );


#endif	// VARIABLE_UTIL_H
