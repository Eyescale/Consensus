#ifndef OUTPUT_UTIL_H
#define OUTPUT_UTIL_H

/*---------------------------------------------------------------------------
	output utilities	- private
---------------------------------------------------------------------------*/

void	output_identifier( char *identifier, int as_is );
int	output_expression( ExpressionOutput component, Expression *expression, int i, int shorty );
void	output_entity_name( Entity *e, Expression *format, int base );
void	output_entity_variable( listItem *i, Expression *format );
void	output_literal_variable( listItem *i, Expression *format );
int	output_variable_value_( VariableVA *variable );
int	output_va_( char *va_name, _context *context );
void	output_narrative_( Narrative *narrative, _context *context );


#endif	// OUTPUT_UTIL_H
