#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"
#include "input.h"
#include "input_util.h"

#include "api.h"
#include "variable.h"
#include "variable_util.h"
#include "expression_util.h"
#include "narrative_util.h"
#include "xl.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	command interface
---------------------------------------------------------------------------*/
static int assign_( int op, char *state, int event, char **next_state, _context * );
enum {
	ResetAll = 1,
	Reset,
	AssignToExpression,
	AssignToExpressionResults,
	AssignToLiteralValue,
	AssignToValueAccount,
	AssignToNarrative,
};
int
reset_variable( char *state, int event, char **next_state, _context *context )
	{ return assign_( Reset, state, event, next_state, context ); }
int
reset_variables( char *state, int event, char **next_state, _context *context )
	{ return assign_( ResetAll, state, event, next_state, context ); }
int
assign_results( char *state, int event, char **next_state, _context *context )
	{ return assign_( AssignToExpressionResults, state, event, next_state, context ); }
int
assign_literals( char *state, int event, char **next_state, _context *context )
	{ return assign_( AssignToLiteralValue, state, event, next_state, context ); }
int
assign_va( char *state, int event, char **next_state, _context *context )
	{ return assign_( AssignToValueAccount, state, event, next_state, context ); }
int
assign_narrative( char *state, int event, char **next_state, _context *context )
	{ return assign_( AssignToNarrative, state, event, next_state, context ); }
int
assign_expression( char *state, int event, char **next_state, _context *context )
	{ return assign_( AssignToExpression, state, event, next_state, context ); }

