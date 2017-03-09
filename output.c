#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "command.h"
#include "output.h"
#include "variables.h"

// #define DEBUG

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
			StackVA *stack = (StackVA *) context->control.stack->next->ptr;
			if ( stack->narrative.state.whole && !stack->narrative.state.then )
				fprintf( stderr, ".../then_$ " );
			else
				fprintf( stderr, "in/on/do_$ " );
		} else {
			printf( "consensus$ " );
		}
		break;
	}
	for ( int i=0; i < context->control.level; i++ )
		printf( "\t" );
}

/*---------------------------------------------------------------------------
	output_variable_value
---------------------------------------------------------------------------*/
static int
just_any( Expression *expression, int i )
{
	ExpressionSub *sub = expression->sub;
	return	sub[ i ].result.any &&
		!sub[ i ].result.active &&
		!sub[ i ].result.inactive &&
		!sub[ i ].result.not;
}

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
output_expression( ExpressionOutput cmp, Expression *expression, int i, int shorty )
{
	ExpressionSub *sub = expression->sub;
	int mark = expression->result.mark;
	switch ( cmp ) {
	case SubFlags:
		if ( sub[ i ].result.active )
			printf( "*" );
		if ( sub[ i ].result.inactive )
			printf( "_" );
		if ( sub[ i ].result.not )
			printf( "~" );
		return 1;
	case SubAny:
		output_expression( SubFlags, expression, i, shorty );
		printf( mark & ( 1 << i ) ? "?" : "." );
		return 1;
	case SubVariable:
		if ( sub[ i ].result.identifier.name != NULL ) {
			output_expression( SubFlags, expression, i, shorty );
			printf( "%%%s", sub[ i ].result.identifier.name );
		}
		return 1;
	case SubIdentifier:
		if ( sub[ i ].result.identifier.name != NULL ) {
			output_expression( SubFlags, expression, i, shorty );
			printf( "%s", sub[ i ].result.identifier.name );
		}
		return 1;
	case SubNull:
		if ( shorty ) {
			printf( "[ " );
			if ( mark == ( 1 << i ) ) {
				printf( "?:" );
			}
			printf( sub[ i ].result.not ? "~" : "~." );
			printf( " ]" );
		}
		return 1;
	case SubSub:
		output_expression( SubFlags, expression, i, shorty );
		printf( "[ " );
		output_expression( ExpressionAll, sub[ i ].e, -1, -1 );
		printf( " ]" );
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
		default:
			output_expression( SubIdentifier, expression, i, shorty );
			break;
		}
		if ( sub[ i ].e != NULL ) {
			output_expression( SubSub, expression, i, shorty );
		}
		return 1;
	case ExpressionMark:
		switch ( mark ) {
		case 0:	return 0;
		case 8: printf( "?" );
			return 1;
		default: if ( !sub[ 1 ].result.none &&
			    ((( mark == 1 ) && sub[ 0 ].result.any ) ||
			     (( mark == 2 ) && sub[ 1 ].result.any ) ||
			     (( mark == 4 ) && sub[ 2 ].result.any ) ||
			     ( just_any( expression, 0 ) && ( just_any( expression, 1 ) && ( just_any( expression, 2 ) )))))
				return 0;
		}
		if ( mark & 8 ) {
			printf ( "?|" );
		}
		printf( ( mark & 1 ) ? "?" : "." );
		printf( ( mark & 2 ) ? "?" : "." );
		printf( ( mark & 4 ) ? "?" : "." );
		return 1;
	default:
		break;
	}

	// output_expression - main body, aka. ExpressionAll
	// -------------------------------------------------

	if ( expression == NULL ) {
		printf( "(null)" );
		return 1;
	}

	// output instance mark
	mark = output_expression( ExpressionMark, expression, -1, -1 );

	// output body in all its various forms...
	if ( sub[ 0 ].result.none ) {
		;	// do nothing here
	}
	else if ( sub[ 1 ].result.none ) {
#ifdef DEBUG
		fprintf( stderr, "debug> output_expression: no medium...\n" );
#endif
		if ( sub[ 0 ].result.identifier.type == NullIdentifier ) {
			if ( mark++ ) printf( ":" );
			printf( sub[ 0 ].result.not ? "~" : "~." );
		}
		else if ( !sub[ 0 ].result.any || sub[ 0 ].result.active || sub[ 0 ].result.inactive ) {
			if ( mark++ ) printf( ": " );
			output_expression( SubAll, expression, 0, 0 );
		}
	}
	else if ( test_shorty( expression ) ) {
#ifdef DEBUG
		fprintf( stderr, "debug> output_expression: found shorty...\n" );
#endif
		if ( mark++ ) printf( ": " );
		if ( expression->result.output_swap && !sub[ 2 ].result.any ) {
			output_expression( SubAll, expression, 2, 1 );
			printf( "<-.." );
		} else {
			output_expression( SubAll, expression, 0, 1 );
			output_expression( SubAll, expression, 1, 1 );
			output_expression( SubAll, expression, 2, 1 );
		}
	}
	else if ( expression->result.output_swap ) {
		if ( mark++ ) printf( ": " );
		output_expression( SubAll, expression, 2, 0 );
		printf( "<-" );
		output_expression( SubAll, expression, 1, 0 );
		printf( "-" );
		output_expression( SubAll, expression, 0, 0 );
	} else {
		if ( mark++ ) printf( ": " );
		output_expression( SubAll, expression, 0, 0 );
		printf( "-" );
		output_expression( SubAll, expression, 1, 0 );
		printf( "->" );
		output_expression( SubAll, expression, 2, 0 );
	}

	// output as_sub
	if ( expression->result.as_sub ) {
		int as_sub = expression->result.as_sub;
		switch ( mark ) {
		case 0: printf( ".:" ); break;
		case 1: if ( expression->result.mark ) printf( ": . " );
		case 2: printf( ": " ); break;
		}
		if ( as_sub & 112 ) {
			printf( "~" );
			as_sub >>= 4;
		}
		if ( (as_sub & 7) == 7 ) {
			printf( "%%[ ??? ]" );
		} else {
			printf( "%%[ " );
			printf( as_sub & 1 ? "?" : "." );
			printf( as_sub & 2 ? "?" : "." );
			printf( as_sub & 4 ? "?" : "." );
			printf( " ]" );
		}
		mark = 1;
	}
	else if ( sub[ 3 ].result.identifier.type == NullIdentifier ) {
		if ( mark ) printf( ":" );
		printf( sub[ 3 ].result.not ? "~" : "~." );
	} else if ( !sub[ 3 ].result.any || sub[ 3 ].result.active || sub[ 3 ].result.inactive ) {
		if ( mark ) printf( ": " );
		output_expression( SubAll, expression, 3, 0 );
	}
	return 1;
}

