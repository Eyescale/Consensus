#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "expression.h"
#include "command.h"
#include "output.h"
#include "io.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	output
---------------------------------------------------------------------------*/
static char *header_list[] =	// ordered as they appear below
{
	"***** Error: ",
	"consensus> Error: ",
	"consensus> Warning: ",
	"consensus> ",
	"debug> "
};

int
output( OutputContentsType type, const char *format, ... )
{
	_context *context = CN.context;
	int retval = 0;

	if ( type == Error ) {
		CN.context->error.flush_input = ( CN.context->input.event != '\n' );
		retval = -1;
	}
	if ( format == NULL ) {
		return retval;
	}

	va_list ap;
	va_start( ap, format );

	if ( context->output.stack != NULL )
	{
		OutputVA *output = (OutputVA *) context->output.stack->ptr;
		switch ( output->mode ) {
		case PeerOutput:	// DO_LATER
			break;
		case ClientOutput:
			switch ( type ) {
			case Error:
			case Warning:
			case Info:
			case Debug:
				break;
			default:
				io_write( context, type, format, ap );
				va_end( ap );
				return retval;
			}
			break;
		}
	}

	FILE *stream = stderr;
	char *header;
	
	switch ( type ) {
	case Text:
		break;
	case Error:
		if ( CN.context->error.flush_output ) {
			stream = stdout;
			header = header_list[ 0 ];
			CN.context->error.flush_output = 0;
		} else {
			header = header_list[ 1 ];
		}
		break;
	case Warning:
		header = header_list[ 2 ];
		break;
	case Info:
		header = header_list[ 3 ];
		break;
	case Debug:
		header = header_list[ 4 ];
		break;
	}

	switch ( type ) {
	case Text:
		vfprintf( stream, format, ap );
		break;
	default:
		fprintf( stream, "%s", header );
		vfprintf( stream, format, ap );
		fprintf( stream, "\n" );
		break;
	}

	va_end( ap );
	return retval;
}

/*---------------------------------------------------------------------------
	push_output
---------------------------------------------------------------------------*/
int
push_output( char *identifier, void *dst, OutputType type, _context *context )
{
	switch ( type ) {
	case ClientOutput:
		break;
	default:
		return 0;	// DO_LATER
	}

	OutputVA *output = (OutputVA *) calloc( 1, sizeof(OutputVA) );
	output->level = context->control.level;
	output->mode = type;
	output->socket = *(int *) dst;

	addItem( &context->output.stack, output );

	return 0;
}

/*---------------------------------------------------------------------------
	pop_output
---------------------------------------------------------------------------*/
int
pop_output( _context *context )
{
	OutputVA *output = (OutputVA *) context->output.stack->ptr;

	switch ( output->mode ) {
	case ClientOutput:
		// flush output buffer
		io_flush( context );

		// close connection
		close( output->socket );
		break;
	default:
		break;	// DO_LATER
	}
	free( output );
	popListItem( &context->output.stack );
	return 0;
}

/*---------------------------------------------------------------------------
	output_identifier
---------------------------------------------------------------------------*/
void
output_identifier( char *identifier )
{
	do switch ( *identifier ) {
		case ' ':
			output( Text, "%s", "\\ " );
			break;
		case '\t':
			output( Text, "%s", "\\\t" );
			break;
		case '\n':
			output( Text, "%s", "\\n" );
			break;
		case '\0':
			break;
		default:
			output( Text, "%c", *identifier );
	}
	while ( *identifier++ );
}

