#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "command_util.h"
#include "expression_util.h"
#include "input.h"
#include "input_util.h"
#include "output_util.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"
#include "value.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define do_( a, s ) \
	event = va_execute( a, &state, event, s, context );

static int
va_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) )
		*state = next_state;
	return event;
}

/*---------------------------------------------------------------------------
	read_va
---------------------------------------------------------------------------*/
static _action va2s, xr2s;
int
read_va( char *state, int event, char **next_state, _context *context )
{
	free_identifier( context, 2 );
	state = base;
	do {
		event = read_input( state, event, next_state, context );
		bgn_
		on_( -1 )	do_( nothing, "" )
		in_( base ) bgn_
			on_( '%' )	do_( nop, "%" )
			on_other	do_( read_2, "" )
			end
		in_( "%" ) bgn_
			on_( '[' )	do_( nop, "%[" )
			on_other	do_( read_1, "%variable" )
			end
			in_( "%variable" ) bgn_
				on_any	do_( va2s, "" )
				end
			in_( "%[" ) bgn_
				on_any	do_( solve_expression, "%[_" )
				end
				in_( "%[_" ) bgn_
					on_( ' ' )	do_( nop, same )
					on_( '\t' )	do_( nop, same )
					on_( ']' )	do_( nop, "%[_]" )
					on_other	do_( error, "" )
					end
				in_( "%[_]" ) bgn_
					on_any	do_( xr2s, "" )
					end
		end
	}
	while ( strcmp( state, "" ) );
	return event;
}

static void take_va( listItem *, int, _context * );
static int
xr2s( char *state, int event, char **next_state, _context *context )
/*
	xr2s - translate expression's first result into string, and
	stores the result into context->identifier.id[ 2 ]
*/
{
	if ( command_mode( 0, 0, ExecutionMode ) )
		take_va( context->expression.results, context->expression.mode, context );
	return event;
}
static int
va2s( char *state, int event, char **next_state, _context *context )
/*
	va2s - translate variable's first value into string, and
	stores the result into context->identifier.id[ 2 ]
*/
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return event;

	for ( listItem *i=context->output.slist; i!=NULL; i=i->next )
		free( i->ptr );
	freeListItem( &context->output.slist );

	char *identifier = get_identifier( context, 1 );
	VariableVA *variable = lookupVariable( context, identifier );
	if ( variable == NULL ) {
		event = outputf( Error, "va2s: variable '%s' not found", identifier );
		return event;
	}
	switch ( variable->type ) {
	case NarrativeVariable:
		event = outputf( Error, "va2s: unsupported variable type - '%s'", identifier );
		break;
	case StringVariable:
		; char *s = strdup( ((listItem *) variable->value )->ptr );
		context->output.slist = newItem( s );
		take_va( NULL, 0, context );
		break;
	case ExpressionVariable:
		; Expression *expression = ((listItem *) variable->value )->ptr;
		context->expression.mode = 0;
	        int retval = expression_solve( expression, 3, NULL, context );
		if ( retval < 0 ) break;
		take_va( context->expression.results, context->expression.mode, context );
		if ( context->expression.mode == ReadMode )
			freeLiterals( &context->expression.results );
		else freeListItem( &context->expression.results );
		break;
	case EntityVariable:
		take_va( variable->value, EvaluateMode, context );
		break;
	case LiteralVariable:
		take_va( variable->value, ReadMode, context );
		break;
	}
	return event;
}

static void
take_va( listItem *list, int mode, _context *context )
{
	// translate first list item into string
	if (( list )) {
		push_output( "", NULL, StringOutput, context );
		struct { int as_is; } backup;
		backup.as_is = context->output.as_is;
		context->output.as_is = 1;
		switch ( mode ) {
		case EvaluateMode:
			output_entity( list->ptr, 3, NULL );
			break;
		case ReadMode:
			output_literal( list->ptr );
			break;
		}
		context->output.as_is = backup.as_is;
		pop_output( context, 0 );
	}
	// assign first string result to context->identifier.id[ 2 ]
	if (( context->output.slist )) {
		listItem **slist = &context->output.slist;
		set_identifier( context, 2, popListItem( slist ) );
		for ( listItem *i = *slist; i!=NULL; i=i->next )
			free( i->ptr );
		freeListItem( slist );
	}
}

/*---------------------------------------------------------------------------
	command interface
---------------------------------------------------------------------------*/
static int va_op( int op, char *state, int event, char **next_state, _context * );
enum {
	Reset = 1,
	Assign,
	AssignNarrative,
	AssignFromVariable
};
int
reset_va( char *state, int event, char **next_state, _context *context )
	{ return va_op( Reset, state, event, next_state, context ); }