void
output_name( Entity *e, Expression *format, int base )
{
	if ( e == NULL ) {
		if ( base ) printf("(null)" );
		return;
	}
	if ( e == CN.nil ) {
		if ( base ) printf("(nil)" );
		return;
	}
	char *name = cn_name( e );
	if ( name != NULL ) {
		printf( "%s", name );
		return;
	}
	if ( !base ) printf( "[ " );
	if ( format == NULL )
	{
		output_name( e->sub[ 0 ], NULL, 0 );
		printf( "-" );
		output_name( e->sub[ 1 ], NULL, 0 );
		printf( "->" );
		output_name( e->sub[ 2 ], NULL, 0 );
	}
	else if ( format->result.output_swap )
	{
		output_name( e->sub[ 2 ], format->sub[ 2 ].e, 0 );
		printf( "<-" );
		output_name( e->sub[ 1 ], format->sub[ 1 ].e, 0 );
		printf( "-" );
		output_name( e->sub[ 0 ], format->sub[ 0 ].e, 0 );
	}
	else
	{
		output_name( e->sub[ 0 ], format->sub[ 0 ].e, 0 );
		printf( "-" );
		output_name( e->sub[ 1 ], format->sub[ 1 ].e, 0 );
		printf( "->" );
		output_name( e->sub[ 2 ], format->sub[ 2 ].e, 0 );
	}
	if ( !base ) printf( " ]" );
}