/*---------------------------------------------------------------------------
	prompt
---------------------------------------------------------------------------*/
void
prompt( _context *context )
{
	if ( !context->control.prompt )
		return;

	context->control.prompt = 0;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	switch ( context->control.mode ) {
	case FreezeMode:
		fprintf( stderr, "(/~) ?_ " );
		break;
	case InstructionMode:
		fprintf( stderr, "directive$ " );
		break;
	case ExecutionMode:
		if ( context->narrative.current ) {
			StackVA *stack = (StackVA *) context->control.stack->ptr;
			if ( stack->narrative.state.closure )
				fprintf( stderr, "(then/.)?_ " );
			else
				fprintf( stderr, "in/on/do_$ " );
		} else {
			fprintf( stderr, "consensus$ " );
		}
		break;
	}
	for ( int i=0; i < context->control.level; i++ )
		fprintf( stderr, "\t" );
}

/*---------------------------------------------------------------------------
	output_variable_value
---------------------------------------------------------------------------*/
static int
test_shorty( Expression *expression )
{
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<3; i++ ) {
		int j = (i+1)%3, k=(j+1)%3;
		if ( just_any( expression, i ) && ( just_any( expression, j ) || just_any( expression, k )))
		{
			return 1;
		}
	}
	return 0;
}

