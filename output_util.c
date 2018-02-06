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

/*---------------------------------------------------------------------------
	printtab
---------------------------------------------------------------------------*/
void
printtab( int level )
{
	for ( int i=0; i < level; i++ )
		for ( int j=0; j < OUTPUT_TABSIZE; j++ )
			output( Text, " " );
}


/*---------------------------------------------------------------------------
	output_identifier
---------------------------------------------------------------------------*/
void
output_identifier( char *identifier )
{
	_context *context = CN.context;
	StringVA *buffer = newString();
	if ( context->output.as_is ) {
		// output identifier without enclosing quotes
		char *src = identifier;
		if ( *src != '\"' )
			string_append( buffer, *src );
#ifdef PREVIOUS
		for ( src++; src[ 1 ]; src++ ) {
			if ( *src == '\\' ) {
				string_append( buffer, '\\' );
			}
			string_append( buffer, *src );
		}
#else
		for ( src++; src[ 1 ]; src++ )
			string_append( buffer, *src );
#endif
		if ( *src != '\"' )
			string_append( buffer, *src );
		string_finish( buffer, 0 );
		output( Text, buffer->value );
	}
	else {
		int has_special = 0;
		for ( char *src = identifier; *src; src++ ) {
			switch ( *src ) {
			case '\r':
				// skip this one - until further notice..
				break;
			case '"':
				has_special = 1;
				string_append( buffer, '\\' );
				string_append( buffer, '\"' );
				break;
			case '\\':
				switch ( src[ 1 ] ) {
				case '"':
				case '\\':
					string_append( buffer, '\\' );
					string_append( buffer, *src );
					src++;
					break;
				default:
					string_append( buffer, '\\' );
					string_append( buffer, '\\' );
					break;
				}
				break;
			default:
				if ( is_separator( *src ) ) {
					has_special = 1;
				}
				string_append( buffer, *src );
			}
		}
		string_finish( buffer, 0 );
		if ( !has_special ) {
			output( Text, buffer->value );
		}
		else if ( context->input.hcn ) {
			outputf( Text, "%%22%s%%22", buffer->value );
		}
		else {
			outputf( Text, "\"%s\"", buffer->value );
		}
	}
	freeString( buffer );
}

/*---------------------------------------------------------------------------
	output_collection
---------------------------------------------------------------------------*/
static void singleton_bgn( int inb, ValueType type );
static void singleton_end( int inb, ValueType type );
static void collection_bgn( int inb, ValueType type );
static void collection_end( int inb, ValueType type );
static void output_elem( void *element, ValueType type, void *format );

#define FIRST(i)	( (list) ? i : ((Registry *) i )->value )
#define NEXT(i)		( (list) ? (void *)((listItem *) i )->next : (void *)((registryEntry *) i )->next )
#define ELEM(i)		( list ? ((listItem *) i )->ptr : i )
int
output_collection( void *i, ValueType type, void *format, int inb )
{
	if ( i == NULL ) return 0;
	int list = ( type == NarrativeIdentifiers ) ? 0 : 1;

	i = FIRST( i );
	if ( NEXT( i ) == NULL ) {
		singleton_bgn( inb, type );
		output_elem( ELEM( i ), type, format );
		singleton_end( inb, type );
	}
	else  {
		collection_bgn( inb, type );
		output_elem( ELEM( i ), type, format );
		for ( i = NEXT( i ); i!=NULL; i=NEXT( i ) )
		{
			output( Text, ", " );
			output_elem( ELEM( i ), type, format );
		}
		collection_end( inb, type );
	}
	return 1;
}