static void
output_results( listItem *i, Expression *format )
{
	if ( i == NULL ) {
		return;
	}
	else if ( i->next == NULL ) {
		output_name( (Entity *) i->ptr, format, 1 );
	}
	else  {
		printf( "{ " );
		output_name( (Entity *) i->ptr, format, 1 );
		for ( i = i->next; i!=NULL; i=i->next )
		{
			printf( ", " );
			output_name( (Entity *) i->ptr, format, 1 );
		}
		printf( " }" );
	}
}

static void
output_value( VariableVA *variable )
{
	switch ( variable->type ) {
	case NarrativeVariable:
		; registryEntry *i = (Registry) variable->data.value;
		Entity *e = (Entity *) i->identifier;
		char *narrative = (char *) i->value;
		if ( i->next == NULL ) {
			if ( i->identifier == CN.nil ) printf( "%s()", narrative );
			else { output_name( e, NULL, 1 ); printf( ".%s()", narrative ); }
		}
		else {
			printf( "{ ");
			if ( i->identifier == CN.nil ) printf( "%s()", narrative );
			else printf( "%s.%s()", cn_name( e ), narrative );
			for ( i=i->next; i!=NULL; i=i->next ) {
				e = (Entity *) i->identifier;
				narrative = (char *) i->value;
				if ( i->identifier == CN.nil ) printf( "%s()", narrative );
				else { output_name( e, NULL, 1 ); printf( ".%s()", narrative ); }
			} 
			printf( " }" );
		}
		break;
	case ExpressionVariable:
		output_expression( ExpressionAll, (Expression *) variable->data.value, -1, -1 );
		break;
	case EntityVariable:
		output_results( (listItem *) variable->data.value, NULL );
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
	fprintf( stderr, "debug> > : %%%s\n", context->identifier.id[ 0 ].ptr );
#endif
	registryEntry *entry = lookupVariable( context, context->identifier.id[ 0 ].ptr );
	if ( entry != NULL ) {
		output_value( (VariableVA *) entry->value );
	}
	free( context->identifier.id[ 0 ].ptr );
	context->identifier.id[ 0 ].ptr = NULL;

	printf( "%c", event );
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
	fprintf( stderr, "debug> > : %%%s\n", variator_symbol );
#endif
	registryEntry *entry = lookupVariable( context, variator_symbol );
	if ( entry != NULL ) {
		output_value( (VariableVA *) entry->value );
	}
	return 0;
}


/*---------------------------------------------------------------------------
	output_expression_results
---------------------------------------------------------------------------*/
int
output_expression_results( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	output_results( context->expression.results, context->expression.ptr );

	printf( "%c", event );
	return 0;
}

/*---------------------------------------------------------------------------
	output_expression_va
---------------------------------------------------------------------------*/
static void
output_va_value( void *value, int narrative_account )
{
	if ( value == NULL ) {
		printf( "(null)\n" );
		return;
	}
	if ( narrative_account ) {
		registryEntry *i = (Registry) value;
		if ( i->next == NULL ) {
			printf( "%s()", (char *) i->identifier );
		} else {
			printf( "{ %s()", (char *) i->identifier );
			for ( i = i->next; i!=NULL; i=i->next ) {
				printf( ", %s()", (char *) i->identifier );
			}
			printf( " }" );
		}
	} else {
		printf( "\"%s\"", (char *) value );
	}
}

static void
output_va_( char *va_name, int event, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> >: %%%s.%s\n", context->identifier.id[ 0 ].ptr, va_name );
#endif
	listItem *i = context->expression.results;
	if ( i == NULL ) return;

	Entity *e = (Entity *) i->ptr;
	void *value = cn_va_get_value( va_name, e );
	int narrative_account = !strcmp( va_name, "narratives" );

	if ( i->next != NULL ) printf( "{ " );
	output_va_value( value, narrative_account );
	if ( i->next == NULL ) printf( "\n" );
	else {
		for ( i = i->next; i!=NULL; i=i->next ) {
			e = (Entity *) i->ptr;
			value = cn_va_get_value( va_name, e );
			output_va_value( value, narrative_account );
		}
		printf( " }\n" );
	}
}

int
output_va( char *state, int event, char **next_state, _context *context )
{
	context->error.flush_output = ( event != '\n' );

	char *va_name = context->identifier.id[ 2 ].ptr;
	if ( !strcmp( va_name, "html" ) ) {
		return push_input_hcn( state, event, next_state, context );
	}
	if ( lookupByName( CN.VB, va_name ) == NULL ) {
		char *msg; asprintf( &msg, "unknown value account name '%s'", va_name );
		event = log_error( context, event, msg ); free( msg );
		return event;
	}
	output_va_( va_name, event, context );

	printf( "%c", event );
	return 0;
}

/*---------------------------------------------------------------------------
	output_mod, output_char
---------------------------------------------------------------------------*/
int
output_mod( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	printf( "%%" );
	if ( event != '\\' )
		printf( "%c", event );
	return 0;
}
int
output_char( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, 0, ExecutionMode ) )
		return 0;

	context->error.flush_output = ( event != '\n' );
	printf( "%c", event );
	return 0;
}