int
output_expression( ExpressionOutput component, Expression *expression, int i, int shorty )
{
	ExpressionSub *sub = expression->sub;
	int mark = expression->result.mark;
	switch ( component ) {
	case SubFlags:
		if ( sub[ i ].result.active )
			output( Text, "*" );
		if ( sub[ i ].result.inactive )
			output( Text, "_" );
		if ( sub[ i ].result.not )
			output( Text, "~" );
		return 1;
	case SubAny:
		output_expression( SubFlags, expression, i, shorty );
		output( Text, mark & ( 1 << i ) ? "?" : "." );
		return 1;
	case SubVariable:
		if ( sub[ i ].result.identifier.value != NULL ) {
			output_expression( SubFlags, expression, i, shorty );
			output( Text, "%%" );
			output_identifier( sub[ i ].result.identifier.value );
		}
		return 1;
	case SubIdentifier:
		if ( sub[ i ].result.identifier.value != NULL ) {
			output_expression( SubFlags, expression, i, shorty );
			output_identifier( sub[ i ].result.identifier.value );
		}
		return 1;
	case SubNull:
		if ( shorty ) {
			output( Text, "[ " );
			if ( mark == ( 1 << i ) ) {
				output( Text, "?:" );
			}
			output( Text, sub[ i ].result.not ? "~" : "~." );
			output( Text, " ]" );
		}
		return 1;
	case SubSub:
		output_expression( SubFlags, expression, i, shorty );
		output( Text, "[ " );
		output_expression( ExpressionAll, sub[ i ].e, -1, -1 );
		output( Text, " ]" );
		return 1;
	case SubAll:
		if ( sub[ i ].result.any ) {
			output_expression( SubAny, expression, i, shorty );
			return 1;
		}
		switch ( sub[ i ].result.identifier.type ) {
		case NullIdentifier:
			output_expression( SubNull, expression, i, shorty );
			break;
		case VariableIdentifier:
			output_expression( SubVariable, expression, i, shorty );
			break;
		case DefaultIdentifier:
			output_expression( SubIdentifier, expression, i, shorty );
			break;
		default:
			break;
		}
		if ( sub[ i ].e != NULL ) {
			output_expression( SubSub, expression, i, shorty );
		}
		return 1;
	case ExpressionMark:
		switch ( mark ) {
		case 0:	return 0;
		case 8: output( Text, "?" );
			return 1;
		default: if ( !sub[ 1 ].result.none &&
			    ((( mark == 1 ) && sub[ 0 ].result.any ) ||
			     (( mark == 2 ) && sub[ 1 ].result.any ) ||
			     (( mark == 4 ) && sub[ 2 ].result.any ) ||
			     ( just_any( expression, 0 ) && ( just_any( expression, 1 ) && ( just_any( expression, 2 ) )))))
				return 0;
		}
		if ( mark & 8 ) {
			output( Text, "?|" );
		}
		output( Text, ( mark & 1 ) ? "?" : "." );
		output( Text, ( mark & 2 ) ? "?" : "." );
		output( Text, ( mark & 4 ) ? "?" : "." );
		return 1;
	case ExpressionAll:
		break;
	}

	// output_expression - main body, aka. ExpressionAll
	// -------------------------------------------------

	if ( expression == NULL ) {
		output( Text, "(null)" );
		return 1;
	}

	// output instance mark
	mark = output_expression( ExpressionMark, expression, -1, -1 );

	// output body in all its various forms...
	if ( sub[ 0 ].result.none ) {
#ifdef DEBUG
		output( Debug, "output_expression: no source..." );
#endif
		;	// do nothing here
	}
	else if ( sub[ 1 ].result.none ) {
#ifdef DEBUG
		output( Debug, "output_expression: no medium..." );
#endif
		if ( sub[ 0 ].result.identifier.type == NullIdentifier ) {
			if ( mark++ ) output( Text, ":" );
			output( Text, sub[ 0 ].result.not ? "~" : "~." );
		}
		else if ( just_any( expression, 0 ) ) {
			if ( !mark ) { mark++; output( Text, "." ); }
		}
		else {
			if ( mark++ ) output( Text, ": " );
			output_expression( SubAll, expression, 0, 0 );
		}
	}
	else if ( test_shorty( expression ) ) {
#ifdef DEBUG
		output( Debug, "output_expression: found shorty..." );
#endif
		if ( mark++ ) output( Text, ": " );
		if ( expression->result.output_swap && !sub[ 2 ].result.any ) {
			output_expression( SubAll, expression, 2, 1 );
			output( Text, "<-.." );
		} else {
			output_expression( SubAll, expression, 0, 1 );
			output_expression( SubAll, expression, 1, 1 );
			output_expression( SubAll, expression, 2, 1 );
		}
	}
	else if ( expression->result.output_swap ) {
#ifdef DEBUG
		output( Debug, "output_expression: output body - swap..." );
#endif
		if ( mark++ ) output( Text, ": " );
		output_expression( SubAll, expression, 2, 0 );
		output( Text, "<-" );
		output_expression( SubAll, expression, 1, 0 );
		output( Text, "-" );
		output_expression( SubAll, expression, 0, 0 );
	} else {
#ifdef DEBUG
		output( Debug, "output_expression: output body..." );
#endif
		if ( mark++ ) output( Text, ": " );
		output_expression( SubAll, expression, 0, 0 );
		output( Text, "-" );
		output_expression( SubAll, expression, 1, 0 );
		output( Text, "->" );
		output_expression( SubAll, expression, 2, 0 );
	}

	// output as_sub
	if ( expression->result.as_sub ) {
		int as_sub = expression->result.as_sub;
		switch ( mark ) {
		case 0: output( Text, ".:" ); break;
		case 1: if ( expression->result.mark ) output( Text, ": . " );
		case 2: output( Text, ": " ); break;
		}
		if ( as_sub & 112 ) {
			output( Text, "~" );
			as_sub >>= 4;
		}
		if ( (as_sub & 7) == 7 ) {
			output( Text, "%%[ ??? ]" );
		} else {
			output( Text, "%%[ " );
			output( Text, as_sub & 1 ? "?" : "." );
			output( Text, as_sub & 2 ? "?" : "." );
			output( Text, as_sub & 4 ? "?" : "." );
			output( Text, " ]" );
		}
		mark = 1;
	}
	else if ( sub[ 3 ].result.identifier.type == NullIdentifier ) {
		if ( mark ) output( Text, ":" );
		output( Text, sub[ 3 ].result.not ? "~" : "~." );
	} else if ( !just_any( expression, 3 ) ) {
		if ( mark ) output( Text, ": " );
		output_expression( SubAll, expression, 3, 0 );
	}
	return 1;
}

