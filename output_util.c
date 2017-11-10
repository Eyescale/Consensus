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
#include "input_util.h"
#include "output.h"
#include "output_util.h"
#include "expression_util.h"
#include "command.h"
#include "io.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

#if 0		// in case of StringVariable
#define NOQ	1
#else
#define NOQ	0
#endif

/*---------------------------------------------------------------------------
	output_identifier
---------------------------------------------------------------------------*/
extern struct {
	int real, fake;
} separator_table[];	// in input_util.c

void
output_identifier( char *identifier, int noq )
{
	_context *context = CN.context;
	IdentifierVA buffer = { NULL, NULL };
	int as_string = 0;

	if (( context->output.stack )) {
		OutputVA *output = context->output.stack->ptr;
		if ( output->mode == StringOutput )
			as_string = 1;
	}
	int has_special = 0;
	for ( char *src = identifier; *src; src++ )
	{
		switch ( *src ) {
		case '"':
			has_special = 1;
			string_append( &buffer, '\\' );
			string_append( &buffer, *src );
			break;
		case '\n':
			has_special = 1;
			string_append( &buffer, '\\' );
			string_append( &buffer, 'n' );
			break;
		case '\\':
			has_special = 1;
			switch ( src[ 1 ] ) {
			case 0:
				break;
			case '"':
			case 'n':
				src++;
				string_append( &buffer, '\\' );
				string_append( &buffer, *src );
				break;
			case 't':
				src++;
				string_append( &buffer, '\t' );
				break;
			default:
				string_append( &buffer, '\\' );
				break;
			}
			break;
		default:
			if ( is_separator( *src ) ) {
				has_special = 1;
				if ( as_string ) string_append( &buffer, '\\' );
			}
			string_append( &buffer, *src );
		}
	}
	string_finish( &buffer, 0 );
	if ( has_special && !( noq || as_string )) {
		outputf( Text, "\"%s\"", buffer.ptr );
	} else {
		output( Text, buffer.ptr );
	}
	free( buffer.ptr );
}

