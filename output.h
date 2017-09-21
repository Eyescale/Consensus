#ifndef OUTPUT_H
#define OUTPUT_H

typedef enum {
	ExpressionAll,
	ExpressionMark,
	SubAll,
	SubFlags,
	SubAny,
	SubVariable,
	SubIdentifier,
	SubNull,
	SubSub
}
ExpressionOutput;

/*---------------------------------------------------------------------------
	output actions		- public
---------------------------------------------------------------------------*/

_action output_results;
_action output_narrative;
_action output_variable_value;
_action output_variator_value;
_action output_va;
_action output_hcn;
_action output_mod;
_action output_char;

/*---------------------------------------------------------------------------
	output utilities	- public
---------------------------------------------------------------------------*/

int	output( OutputType type, const char *format, ... );
void	output_name( Entity *e, Expression *format, int base );
int	output_expression( ExpressionOutput component, Expression *expression, int i, int shorty );
void	prompt( _context *context );
void	narrative_output( Narrative *narrative, _context *context );



#endif	// OUTPUT_H
