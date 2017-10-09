#ifndef OUTPUT_H
#define OUTPUT_H

typedef enum {
	ExpressionAll,
	ExpressionSup,
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
_action output_special_char;

/*---------------------------------------------------------------------------
	output utilities	- public
---------------------------------------------------------------------------*/

int	output( OutputContentsType type, const char *format, ... );
int	push_output( char *identifier, void *dst, OutputType type, _context *context );
int	pop_output( _context *context );
void	output_identifier( char *identifier, int as_is );
void	output_entity_name( Entity *e, Expression *format, int base );
void	output_narrative_name( Entity *e, char *name );
int	output_expression( ExpressionOutput component, Expression *expression, int i, int shorty );
void	prompt( _context *context );
void	narrative_output( Narrative *narrative, _context *context );



#endif	// OUTPUT_H
