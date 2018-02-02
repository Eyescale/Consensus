#ifndef OUTPUT_UTIL_H
#define OUTPUT_UTIL_H

/*---------------------------------------------------------------------------
	output utilities	- private
---------------------------------------------------------------------------*/

void	output_identifier( char *identifier );
int	output_collection( void *i, ValueType type, void *format, int inb );
int	output_expression( ExpressionOutput part, Expression *expression, ... );
void	output_entity( Entity *super, int sub, Expression *format );
void	output_literal( Literal *l );
void	output_narrative( Narrative *narrative, _context *context );

int	output_variable_value( VariableVA *variable );
int	output_entities_va( char *va_name, _context *context );
int	output_collection( void *value, ValueType type, void *format, int inb );

void	printtab( int level );


#endif	// OUTPUT_UTIL_H