/*---------------------------------------------------------------------------
	output_expression
---------------------------------------------------------------------------*/
static int
test_shorty( Expression *expression )
{
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<3; i++ ) {
		int j = (i+1)%3, k=(i+2)%3;
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
			output( Text, "%" );
			output_identifier( sub[ i ].result.identifier.value, 0 );
		}
		return 1;
	case SubIdentifier:
		if ( sub[ i ].result.identifier.value != NULL ) {
			output_expression( SubFlags, expression, i, shorty );
			output_identifier( sub[ i ].result.identifier.value, 0 );
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
	case ExpressionSup:
		; int as_sup = expression->result.as_sup;
		output( Text, ( as_sup & 1 ) ? "!" : "." );
		output( Text, ( as_sup & 2 ) ? "!" : "." );
		output( Text, ( as_sup & 4 ) ? "!" : "." );
		return 0;
	case ExpressionAll:
		break;
	}

	// output_expression - main body, aka. ExpressionAll
	// -------------------------------------------------

	if ( expression == NULL ) {
		return 1;
	}

	if ( expression->result.as_sup ) {
		// output super
		mark = output_expression( ExpressionSup, expression, -1, -1 );
	} else {
		// output instance mark
		mark = output_expression( ExpressionMark, expression, -1, -1 );
	}

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
			output( Text, "%[ ??? ]" );
		} else {
			output( Text, "%[ " );
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
		if ( mark ) output( Text, ":" );
		output_expression( SubAll, expression, 3, 0 );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	output_entity_name
---------------------------------------------------------------------------*/
void
output_entity_name( Entity *e, Expression *format, int base )
{
	if (( e == NULL ) || ( e == CN.nil )) {
		return;
	}
	char *name = cn_name( e );
	if ( name != NULL ) {
		output_identifier( name, 0 );
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

/*---------------------------------------------------------------------------
	output_variable_value_
---------------------------------------------------------------------------*/
static void
output_narrative_name( Entity *e, char *name )
{
	if ( e == CN.nil )
	{
		output_identifier( name, 0 );
		output( Text, "()" );
	}
	else {
		output( Text, "%[ " );
		output_entity_name( e, NULL, 1 );
		output( Text, " ]." );
		output_identifier( name, 0 );
		output( Text, "()" );
	}
}

static void
output_narrative_variable( Registry *registry )
{
	registryEntry *r = registry->value;
	if ( r == NULL ) return;

	Entity *e = (Entity *) r->index.address;
	char *name = (char *) r->value;
	if ( r->next == NULL ) {
		output_narrative_name( e, name );
	}
	else {
		output( Text, "{ ");
		output_narrative_name( e, name );
		for ( r=r->next; r!=NULL; r=r->next ) {
			output( Text, ", " );
			e = (Entity *) r->index.address;
			name = (char *) r->value;
			output_narrative_name( e, name );
		} 
		output( Text, " }" );
	}
}

int
output_entity_variable( listItem *i, Expression *format )
{
	if ( i == NULL ) return 0;
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
	return 1;
}

int
output_literal_variable( listItem *i, Expression *format )
{
	if ( i == NULL ) return 0;
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
	return 1;
}

static void
output_string_variable( listItem *i )
{
	if ( i == NULL ) return;
	if ( i->next == NULL ) {
		output_identifier((char *) i->ptr, NOQ );
	} else {
		output( Text, "{ " );
		output_identifier((char *) i->ptr, NOQ );
		for ( i=i->next; i!=NULL; i=i->next ) {
			output( Text, ", " );
			output_identifier((char *) i->ptr, NOQ );
		} 
		output( Text, " }" );
	}
}

int
output_variable_value_( VariableVA *variable )
{
	if ( variable->value == NULL )
		return 0;

	switch ( variable->type ) {
	case NarrativeVariable:
		output_narrative_variable( (Registry *) variable->value );
		break;
	case EntityVariable:
		output_entity_variable( (listItem *) variable->value, NULL );
		break;
	case ExpressionVariable:
		output_expression( ExpressionAll, ((listItem *) variable->value )->ptr, -1, -1 );
		break;
	case LiteralVariable:
		output_literal_variable( (listItem *) variable->value, NULL );
		break;
	case StringVariable:
		output_string_variable( (listItem *) variable->value );
		break;
	}
	return 1;
}

/*---------------------------------------------------------------------------
	output_va_
---------------------------------------------------------------------------*/
static void
output_va_value( void *value, int narrative_account )
{
	if ( value == NULL ) {
		return;
	}
	if ( narrative_account ) {
		registryEntry *i = ((Registry *) value )->value;
		if ( i->next == NULL ) {
			output_identifier( (char *) i->index.name, 0 );
			output( Text, "()" );
		} else {
			output( Text, "{ " );
			output_identifier( (char *) i->index.name, 0 );
			output( Text, "()" );
			for ( i = i->next; i!=NULL; i=i->next ) {
				output( Text, ", " );
				output_identifier( (char *) i->index.name, 0 );
				output( Text, "()" );
			}
			output( Text, " }" );
		}
	} else {
		output_identifier( (char *) value, 0 );
	}
}

int
output_va_( char *va_name, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "output_va_: %%%s.%s", context->identifier.id[ 0 ].ptr, va_name );
#endif
	listItem *i = context->expression.results;

	Entity *e = (Entity *) i->ptr;
	void *value = cn_va_get( e, va_name );
	if ( value == NULL ) return 0;

	int narrative_account = !strcmp( va_name, "narratives" );

	if ( i->next != NULL ) output( Text, "{ " );
	output_va_value( value, narrative_account );
	if ( i->next != NULL ) {
		for ( i = i->next; i!=NULL; i=i->next ) {
			e = (Entity *) i->ptr;
			value = cn_va_get( e, va_name );
			if ( value == NULL ) continue;
			output_va_value( value, narrative_account );
		}
		output( Text, " }" );
	}
	return 1;
}

/*---------------------------------------------------------------------------
	output_narrative_
---------------------------------------------------------------------------*/
void
printtab( int level )
{
	for ( int i=0; i < level; i++ )
		for ( int j=0; j < OUTPUT_TABSIZE; j++ )
			output( Text, " " );
}

static void
narrative_output_occurrence( Occurrence *occurrence, int level )
{
	_context *context = CN.context;
	struct { int mode, level; } backup;

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
		if ( occurrence->contrary ) {
			output( Text, "~( " );
		}
		else if ( occurrence->va_num > 1 ) {
			output( Text, "( " );
		}
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			ConditionVA *condition = (ConditionVA *) i->ptr;
			if ( condition->identifier != NULL ) {
				output_identifier( condition->identifier, 0 );
			}
			output( Text, ": " );
			output_expression( ExpressionAll, condition->format, -1, -1 );
			if ( i->next != NULL ) {
				output( Text, ", " );
			}
		}
		if (( occurrence->va_num > 1 ) || occurrence->contrary ) {
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

			// output event identifier
			if ( event->identifier != NULL ) {
				output_identifier( event->identifier, 0 );
			}
			if ( event->type != InitEvent ) output( Text, ": " );

			// output event format
			if ( event->type == StreamEvent ) {
				outputf( Text, "\"file://%s\"", event->format );
			} else if ( event->format != NULL ) {
				output_expression( ExpressionAll, event->format, -1, -1 );
			}

			// output event type
			switch ( event->type ) {
			case InitEvent:
				output( Text, "init" );
				break;
			case StreamEvent:
				output( Text, " !<" );
				break;
			case InstantiateEvent:
				output( Text, " !!" );
				break;
			case ReleaseEvent:
				output( Text, " !~" );
				break;
			case ActivateEvent:
				output( Text, " !*" );
				break;
			case DeactivateEvent:
				output( Text, " !_" );
				break;
			default:
				break;
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
		backup.mode = context->command.mode;
		backup.level = context->command.level;
		set_command_mode( InstructionMode, context );

		ActionVA *action = (ActionVA *) occurrence->va->ptr;
		listItem *instructions = action->instructions;
		if ( instructions->next == NULL ) {
			output( Text, "do " );
			cn_do( instructions->ptr );
		} else {
			output( Text, "do\n" );
			push( base, 0, &same, context );
			context->command.level = level; // just to get the tabs right
			cn_dob( instructions );
			if ( context->command.level == level ) {
				// returned from '/' - did not pop yet (expecting then)
				pop( base, 0, &same, context );
			}
		}
		context->command.level = backup.level;
		set_command_mode( backup.mode, context );
		break;
	case ThenOccurrence:
		output( Text, "then" );
		break;
	}
}

static void
narrative_output_traverse( Occurrence *root, int level )
{
	int do_close = 0;
	for ( listItem *i=root->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = i->ptr;

		printtab( level );
		narrative_output_occurrence( occurrence, level );

		// action blocks include terminating '\n'
		if ( occurrence->type == ActionOccurrence )
		{
			ActionVA *action = occurrence->va->ptr;
			if ( action->instructions->next )
				narrative_output_traverse( occurrence, level + 1 );
			else if ( !occurrence->sub.num )
				output( Text, "\n" );
			else {
				Occurrence *then = occurrence->sub.n->ptr;
				output( Text, " " );
				narrative_output_occurrence( then, level );
				output( Text, "\n" );
				for ( listItem *j = then->sub.n; j!=NULL; j=j->next )
					narrative_output_traverse( then, level + 1 );
			}
		}
		else
		{
			output( Text, "\n" );
			narrative_output_traverse( occurrence, level + 1 );
		}
		if ( occurrence->type != ThenOccurrence )
			do_close = 1;
	}
	if ( do_close ) {
		printtab( level );
		output( Text, "/\n" );
	}
}

void
output_narrative_( Narrative *narrative, _context *context )
{
	if ( narrative == NULL ) return;

	struct { int mode; } backup;
	backup.mode = context->narrative.mode.output;
	context->narrative.mode.output = 1;

	if (( context->input.stack == NULL ) && !context->output.redirected )
	{
		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
		fprintf( stderr, "\n" );

		fprintf( stderr, "\n" );
		narrative_output_traverse( &narrative->root, 1 );
		fprintf( stderr, "\n" );

		for ( int i=0; i<75; i++ ) fprintf( stderr, "-" );
	}
	else	// no cosmetics
	{
		narrative_output_traverse( &narrative->root, 1 );
	}
	context->narrative.mode.output = backup.mode;
}