/*---------------------------------------------------------------------------
	assign_
---------------------------------------------------------------------------*/
static int
assign_( int op, char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return 0;

	AssignmentMode mode = context->assignment.mode;
	switch ( op ) {
	case ResetAll:
		resetVariables( context );
		break;
	case Reset:
		resetVariable( NULL, context );
		break;
	case AssignToExpression:
		; Expression *e = context->expression.ptr;
		context->expression.ptr = NULL;
		if (( e ) && e->sub[ 1 ].result.none && e->sub[ 3 ].result.none &&
		    ( e->sub[ 0 ].e == NULL ) && !flagged( e, 0 ))
		{
			ValueType type = e->sub[ 0 ].result.identifier.type;
			listItem *value = e->sub[ 0 ].result.identifier.value;
			switch ( type ) {
			case DefaultIdentifier:
			case NullIdentifier:
			case VariableIdentifier:
				assign_variable( mode, NULL, e, ExpressionValue, context );
				break;
			case ExpressionCollect:
				if ( value->next == NULL ) {
					assign_variable( mode, NULL, value->ptr, ExpressionValue, context );
					freeListItem( &value );
					break;
				}
				// no break
			default:
				assign_variable( mode, NULL, value, type, context );
				e->sub[ 0 ].result.identifier.value = NULL;
				freeExpression( e );
			}
		}
		else assign_variable( mode, NULL, e, ExpressionValue, context );
		break;
	case AssignToExpressionResults:
		; ValueType type = ( context->expression.mode == ReadMode ) ? LiteralResults : EntityResults;
		assign_variable( mode, NULL, context->expression.results, type, context );
		context->expression.results = NULL;
		break;
	case AssignToValueAccount:
		if ( strcmp( get_identifier( context, 2 ), "literal" ) )
			return output( Error, "currently only 'literal' values can be assigned to variables" );
		// no break
	case AssignToLiteralValue:
		// translate expression results into literals
		if (( context->expression.results ) && ( context->expression.mode != ReadMode ))
			E2L( &context->expression.results, NULL, context );
		assign_variable( mode, NULL, context->expression.results, LiteralResults, context );
		context->expression.results = NULL;
		break;
	case AssignToNarrative:
		; Registry *registry = NULL;
		if ( test_identifier( context, 1 ) && (context->expression.results) )
		{
			// fetch all entity narrative instances with passed name
			if ( context->expression.mode == ReadMode )
				L2E( &context->expression.results, NULL, context );

			char *narrative = get_identifier( context, 1 );
			registry = newRegistry( IndexedByAddress );
			for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
			{
				Entity *e = (Entity *) i->ptr;
				Narrative *n = lookupNarrative( e, narrative );
				if ( n != NULL )
					registryRegister( registry, e, n->name );
			}
			if ( registry->value == NULL )
				freeRegistry( &registry );
		}
		assign_variable( mode, NULL, registry, NarrativeIdentifiers, context );
		break;
	}
	context->assignment.mode = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	assign_occurrence_variable
---------------------------------------------------------------------------*/
void
assign_occurrence_variable( char *identifier, void *value, ValueType type, _context *context )
{
	if ( identifier == NULL ) return;
	if (( value )) {
		identifier = strdup( identifier );
		// create new or retrieve existing occurrence variable
		VariableVA *variable = fetchVariable( context, identifier, 1 );
		if ( variable->type ) {
			// variable already existed
			free( identifier );
		}
		setVariableValue( variable, value, type, context );
	}
	else resetVariable( identifier, context );
}

/*---------------------------------------------------------------------------
	assign_this_variable
---------------------------------------------------------------------------*/
void
assign_this_variable( Registry *variables, Entity *e )
{
	VariableVA *variable = newVariable( variables, this_symbol );
	variable->type = EntityVariable;
	variable->value = newItem(e);
}

/*---------------------------------------------------------------------------
	assign_variable
---------------------------------------------------------------------------*/
static int dereference( void *value, ValueType, char *identifier, _context * );

int
assign_variable( AssignmentMode mode, char **identifier, void *value, ValueType type, _context *context )
{
	char *take_default;
	if ( identifier == NULL ) {
		take_default = take_identifier( context, 0 );
		identifier = &take_default;
	}
	if ( *identifier == NULL )
		return -1;

	if (( value )) {
		// detect and replace variable reference(s) in expression(s)
		int retval = dereference( value, type, *identifier, context );
		if ( retval < 0 ) {
			if (( *identifier != variator_symbol ) && ( *identifier != this_symbol ))
				free( *identifier );
			return retval;
		}
		// create new or retrieve existing identified variable
		VariableVA *variable = fetchVariable( context, *identifier, 1 );
		if ( variable->type ) {
			// variable already existed - we can free the passed identifier
			if (( *identifier != variator_symbol ) && ( *identifier != this_symbol ))
				free( *identifier );
		}
		if (( *identifier != variator_symbol ) && ( *identifier != this_symbol )) {
			*identifier = NULL;
		}
		// perform assignment
		switch ( mode ) {
		case AssignSet:
			setVariableValue( variable, value, type, context );
			break;
		case AssignAdd:
			addVariableValue( variable, value, type, context );
			break;
		default:
			output( Debug, "***** Error: assign_variable: unknown assignment mode" );
		}
	}
	else	// no value passed
	{
		switch ( mode ) {
		case AssignSet:
			resetVariable( *identifier, context );
		default:
			break;
		}
		if (( *identifier != variator_symbol ) && ( *identifier != this_symbol ))
		{
			free( *identifier );
			*identifier = NULL;
		}
	}
	return 0;
}

static int count_references( Expression *, char *identifier, int count );
static int substitute( Expression *, char *identifier, VariableVA *, int count, _context *context );
static int
dereference( void *value, ValueType type, char *identifier, _context *context )
{
	VariableVA *variable;
	/*
	   get special cases out of the way
	   --------------------------------
	*/
	switch ( type ) {
	case NarrativeIdentifiers:
	case LiteralResults:
	case EntityResults:
	case StringResults:
		// values of these types cannot hold any variable references
		return 0;
	case ExpressionValue:
		variable = lookupVariable( context, identifier );
		if ( variable == NULL ) {
			return count_references( value, identifier, 0 ) ?
				output( Error, "referenced variable cannot be found" ) :
				0;
		}
		switch ( variable->type ) {
		case EntityVariable:
		case StringVariable:
		case LiteralVariable:
		case ExpressionVariable:
			break;
		case NarrativeVariable:
			return count_references( value, identifier, 0 ) ?
				output( Error, "referenced variable type (narratives) mismatch in expression" ) :
				0;
		}
		break;
	case ExpressionCollect:
		variable = lookupVariable( context, identifier );
		if ( variable == NULL ) {
			listItem *i;
			for ( i = value; i!=NULL; i=i->next )
				if ( count_references( i->ptr, identifier, 0 ) )
					break;
			return ( i == NULL ) ? 0 :
				output( Error, "referenced variable cannot be found" );
		}
		switch ( variable->type ) {
		case EntityVariable:
		case StringVariable:
		case LiteralVariable:
		case ExpressionVariable:
			break;
		case NarrativeVariable:
			return count_references( value, identifier, 0 ) ?
				output( Error, "referenced variable type (narratives) not supported in expression" ) :
				0;
		}
		break;
	default:
		// cf. command.c: replace variator value w/o changing type
		// variator value cannot hold variable references
		return 0;
	}

	/*
	   Now actually perform the substitution
	   -------------------------------------
	*/
	int count = 0;
	switch ( type ) {
	case ExpressionValue:
		count = count_references( value, identifier, 0 );
		if ( count ) {
			substitute( value, identifier, variable, count, context );
		}
		break;
	case ExpressionCollect:
		; listItem *refcounts = NULL;
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			count += count_references( i->ptr, identifier, 0 );
			// casting integer to (void *)
			union { void *ptr; int count; } data;
			data.ptr = NULL;
			data.count = count;
			addItem( &refcounts, data.ptr );
		}
		reorderListItem( &refcounts );
		for ( listItem *i = value; i!=NULL; i=i->next ) {
			int num = (int) popListItem( &refcounts );
			substitute( i->ptr, identifier, variable, num, context );
		}
		break;
	default: ;
		// impossible, but compiler does not know that
	}
	if ( count ) variable->value = NULL;
	return count;
}
static int
count_references( Expression *expression, char *identifier, int count )
{
	if ( expression == NULL )
		return count;

	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *name = sub[ i ].result.identifier.value;
			if (( name ) && !strcmp( name, identifier ) ) {
				count++;
			}
			break;
		default:
			if ( sub[ i ].result.any ) break;
			count = count_references( sub[ i ].e, identifier, count );
		}
	}
	return count;
}
static int
substitute( Expression *expression, char *identifier, VariableVA *variable, int count, _context *context )
/*
	Two optimizations here:
	1. no unnecessarily duplication of the original variable value
	2. direct substitution of the expression term in case of ExpressionVariable singleton
*/
{
	if (( expression == NULL ) || !count )
		return count;

	ExpressionSub *sub = expression->sub;
	int do_collapse = 0;
	for ( int i=0; i<4; i++ ) {
		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *name = sub[ i ].result.identifier.value;
			if (( name ) && !strcmp( name, identifier ) )
			{
				count--;
				int duplicate = count ? 1 : 0;
				if (( name != variator_symbol ) && ( name != this_symbol ))
					free( name );
				sub[ i ].result.identifier.value = NULL;
				switch ( variable->type ) {
				case EntityVariable:
					sub[ i ].result.identifier.type = EntityResults;
					sub[ i ].result.identifier.value = getVariableValue( variable, context, duplicate );
					// if only one entity could just put the name as DefaultIdentifier
					break;
				case LiteralVariable:
					sub[ i ].result.identifier.type = LiteralResults;
					sub[ i ].result.identifier.value = getVariableValue( variable, context, duplicate );
					// if only one literal could replace sub[ i ].e with literal value, and collapse
					break;
				case StringVariable:
					sub[ i ].result.identifier.type = StringResults;
					sub[ i ].result.identifier.value = getVariableValue( variable, context, duplicate );
					// could also convert to ExpressionCollect, and process as ExpressionVariable
					break;
				case ExpressionVariable:
					; listItem *expressions = getVariableValue( variable, context, duplicate );
					if ( expressions->next == NULL ) {
						sub[ i ].e = expressions->ptr;
						freeListItem( &expressions );
						do_collapse = 1;
					}
					else {
						sub[ i ].result.identifier.type = ExpressionCollect;
						sub[ i ].result.identifier.value = getVariableValue( variable, context, duplicate );
					}
					break;
				case NarrativeVariable:
					return output( Debug, "***** in substitute: argument is a NarrativeVariable!" );
				}
			}
			break;
		default:
			if ( sub[ i ].result.any ) break;
			count = substitute( sub[ i ].e, identifier, variable, count, context );
			if ( count < 0 ) return count;
		}
		if ( !count ) break;
	}
	if ( do_collapse ) {
		expression_collapse( expression );
	}
	return count;
}
