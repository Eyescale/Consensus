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

_action error;
_action warning;
_action output_mod;
_action output_char;
_action output_special_char;
_action output_variable_value;
_action output_variator_value;
_action output_scheme_descriptor;
_action output_scheme_va;
_action output_results;
_action output_va;
_action output_narrative;

/*---------------------------------------------------------------------------
	output utilities	- public
---------------------------------------------------------------------------*/

#define	output( type, string )	outputf( type, "", string )

void	anteprompt( _context *context );
void	prompt( _context *context );
int	push_output( char *identifier, void *dst, OutputType type, _context *context );
int	pop_output( _context *context, int terminate );
int	outputf( OutputContentsType type, const char *format, ... );


#endif	// OUTPUT_H