static void
singleton_bgn( int inb, ValueType type )
/*
	inb, if true: in expression; otherwise
	could be as variable value or expression results
*/
{
	if ( inb ) {
		output( Text, "{ " );
	}
	else {
		switch ( type ) {
#ifdef DEBUG
		case LiteralResults:
			output( Text, "$\\{ " );
			break;
#endif
		default:
			break;
		}
	}
}
static void
singleton_end( int inb, ValueType type )
{
	if ( inb ) {
		output( Text, " }" );
	}
	else {
		switch ( type ) {
#ifdef DEBUG
		case LiteralResults:
			output( Text, " }" );
			break;
#endif
		default:
			break;
		}
	}
}
static void
collection_bgn( int inb, ValueType type )
{
	if ( inb ) {
		output( Text, "{ " );
	}
	else {
		switch ( type ) {
#ifdef DEBUG
		case LiteralResults:
			output( Text, "$\\{ " );
			break;
#endif
		default:
			output( Text, "{ " );
			break;
		}
	}
}
static void
collection_end( int inb, ValueType type )
{
	output( Text, " }" );
}
static void output_narrative_name( Entity *e, char *name );
static void
output_elem( void *element, ValueType type, void *format )
{
	switch ( type ) {
	case ExpressionCollect:
		output_expression( ExpressionAll, element );
		break;
	case EntityResults:
		output_entity( element, 3, format );
		break;
	case LiteralResults:
		output_literal( element );
		break;
	case StringResults:
		output_identifier( element );
		break;
	case NarrativeIdentifiers:;
		registryEntry *r = element;
		Entity *e = (Entity *) r->index.address;
		char *name = (char *) r->value;
		output_narrative_name( e, name );
	default:
		break;
	}
}

/*---------------------------------------------------------------------------
	output_variable_value
---------------------------------------------------------------------------*/
int
output_variable_value( VariableVA *variable )
{
	if ( variable->value == NULL )
		return 0;

	switch ( variable->type ) {
	case NarrativeVariable:
		output_collection( variable->value, NarrativeIdentifiers, NULL, 0 );
		break;
	case EntityVariable:
		output_collection( variable->value, EntityResults, NULL, 0 );
		break;
	case ExpressionVariable:
		output_collection( variable->value, ExpressionCollect, NULL, 0 );
		break;
	case LiteralVariable:
		output_collection( variable->value, LiteralResults, NULL, 0 );
		break;
	case StringVariable:
		output_collection( variable->value, StringResults, NULL, 0 );
		break;
	}
	return 1;
}

/*---------------------------------------------------------------------------
	output_expression
---------------------------------------------------------------------------*/
static int test_shorty( Expression *expression );