void
output_entity_name( Entity *e, Expression *format, int base )
{
	if ( e == NULL ) {
		if ( base ) output( Text,"(null)" );
		return;
	}
	if ( e == CN.nil ) {
		if ( base ) output( Text,"(nil)" );
		return;
	}
	char *name = cn_name( e );
	if ( name != NULL ) {
		output_identifier( name );
		return;
	}
	if ( !base ) output( Text, "[ " );
	if ( format == NULL )
	{
		output_entity_name( e->sub[ 0 ], NULL, 0 );
		output( Text, "-" );
		output_entity_name( e->sub[ 1 ], NULL, 0 );
		output( Text, "->" );
		output_entity_name( e->sub[ 2 ], NULL, 0 );
	}
	else if ( format->result.output_swap )
	{
		output_entity_name( e->sub[ 2 ], format->sub[ 2 ].e, 0 );
		output( Text, "<-" );
		output_entity_name( e->sub[ 1 ], format->sub[ 1 ].e, 0 );
		output( Text, "-" );
		output_entity_name( e->sub[ 0 ], format->sub[ 0 ].e, 0 );
	}
	else
	{
		output_entity_name( e->sub[ 0 ], format->sub[ 0 ].e, 0 );
		output( Text, "-" );
		output_entity_name( e->sub[ 1 ], format->sub[ 1 ].e, 0 );
		output( Text, "->" );
		output_entity_name( e->sub[ 2 ], format->sub[ 2 ].e, 0 );
	}
	if ( !base ) output( Text, " ]" );
}

void
output_narrative_name( Entity *e, char *name )
{
	if ( e == CN.nil )
	{
		output_identifier( name );
		output( Text, "()" );
	}
	else {
		output( Text, "%%[ " );
		output_entity_name( e, NULL, 1 );
		output( Text, " ]." );
		output_identifier( name );
		output( Text, "()" );
	}
}

static void
output_narrative_variable( registryEntry *r )
{
	if ( r == NULL ) return;

	Entity *e = (Entity *) r->identifier;
	char *name = (char *) r->value;
	if ( r->next == NULL ) {
		output_narrative_name( e, name );
	}
	else {
		output( Text, "{ ");
		output_narrative_name( e, name );
		for ( r=r->next; r!=NULL; r=r->next ) {
			output( Text, ", " );
			e = (Entity *) r->identifier;
			name = (char *) r->value;
			output_narrative_name( e, name );
		} 
		output( Text, " }" );
	}
}

static void
output_entity_variable( listItem *i, Expression *format )
{
	if ( i == NULL ) return;
	if ( i->next == NULL ) {
		output_entity_name( (Entity *) i->ptr, format, 1 );
	}
	else  {
		output( Text, "{ " );
		output_entity_name( (Entity *) i->ptr, format, 1 );
		for ( i = i->next; i!=NULL; i=i->next )
		{
			output( Text, ", " );
			output_entity_name( (Entity *) i->ptr, format, 1 );
		}
		output( Text, " }" );
	}
}

static void
output_literal_variable( listItem *i, Expression *format )
{
	if ( i == NULL ) return;
	ExpressionSub *s = (ExpressionSub *) i->ptr;
	if ( i->next == NULL ) {
		output_expression( ExpressionAll, s->e, -1, -1 );
	} else {
		output( Text, "{ " );
		output_expression( ExpressionAll, s->e, -1, -1 );
		for ( i=i->next; i!=NULL; i=i->next ) {
			output( Text, ", " );
			ExpressionSub *s = (ExpressionSub *) i->ptr;
			output_expression( ExpressionAll, s->e, -1, -1 );
		} 
		output( Text, " }" );
	}
}

static void
output_value( VariableVA *variable )
{
	switch ( variable->type ) {
	case NarrativeVariable:
		output_narrative_variable( (registryEntry *) variable->data.value );
		break;
	case EntityVariable:
		output_entity_variable( (listItem *) variable->data.value, NULL );
		break;
	case ExpressionVariable:
		output_expression( ExpressionAll, ((listItem *) variable->data.value )->ptr, -1, -1 );
		break;
	case LiteralVariable:
		output_literal_variable( (listItem *) variable->data.value, NULL );
		break;
	}
}