/*---------------------------------------------------------------------------
	output_narrative
---------------------------------------------------------------------------*/
static void
printtab( int level )
{
	for ( int i=0; i < level; i++ ) printf( "\t" );
}

static void
narrative_output_occurrence( Occurrence *occurrence, int level )
{
	switch( occurrence->type ) {
	case ConditionOccurrence:
		output_expression( ExpressionAll, occurrence->va.condition.expression, -1, -1 );
		break;
	case EventOccurrence:
		if ( occurrence->va.event.identifier.name != NULL ) {
			if ( occurrence->va.event.identifier.type == VariableIdentifier )
				printf( "%%" );
			printf( "%s: ", occurrence->va.event.identifier.name );
		} else if ( occurrence->va.event.type.notification ) {
			printf( ":" );
		}
		if ( occurrence->va.event.type.request ) {
			if ( occurrence->va.event.type.instantiate ) {
				printf( "!! " );
			} else if ( occurrence->va.event.type.release ) {
				printf( "!~ " );
			} else if ( occurrence->va.event.type.activate ) {
				printf( "!* " );
			} else if ( occurrence->va.event.type.deactivate ) {
				printf( "!_ " );
			}
		} else if ( occurrence->va.event.type.init ) {
			printf( "init" );
		}
		if ( occurrence->va.event.expression != NULL ) {
			output_expression( ExpressionAll, occurrence->va.event.expression, -1, -1 );
		} else if ( occurrence->va.event.narrative_identifier != NULL ) {
			printf( "%s()", occurrence->va.event.narrative_identifier );
		}
		if ( occurrence->va.event.type.notification ) {
			if ( occurrence->va.event.type.instantiate ) {
				printf( " !!" );
			} else if ( occurrence->va.event.type.release ) {
				printf( " !~" );
			} else if ( occurrence->va.event.type.activate ) {
				printf( " !*" );
			} else if ( occurrence->va.event.type.deactivate ) {
				printf( " !_" );
			}
		}
		break;
	case ActionOccurrence:
		; listItem *instruction = occurrence->va.action.instructions;
		if ( instruction->next == NULL )
		{
			printf( "do " );
			_context *context = CN.context;
			context->control.mode = InstructionMode;
			context->narrative.mode.one = 1;

			push_input( NULL, occurrence->va.action.instructions->ptr, APIStringInput, context );
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, &same, context );

			context->narrative.mode.one = 0;
			context->control.mode = ExecutionMode;
		}
		else
		{
			printf( "do\n" );
			_context *context = CN.context;
			char *state = base;
			int backup = context->control.level;

			push( state, 0, &same, context );
			context->control.level = level;
			context->control.mode = InstructionMode;
			context->control.execute = occurrence->va.action.instructions;
			context->record.level = level;
			context->narrative.mode.block = 1;

			push_input( NULL, occurrence->va.action.instructions->ptr, BlockStringInput, context );
			((StreamVA *) context->input.stack->ptr )->level--; // we want to call pop_input ourselves
			int event = read_command( base, 0, &same, context );
			pop_input( base, 0, &same, context );

			if ( context->control.level == level ) {
				// returned from '/' - did not pop yet (expecting then)
				pop( state, 0, NULL, context );
			}
			context->control.level = backup;
			context->narrative.mode.block = 0;
			context->control.mode = ExecutionMode;
		}
		break;
	case ThenOccurrence:
		printf( "then" );
		break;
	}
}