int
output_expression( ExpressionOutput part, Expression *expression, ... )
{
	if ( expression == NULL )
		return 0;

	int i, shorty;
	listItem *list;
	if ( part != ExpressionAll )
	{
		va_list ap;
		va_start( ap, expression );
		i = va_arg( ap, int );		// the Sub index where required
		shorty = va_arg( ap, int );	// for SubNull to come out as '~.' in case of shorty
		va_end( ap );
	}
	
	ExpressionSub *sub = expression->sub;
	int mark = expression->result.mark;
	switch ( part ) {
	case SubFlags:
		if ( sub[ i ].result.active )
			output( Text, "*" );
		if ( sub[ i ].result.passive )
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
		else if ( sub[ i ].result.not ) {
			output( Text, "~" );
		}
		return 1;
	case SubSub:
		output_expression( SubFlags, expression, i, shorty );
		output( Text, "[ " );
		output_expression( ExpressionAll, sub[ i ].e );
		output( Text, " ]" );
		return 1;
	case SubAll:
		if ( sub[ i ].result.any ) {
			output_expression( SubAny, expression, i, shorty );
			return 1;
		}
		int type = sub[ i ].result.identifier.type;
		switch ( type ) {
		case NullIdentifier:
			output_expression( SubNull, expression, i, shorty );
			break;
		case VariableIdentifier:
			output_expression( SubVariable, expression, i, shorty );
			break;
		case DefaultIdentifier:
			output_expression( SubIdentifier, expression, i, shorty );
			break;
		case ExpressionCollect:
		case EntityResults:
		case LiteralResults:
		case StringResults:
			output_collection( sub[ i ].result.identifier.value, type, NULL, 1 );
			break;
		default:
			if ( sub[ i ].e != NULL ) {
				output_expression( SubSub, expression, i, shorty );
			}
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
		if (( expression->result.twist & 8 ) && !sub[ 2 ].result.any ) {
			output_expression( SubAll, expression, 2, 1 );
			output( Text, "<-.." );
		} else {
			output_expression( SubAll, expression, 0, 1 );
			output_expression( SubAll, expression, 1, 1 );
			output_expression( SubAll, expression, 2, 1 );
		}
	}
	else if ( expression->result.twist & 8 ) {
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
		case 0: // neither mark nor body
			output( Text, ". : " ); break;
		case 1: // mark but no body
			if ( expression->result.mark )
				output( Text, ": . " );
			// no break
		case 2: // mark and body
			output( Text, ": " ); break;
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
		if ( mark ) output( Text, ": " );
		output( Text, sub[ 3 ].result.not ? "~" : "~." );
	} else if ( !just_any( expression, 3 ) ) {
		if ( mark ) output( Text, ": " );
		output_expression( SubAll, expression, 3, 0 );
	}
	return 1;
}

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

/*---------------------------------------------------------------------------
	output_literal
---------------------------------------------------------------------------*/
void
output_literal( Literal *l )
{
	output_expression( ExpressionAll, l->e );
}

/*---------------------------------------------------------------------------
	output_entity
---------------------------------------------------------------------------*/
void
output_entity( Entity *entity, int i, Expression *format )
{
	Entity *e = ( i < 3 ) ? entity->sub[ i ] : entity;
	if ( e == NULL ) {
#ifdef DEBUG
		output( Text, "(null)" );
#endif
		return;
	}
	if ( e == CN.nil ) {
		if ( i == 3 ) output( Text, "~." );
		return;
	}
	char *name = cn_name( e );
	if ( name != NULL ) {
		output_identifier( name );
		return;
	}
	if ( i < 3 ) output( Text, "[ " );
	if (( format == NULL ) || !format->result.twist ) {
		int twist = ( i < 3 ) ? entity->twist & ( 1 << i ) :
			( format == NULL ) || !format->result.as_sub ? 0 :
			entity->twist & 8;
		if ( twist ) {
			output_entity( e, 2, NULL );
			output( Text, "<-" );
			output_entity( e, 1, NULL );
			output( Text, "-" );
			output_entity( e, 0, NULL );
		}
		else {
			output_entity( e, 0, NULL );
			output( Text, "-" );
			output_entity( e, 1, NULL );
			output( Text, "->" );
			output_entity( e, 2, NULL );
		}
	}
	else if ( format->result.twist & 8 ) {
		output_entity( e, 2, format->sub[ 2 ].e );
		output( Text, "<-" );
		output_entity( e, 1, format->sub[ 1 ].e );
		output( Text, "-" );
		output_entity( e, 0, format->sub[ 0 ].e );
	}
	else {
		output_entity( e, 0, format->sub[ 0 ].e );
		output( Text, "-" );
		output_entity( e, 1, format->sub[ 1 ].e );
		output( Text, "->" );
		output_entity( e, 2, format->sub[ 2 ].e );
	}
	if ( i < 3 ) output( Text, " ]" );
}

/*---------------------------------------------------------------------------
	output_entities_va
---------------------------------------------------------------------------*/
static void output_va_value( void *value, int narrative_account );

int
output_entities_va( char *va_name, _context *context )
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

static void
output_va_value( void *value, int narrative_account )
{
	if ( value == NULL ) {
		return;
	}
	if ( narrative_account ) {
		registryEntry *i = ((Registry *) value )->value;
		if ( i->next == NULL ) {
			output_identifier( (char *) i->index.name );
			output( Text, "()" );
		} else {
			output( Text, "{ " );
			output_identifier( (char *) i->index.name );
			output( Text, "()" );
			for ( i = i->next; i!=NULL; i=i->next ) {
				output( Text, ", " );
				output_identifier( (char *) i->index.name );
				output( Text, "()" );
			}
			output( Text, " }" );
		}
	}
	else output_identifier( (char *) value );
}

/*---------------------------------------------------------------------------
	output_narrative_name	- local
---------------------------------------------------------------------------*/
static void
output_narrative_name( Entity *e, char *name )
{
	if ( e == CN.nil ) {
		output_identifier( name );
		output( Text, "()" );
	}
	else {
		output( Text, "$[ " );
		output_entity( e, 3, NULL );
		output( Text, " ]." );
		output_identifier( name );
		output( Text, "()" );
	}
}

/*---------------------------------------------------------------------------
	output_narrative
---------------------------------------------------------------------------*/
static void narrative_output_traverse( Occurrence *root, int level );

void
output_narrative( Narrative *narrative, _context *context )
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
		fprintf( stderr, "\n" );
	}
	else	// no cosmetics
	{
		narrative_output_traverse( &narrative->root, 1 );
	}
	context->narrative.mode.output = backup.mode;
}