int
output_variable_value( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
#ifdef DEBUG
	output( Debug, "output_variable_value: %%%s", context->identifier.id[ 0 ].ptr );
#endif
	registryEntry *entry = lookupVariable( context, context->identifier.id[ 0 ].ptr );
	if ( entry != NULL ) {
		output_value( (VariableVA *) entry->value );
	}
	free( context->identifier.id[ 0 ].ptr );
	context->identifier.id[ 0 ].ptr = NULL;

	output( Text, "%c", event );
	return 0;
}

/*---------------------------------------------------------------------------
	output_variator_value
---------------------------------------------------------------------------*/
int
output_variator_value( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
#ifdef DEBUG
	output( Debug, "output_variator_value: %%%s", variator_symbol );
#endif
	registryEntry *entry = lookupVariable( context, variator_symbol );
	if ( entry != NULL ) {
		output_value( (VariableVA *) entry->value );
	}
	return 0;
}


/*---------------------------------------------------------------------------
	output_results
---------------------------------------------------------------------------*/
int
output_results( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	if (( event == '\n' ) && ( context->expression.results == NULL ))
		return 0;

	context->error.flush_output = ( event != '\n' );
	switch ( context->expression.mode ) {
	case ReadMode:
		output_literal_variable( context->expression.results, context->expression.ptr );
		freeLiteral( &context->expression.results );
		break;
	default:
		output_entity_variable( context->expression.results, context->expression.ptr );
	}

	output( Text, "%c", event );
	return 0;
}

/*---------------------------------------------------------------------------
	output_expression_va
---------------------------------------------------------------------------*/
static void
output_va_value( void *value, int narrative_account )
{
	if ( value == NULL ) {
		output( Text, "(null)\n" );
		return;
	}
	if ( narrative_account ) {
		registryEntry *i = (Registry) value;
		if ( i->next == NULL ) {
			output_identifier( (char *) i->identifier );
			output( Text, "()" );
		} else {
			output( Text, "{ " );
			output_identifier( (char *) i->identifier );
			output( Text, "()" );
			for ( i = i->next; i!=NULL; i=i->next ) {
				output( Text, ", " );
				output_identifier( (char *) i->identifier );
				output( Text, "()" );
			}
			output( Text, " }" );
		}
	} else {
		output_identifier( (char *) value );
	}
}

static void
output_va_( char *va_name, int event, _context *context )
{
#ifdef DEBUG
	output( Debug, "output_va_: %%%s.%s", context->identifier.id[ 0 ].ptr, va_name );
#endif
	listItem *i = context->expression.results;
	if ( i == NULL ) return;

	Entity *e = (Entity *) i->ptr;
	void *value = cn_va_get_value( e, va_name );
	int narrative_account = !strcmp( va_name, "narratives" );

	if ( i->next != NULL ) output( Text, "{ " );
	output_va_value( value, narrative_account );
	if ( i->next == NULL ) output( Text, "\n" );
	else {
		for ( i = i->next; i!=NULL; i=i->next ) {
			e = (Entity *) i->ptr;
			value = cn_va_get_value( e, va_name );
			output_va_value( value, narrative_account );
		}
		output( Text, " }\n" );
	}
}