static void
narrative_output_standalone( Occurrence *occurrence, int level )
{
	switch ( occurrence->type ) {
	case ConditionOccurrence:
		printf( "in " );
		break;
	case EventOccurrence:
		printf( "on " );
		break;
	case ActionOccurrence:
	case ThenOccurrence:
		break;
	}
	narrative_output_occurrence( occurrence, level );
}

static Occurrence *
narrative_output_vector( Occurrence *occurrence, int level )
{
	OccurrenceType type = occurrence->type;
	if ( type == ConditionOccurrence )
		printf( "in ( " );
	else
		printf( "on ( " );

	narrative_output_occurrence( occurrence, level );
	do {
		occurrence = occurrence->sub.n->ptr;
		if ( occurrence->type == type ) {
			printf( ", " ); narrative_output_occurrence( occurrence, level );
		}
	}
	while (( occurrence->type == type ) && ( occurrence->sub.num == 1 ));
	printf( " )" );

	return occurrence;
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
		do
		{
			OccurrenceType type = occurrence->type;
			if ( occurrence->sub.num == 0 ) {
				narrative_output_standalone( occurrence, level );
				if (( type == ActionOccurrence ) ? ( occurrence->va.action.instructions->next == NULL ) : 1 ) {
					printf( "\n" );
				}
				occurrence = NULL;
			}
			else if ( occurrence->sub.num > 1 ) {
				if ( type == ActionOccurrence ) {
					fprintf( stderr, "consensus> Error: Action can only be followed by then\n" );
				}
				narrative_output_standalone( occurrence, level ); printf( "\n" );
				narrative_output_traverse( occurrence, level + 1 );
				occurrence = NULL;
			}
			else {
				Occurrence *sub = (Occurrence *) occurrence->sub.n->ptr;
				switch ( type ) {
				case ActionOccurrence:
					narrative_output_standalone( occurrence, level );
					if (( type == ActionOccurrence ) && ( occurrence->va.action.instructions->next != NULL )) {
						narrative_output_traverse( occurrence, level + 1 );
						occurrence = NULL;
					} else {
						printf( " " );
						occurrence = sub;
					}
					break;
				case ThenOccurrence:
					narrative_output_standalone( occurrence, level ); printf( " " );
					occurrence = sub;
					break;
				case ConditionOccurrence:
				case EventOccurrence:
					if ( sub->type != type ) {
						narrative_output_standalone( occurrence, level ); printf( " " );
						occurrence = sub;
						break;
					}
					occurrence = narrative_output_vector( occurrence, level );
					if ( occurrence->type != type ) {
						printf( " " );
					}
					else if ( occurrence->sub.num == 0 ) {
						printf( "\n" );
						occurrence = NULL;
					}
					else if ( occurrence->sub.num > 1 ) {
						printf( "\n" );
						narrative_output_traverse( occurrence, level + 1 );
						occurrence = NULL;
					}
					else {
						printf( " " );
					}
					break;
				}
			}
		}
		while ( occurrence != NULL );
	}
	if ( do_close ) {
		printtab( level );
		printf( "/\n" );
	}
}

static void
narrative_output( Narrative *narrative, _context *context )
{
	if ( narrative == NULL ) return;
	if ( context->input.stack == NULL )
	{
		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
		fprintf( stderr, "\n" );
	}

	context->narrative.mode.output = 1;
	if ( context->input.stack == NULL ) printf( "\n" );
	narrative_output_traverse( &narrative->root, 1 );
	if ( context->input.stack == NULL ) printf( "\n" );
	context->narrative.mode.output = 0;

	if ( context->input.stack == NULL )
	{
		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
		fprintf( stderr, "\n" );
	}
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
		Registry narratives = cn_va_get_value( "narratives", e );
		if ( narratives == NULL ) continue;
		registryEntry *entry = lookupByName( narratives, name );
		if ( entry == NULL ) continue;
		narrative_output((Narrative *) entry->value, context );
	}
	return 0;
}