static void narrative_output_occurrence( Occurrence *, int level );
static Occurrence *single_action_block( Occurrence *occurrence );
static void
narrative_output_traverse( Occurrence *root, int level )
{
	int do_close = root->sub.num;
	for ( listItem *i=root->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *sub, *occurrence = i->ptr;

		printtab( level );
		narrative_output_occurrence( occurrence, level );

		// action blocks include terminating '\n'
		if ( occurrence->type == ActionOccurrence ) {
			ActionVA *action = occurrence->va->ptr;
			if ( action->instructions->next )
				narrative_output_traverse( occurrence, level + 1 );
			else if ( !occurrence->sub.num )
				output( Text, "\n" );
			else {
				output( Text, " " );
				Occurrence *then = occurrence->sub.n->ptr;
				narrative_output_occurrence( then, level );
				output( Text, "\n" );
				narrative_output_traverse( then, level + 1 );
			}
		}
		else if (( sub = single_action_block( occurrence ) )) {
			output( Text, " " );
			narrative_output_occurrence( sub, level );
			narrative_output_traverse( sub, level + 1 );
		}
		else {
			output( Text, "\n" );
			narrative_output_traverse( occurrence, level + 1 );
		}
		if ( occurrence->type == ThenOccurrence )
			do_close = 0;
	}
	if ( do_close ) {
		printtab( level );
		output( Text, "/\n" );
	}
}
static Occurrence *
single_action_block( Occurrence *occurrence )
{
	if ( occurrence->sub.num == 1 ) {
		Occurrence *sub = occurrence->sub.n->ptr;
		if ( sub->type == ActionOccurrence ) {
			ActionVA *action = sub->va->ptr;
			if (( action->instructions->next ))
				return sub;
		}
	}
	return NULL;
}
static void
narrative_output_occurrence( Occurrence *occurrence, int level )
{
	_context *context = CN.context;
	struct { int mode; } backup;

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
				output_identifier( condition->identifier );
			}
			output( Text, ": " );
			output_expression( ExpressionAll, condition->format );
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
				output_identifier( event->identifier );
			}
			switch ( event->source.scheme ) {
			case CGIScheme:
				output( Text, " < cgi >" );
				break;
			default:
				// DO_LATER
				break;
			}
			if ( event->type != InitEvent ) output( Text, ": " );

			// output event format
			if ( event->type == StreamEvent ) {
				outputf( Text, "\"file://%s\"", event->format );
			} else if ( event->format != NULL ) {
				output_expression( ExpressionAll, event->format );
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
		set_command_mode( InstructionMode, context );

		ActionVA *action = (ActionVA *) occurrence->va->ptr;
		listItem *instructions = action->instructions;
		if ( instructions->next == NULL ) {
			output( Text, "do " );
			cn_do( instructions->ptr );
		} else {
			output( Text, "do\n" );
			cn_dob( instructions, level );
		}
		set_command_mode( backup.mode, context );
		break;
	case ThenOccurrence:
		output( Text, "then" );
		break;
	}
}