int
output_va( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );

	char *va_name = context->identifier.id[ 2 ].ptr;
	if ( !strcmp( va_name, "html" ) ) {
		if ( context->expression.results != NULL ) {
			Entity *entity = (Entity *) context->expression.results->ptr;
			char *identifier = (char *) cn_va_get_value( entity, "hcn" );
			return push_input( identifier, NULL, HCNFileInput, context );
		}
	}
	if ( !strcmp( va_name, "literal" ) ) {
		return output_results( state, event, next_state, context );
	}
	if ( lookupByName( CN.VB, va_name ) == NULL ) {
		return output( Error, "unknown value account name '%s'", va_name );
	}
	output_va_( va_name, event, context );

	if ( event != '\n' ) {
		output( Text, "%c", event );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	output_mod, output_char, output_special_char
---------------------------------------------------------------------------*/
int
output_mod( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	output( Text, "%%" );
	if ( event != '\\' )
		output( Text, "%c", event );
	return 0;
}
int
output_char( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	output( Text, "%c", event );
	return 0;
}

int
output_special_char( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	switch ( event ) {
	case 't': output( Text, "\t" ); break;
	case 'n': output( Text, "\n" ); break;
	default:
		output( Text, "%c", event );
	}

	return 0;
}

/*---------------------------------------------------------------------------
	output_narrative
---------------------------------------------------------------------------*/
static void
printtab( int level )
{
	for ( int i=0; i < level; i++ ) output( Text, "\t" );
}

static void
narrative_output_occurrence( Occurrence *occurrence, int level )
{
	switch( occurrence->type ) {
	case OtherwiseOccurrence:
		output( Text, "else" );
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			output( Text, " " );
			narrative_output_occurrence( (Occurrence *) i->ptr, level );
		}
		break;
	case ConditionOccurrence:
		output( Text, "in " );
		if ( occurrence->va_num > 1 ) {
			output( Text, "( " );
		}
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			ConditionVA *condition = (ConditionVA *) i->ptr;
			if ( condition->identifier != NULL ) {
				output_identifier( condition->identifier );
				output( Text, ": " );
			}
			else output( Text, ":" );
			output_expression( ExpressionAll, condition->expression, -1, -1 );
			if ( i->next != NULL ) {
				output( Text, ", " );
			}
		}
		if ( occurrence->va_num > 1 ) {
			output( Text, " )" );
		}
		break;
	case EventOccurrence:
		output( Text, "on " );
		if ( occurrence->va_num > 1 ) {
			output( Text, "( " );
		}
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			EventVA *event = (EventVA *) i->ptr;
			if ( event->identifier.name != NULL ) {
				if ( event->identifier.type == VariableIdentifier )
					output( Text, "%%" );
				output_identifier( event->identifier.name );
				output( Text, ": " );
			} else if ( event->type.notification ) {
				output( Text, ":" );
			}
			if ( event->type.request ) {
				if ( event->type.instantiate ) {
					output( Text, "!! " );
				} else if ( event->type.release ) {
					output( Text, "!~ " );
				} else if ( event->type.activate ) {
					output( Text, "!* " );
				} else if ( event->type.deactivate ) {
					output( Text, "!_ " );
				}
			} else if ( event->type.init ) {
				output( Text, "init" );
			}
			if ( event->expression != NULL ) {
				output_expression( ExpressionAll, event->expression, -1, -1 );
			}
			if ( event->type.notification ) {
				if ( event->type.instantiate ) {
					output( Text, " !!" );
				} else if ( event->type.release ) {
					output( Text, " !~" );
				} else if ( event->type.activate ) {
					output( Text, " !*" );
				} else if ( event->type.deactivate ) {
					output( Text, " !_" );
				}
			}
			if ( i->next != NULL ) {
				output( Text, ", " );
			}
		}
		if ( occurrence->va_num > 1 ) {
			output( Text, " )" );
		}
		break;
	case ActionOccurrence:
		; ActionVA *action = (ActionVA *) occurrence->va->ptr;
		listItem *instruction = action->instructions;
		if ( instruction->next == NULL )
		{
			output( Text, "do " );
			_context *context = CN.context;
			context->control.mode = InstructionMode;
			context->narrative.mode.action.one = 1;

			push_input( NULL, instruction->ptr, InstructionOne, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, NULL, context );

			context->narrative.mode.action.one = 0;
			context->control.mode = ExecutionMode;
		}
		else
		{
			output( Text, "do\n" );
			_context *context = CN.context;
			char *state = base;
			int backup = context->control.level;

			push( state, 0, NULL, context );
			context->control.level = level;
			context->control.mode = InstructionMode;

			context->narrative.mode.action.block = 1;
			push_input( NULL, instruction, InstructionBlock, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, NULL, context );

			if ( context->control.level == level ) {
				// returned from '/' - did not pop yet (expecting then)
				pop( state, 0, NULL, context );
			}
			context->control.level = backup;
			context->narrative.mode.action.block = 0;
			context->control.mode = ExecutionMode;
		}
		break;
	case ThenOccurrence:
		output( Text, "then" );
		break;
	}
}