int
set_va( char *state, int event, char **next_state, _context *context )
	{ return va_op( Assign, state, event, next_state, context ); }
int
set_va_from_narrative( char *state, int event, char **next_state, _context *context )
	{ return va_op( AssignNarrative, state, event, next_state, context ); }
int
set_va_from_variable( char *state, int event, char **next_state, _context *context )
	{ return va_op( AssignFromVariable, state, event, next_state, context ); }

/*---------------------------------------------------------------------------
	va_op
---------------------------------------------------------------------------*/
static int
va_op( int op, char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;
	if ( context->expression.results == NULL )
		return output( Error, "cannot assign value to (null) results" );

	char *va_name = get_identifier( context, 2 );
	if ( registryLookup( CN.VB, va_name ) == NULL ) {
		return outputf( Error, "unknown value account name '%s'", va_name );
	}

	switch ( op ) {
	case Reset:
		for ( listItem *i = (listItem *) context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			cn_va_set( e, va_name, NULL );
		}
		break;
	case Assign:
#ifdef DEBUG
		outputf( Debug, "set_va: $[_]( %s ) : %s", va_name, get_identifier( context, 1 ) );
#endif
		; char *value = take_identifier( context, 1 );
		if ( !strcmp( va_name, "narratives" ) ) {
			free( value );
			return outputf( Error, "'%s' is not a narrative identifier", value );
		}
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			Entity *e = (Entity *) i->ptr;
			if (( i->next )) {
				cn_va_set( e, va_name, strdup(value) );
			}
			else cn_va_set( e, va_name, value );
		}
		break;
	case AssignNarrative:
#ifdef DEBUG
		outputf( Debug, "set_va: $[_]( %s ) : %s()", va_name, get_identifier( context, 1 ) );
#endif
		if ( strcmp( va_name, "narratives" ) ) {
			return outputf( Error, "'%s' value cannot be a narrative identifier", va_name );
		}
		// find the identified narrative in the list of registered narratives
		char *name = get_identifier( context, 1 );
		Narrative *narrative = lookupNarrative( CN.nil, name );
		if ( narrative == NULL ) {
			return outputf( Error, "narrative '%s' is not instantiated - cannot assign", name );
		}
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			// reset each of these entities' narratives value account to that single narrative
			Registry *registry = newRegistry( IndexedByName );
			registryRegister( registry, narrative->name, narrative );
			cn_va_set( e, va_name, registry );
			// register each of the passed entities to the narrative's list of entities
			addToNarrative( narrative, e );
		}
		break;
	case AssignFromVariable:
		if ( !strcmp( va_name, "narratives" ) ) {
			char *name = get_identifier( context, 1 );
			VariableVA *variable = lookupVariable( context, name );
			if (( variable == NULL ) || ( variable->type != NarrativeVariable )) {
				return outputf( Error, "variable '%s' is not a narrative variable", name );
			}
			Registry *registry = variable->value;
			Registry *narratives = newRegistry( IndexedByName );
			for ( registryEntry *r = registry->value; r!=NULL; r=r->next ) {
				Entity *e = r->index.address;
				char *n = r->value;
				Narrative *narrative = lookupNarrative( e, n );
				if (( narrative ))
					registryRegister( narratives, narrative->name, narrative );
			}
			if ( narratives->value == NULL ) {
				free( narratives );
				break;
			}
			for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
				Entity *e = (Entity *) i->ptr;
				for ( registryEntry *j = narratives->value; j!=NULL; j=j->next ) {
					addToNarrative((Narrative *) j->value, e );
				}
				if (( i->next )) {
					cn_va_set( e, va_name, registryDuplicate(narratives) );
				}
				else cn_va_set( e, va_name, narratives );
			}
		}
		else {
			set_identifier( context, 2, NULL );
			listItem *entities = context->expression.results;
			context->expression.results = NULL;
			va2s( state, event, next_state, context );
			char *value = take_identifier( context, 2 );
			if (( value )) {
				for ( listItem *i = entities; i!=NULL; i=i->next ) {
					Entity *e = (Entity *) i->ptr;
					if (( i->next )) {
						cn_va_set( e, va_name, strdup(value) );
					}
					else cn_va_set( e, va_name, value );
				}
			}
			freeListItem( &entities );
			free( va_name );
		}
		break;
	}
	return 0;
}