static void
narrative_output_traverse( Occurrence *root, int level )
{
	int do_close = 1;
	for ( listItem *i=root->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		if ( occurrence->type == ThenOccurrence )
			do_close = 0;

		printtab( level );
		do {
			narrative_output_occurrence( occurrence, level );

			OccurrenceType type = occurrence->type;
			if ( occurrence->sub.num == 0 ) {
				ActionVA *action = (ActionVA *) occurrence->va->ptr;
				if (( type != ActionOccurrence ) || ( action->instructions->next == NULL )) {
					output( Text, "\n" );
				}
				occurrence = NULL;
			} else if ( occurrence->sub.num > 1 ) {
				if ( type == ActionOccurrence ) {
					output( Error, "Action can only be followed by then" );
				}
				output( Text, "\n" );
				narrative_output_traverse( occurrence, level + 1 );
				occurrence = NULL;
			} else {
				switch ( type ) {
				case OtherwiseOccurrence:
				case ConditionOccurrence:
					output( Text, "\n" );
					narrative_output_traverse( occurrence, level + 1 );
					occurrence = NULL;
					break;
				case EventOccurrence:
					; Occurrence *sub = occurrence->sub.n->ptr;
					if ( sub->type == ActionOccurrence ) {
						output( Text, " " );
						occurrence = occurrence->sub.n->ptr;
					} else {
						output( Text, "\n" );
						narrative_output_traverse( occurrence, level + 1 );
						occurrence = NULL;
					}
					break;
				case ActionOccurrence:
					; ActionVA *action = (ActionVA *) occurrence->va->ptr;
					if ( action->instructions->next != NULL ) {
						// instruction block - may be followed by then
						narrative_output_traverse( occurrence, level + 1 );
						occurrence = NULL;
					} else {
						// standalone instruction - may be followed by then
						output( Text, " " );
						occurrence = occurrence->sub.n->ptr;
					}
					break;
				case ThenOccurrence:
					output( Text, " " );
					occurrence = occurrence->sub.n->ptr;
					break;
				}
			}
		}
		while ( occurrence != NULL );
	}
	if ( do_close ) {
		printtab( level );
		output( Text, "/\n" );
	}
}

void
narrative_output( Narrative *narrative, _context *context )
{
	if ( narrative == NULL ) return;
	context->narrative.mode.output = 1;

	if ( context->input.stack == NULL )
	{
		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
		fprintf( stderr, "\n" );

		fprintf( stderr, "\n" );
		narrative_output_traverse( &narrative->root, 1 );
		fprintf( stderr, "\n" );

		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
		fprintf( stderr, "\n" );
	}
	else	// no cosmetics
	{
		narrative_output_traverse( &narrative->root, 1 );
	}
	context->narrative.mode.output = 0;
}

int
output_narrative( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	char *name = context->identifier.id[ 1 ].ptr;
	for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
	{
		Entity *e = (Entity *) i->ptr;
		Registry narratives = cn_va_get_value( e, "narratives" );
		if ( narratives == NULL ) continue;
		registryEntry *entry = lookupByName( narratives, name );
		if ( entry == NULL ) continue;
		narrative_output((Narrative *) entry->value, context );
	}
	return 0;
}

