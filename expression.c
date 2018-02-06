#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "input_util.h"
#include "output.h"
#include "output_util.h"
#include "expression.h"
#include "expression_util.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

static Expression *expression_set( char *state, _context *context );
static int test_super( char *state, int event, char **next_state, _context *context );

/*---------------------------------------------------------------------------
	execution engine
---------------------------------------------------------------------------*/
#define	do_( a, s )	\
	event = expression_execute( a, &state, event, s, context );

static int
expression_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval = 0;

	if ( action == push ) {
#ifdef DEBUG
		outputf( Debug, "expression_execute: pushing from level %d", context->command.level );
#endif
		Expression *e = ( context->command.mode == ExecutionMode ) ?
			expression_set( *state, context ) : NULL;

		event = push( *state, event, &next_state, context );
		*state = base;

		StackVA *stack = (StackVA *) context->command.stack->ptr;
		stack->expression.ptr = e;
	}
	else if ( !strcmp( next_state, pop_state ) ) {
		int popout = ( context->command.level == context->expression.level );
		if ( ( event != ']' ) && !popout )
			event = error( *state, event, &next_state, context );
		else {
#ifdef DEBUG
			if ( popout ) {
				outputf( Debug, "expression_execute: popping from base level %d", context->command.level );
			} else {
				outputf( Debug, "expression_execute: popping from level %d", context->command.level );
			}
#endif
			if ( context->command.mode == ExecutionMode ) {
				retval = action( *state, event, &next_state, context );
				StackVA *stack = (StackVA *) context->command.stack->ptr;
				expression_collapse( stack->expression.ptr );
			}
			if ( retval < 0 )
				event = retval;
			else if ( popout )
				*state = "";
			else {
				event = pop( *state, event, &next_state, context );
				*state = next_state;
			}
		}
	}
	else if ( !strcmp( next_state, "source-medium->target:" ) ) {
		// forbid instance specification in case super has been specified
		int retval = test_super( *state, event, &next_state, context );
		if ( retval < 0 )
			event = retval;
		else {
			event = action( *state, event, &next_state, context );
			if ( strcmp( next_state, same ) )
				*state = next_state;
		}
	}
	else {
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	// so that we can output the expression the way it was entered...
	if ( context->command.mode == ExecutionMode ) {
		if ( !strcmp( next_state, "target<" ) ) {
			StackVA *stack = (StackVA*) context->command.stack->ptr;
			Expression *expression = stack->expression.ptr;
			expression->result.twist = 8;
		}
		else if ( !strcmp( next_state, "source-" ) ) {
			StackVA *stack = (StackVA*) context->command.stack->ptr;
			Expression *expression = stack->expression.ptr;
			expression->result.twist = 16;
		}
	}
	return event;
}

/*---------------------------------------------------------------------------
	expression_set, _close, _abort, _resolve
---------------------------------------------------------------------------*/
static Expression *
expression_set( char *state, _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

	Expression *e = newExpression( context );
	bgn_
	in_( base )				sub[ 0 ].e = e;
	in_( "*" )				sub[ 0 ].e = e;
	in_( "_" )				sub[ 0 ].e = e;
	in_( "~" )				sub[ 0 ].e = e;
	in_( "target<-medium-" )		sub[ 0 ].e = e;
	in_( "target<-medium-*" )		sub[ 0 ].e = e;
	in_( "target<-medium-_" )		sub[ 0 ].e = e;
	in_( "target<-medium-~" )		sub[ 0 ].e = e;

	in_( "source-")				sub[ 1 ].e = e;
	in_( "source-*")			sub[ 1 ].e = e;
	in_( "source-~")			sub[ 1 ].e = e;
	in_( "source-_")			sub[ 1 ].e = e;
	in_( "target<-" )			sub[ 1 ].e = e;
	in_( "target<-*" )			sub[ 1 ].e = e;
	in_( "target<-_" )			sub[ 1 ].e = e;
	in_( "target<-~" )			sub[ 1 ].e = e;
	in_( ".")				sub[ 1 ].e = e;

	in_( ".." )				sub[ 2 ].e = e;
	in_( "source-medium->" )		sub[ 2 ].e = e;
	in_( "source-medium->*" )		sub[ 2 ].e = e;
	in_( "source-medium->_" )		sub[ 2 ].e = e;
	in_( "source-medium->~" )		sub[ 2 ].e = e;

	in_( "source-medium->target:" )		sub[ 3 ].e = e;
	in_( "source-medium->target: *" )	sub[ 3 ].e = e;
	in_( "source-medium->target: _" )	sub[ 3 ].e = e;
	in_( "source-medium->target: ~" )	sub[ 3 ].e = e;
	end

	return e;
}

static Expression *
take_expression( _context *context )
{
	StackVA *stack = context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	if ( expression->sub[ 0 ].result.none ) {
		free( expression );
		expression = NULL;
	} else if ( context->expression.marked ) {
		if ( trace_mark( expression ) ) {
			expression->result.marked = 1;
			context->expression.marked = 0;
		}
		if ( mark_negated( expression, 0 ) ) {
			output( Warning, "'?' inside of negative clause - will not yield result..." );
		}
	}
	freeExpressions( &stack->expression.collect );
	stack->expression.ptr = NULL;
	return expression;
}

static int
expression_abort( _context *context )
{
	context->expression.mode = ErrorMode;
	context->command.mode = FreezeMode;
	return 0;
}

/*---------------------------------------------------------------------------
	action utilities	- local
---------------------------------------------------------------------------*/
static int
reset_flags( _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->expression.flag.not = 0;
	stack->expression.flag.active = 0;
	stack->expression.flag.passive = 0;
	return 0;
}
static void
set_flags( StackVA *stack, Expression *expression, int count )
{
	expression->sub[ count ].result.not = stack->expression.flag.not;
	expression->sub[ count ].result.active = stack->expression.flag.active;
	expression->sub[ count ].result.passive = stack->expression.flag.passive;
	stack->expression.flag.not = 0;
	stack->expression.flag.active = 0;
	stack->expression.flag.passive = 0;
}

static int
set_sub_mark( int count, int event, _context *context )
{
	if (( count == 3 ) && ( event == ':' )) {
		StackVA *stack = (StackVA*) context->command.stack->ptr;
		if ( stack->expression.flag.not )
			return output( Error, "extraneous '~' in expression" );
		if ( stack->expression.flag.active )
			return output( Error, "extraneous '*' in expression" );
		if ( stack->expression.flag.passive )
			return output( Error, "extraneous '_' in expression" );
	}
	if ( context->expression.marked ) {
		return output( Error, "too many '?''s" );
	}
	context->expression.marked = 1;
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.any = 1;
	expression->sub[ count ].result.none = 0;
	expression->result.mark |= 1 << count;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_null( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = NullIdentifier;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_any( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	if ( stack->expression.flag.not ) {
		expression->sub[ count ].result.identifier.type = NullIdentifier;
		expression->sub[ count ].result.any = 0;
		expression->sub[ count ].result.none = 0;
		stack->expression.flag.not = 0;
		stack->expression.flag.active = 0;
		if ( stack->expression.flag.passive ) {
			return output( Error, "(nil) is always active" );
		}
	} else {
		expression->sub[ count ].result.any = 1;
		expression->sub[ count ].result.none = 0;
		set_flags( stack, expression, count );
	}
	return 0;
}

static int
set_sub_identifier( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = DefaultIdentifier;
	expression->sub[ count ].result.identifier.value = take_identifier( context, 1 );
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_collect( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	if ( stack->expression.collect == NULL )
		return expression_abort( context );

	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = ExpressionCollect;
	expression->sub[ count ].result.identifier.value = stack->expression.collect;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	stack->expression.collect = NULL;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_results( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	if ( context->expression.ptr == NULL )
		return expression_abort( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	if ( context->expression.mode == BuildMode ) {
		stack->expression.collect = newItem( context->expression.ptr );
		context->expression.ptr = NULL;
		return set_sub_collect( count, event, context );
	}
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;

	if ( context->expression.results == NULL ) {
		if ( stack->expression.flag.not ) {
			stack->expression.flag.not = 0;
			return set_sub_any( count, event, context );
		}
		else return expression_abort( context );
	}

	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type =
		( context->expression.mode == ReadMode ) ? LiteralResults : EntityResults;
	expression->sub[ count ].result.identifier.value = context->expression.results;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	context->expression.results = NULL;
	context->expression.mode = EvaluateMode;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variable_ref( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.value = take_identifier( context, 1 );
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variable( int count, int event, _context *context )
{
	if ( context->expression.mode == BuildMode )
		return set_sub_variable_ref( count, event, context );

	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	char *identifier = get_identifier( context, 1 );
	VariableVA *variable = lookupVariable( context, identifier );
	if ( variable == NULL ) {
#ifdef DEBUG
		outputf( Debug, "in expression: '%%%s' variable not found", identifier );
#endif
		return expression_abort( context );
	}
	listItem *value = getVariableValue( variable, context, 1 ); // duplicate

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	switch ( variable->type ) {
	case EntityVariable:
		expression->sub[ count ].result.identifier.type = EntityResults;
		expression->sub[ count ].result.identifier.value = value;
		break;
	case LiteralVariable:
		expression->sub[ count ].result.identifier.type = LiteralResults;
		expression->sub[ count ].result.identifier.value = value;
		break;
	case ExpressionVariable:
		expression->sub[ count ].result.identifier.type = ExpressionCollect;
		expression->sub[ count ].result.identifier.value = value;
		break;
	case StringVariable:
		expression->sub[ count ].result.identifier.type = StringResults;
		expression->sub[ count ].result.identifier.value = value;
		break;
	case NarrativeVariable:
		return outputf( Error, "'%%%s' is a narrative variable - not allowed in expressions", identifier );
	}
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_pop( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variator( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.value = variator_symbol;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_this( int count, int event, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.value = this_symbol;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
instance_solve( int event, _context *context )
{
	if (( event < 0 ) || ( context->command.mode != ExecutionMode ))
		return event;

	Expression *expression = take_expression( context );
	if ( expression == NULL ) {
		return expression_abort( context );
	}

	context->expression.mode = 0;
	event = expression_solve( expression, 3, NULL, context );
	context->expression.ptr = expression;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->expression.ptr = newExpression( context );
	return set_sub_results( 0, event, context );
}

/*---------------------------------------------------------------------------
	source actions
---------------------------------------------------------------------------*/
static int
set_source_pop( char *state, int event, char **next_state, _context *context )
	{ return set_sub_pop( 0, event, context ); }
static int
set_source_mark( char *state, int event, char **next_state, _context *context )
	{ return set_sub_mark( 0, event, context ); }
static int
set_source_null( char *state, int event, char **next_state, _context *context )
	{ return set_sub_null( 0, event, context ); }
static int
set_source_any( char *state, int event, char **next_state, _context *context )
	{ return set_sub_any( 0, event, context ); }
static int
set_source_identifier( char *state, int event, char **next_state, _context *context )
	{ return set_sub_identifier( 0, event, context ); }
static int
set_source_variable( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variable( 0, event, context ); }
static int
set_source_variator( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variator( 0, event, context ); }
static int
set_source_this( char *state, int event, char **next_state, _context *context )
	{ return set_sub_this( 0, event, context ); }
static int
set_source_results( char *state, int event, char **next_state, _context *context )
	{ return set_sub_results( 0, event, context ); }
static int
set_source_collect( char *state, int event, char **next_state, _context *context )
	{ return set_sub_collect( 0, event, context ); }

/*---------------------------------------------------------------------------
	medium actions
---------------------------------------------------------------------------*/
static int
set_medium_pop( char *state, int event, char **next_state, _context *context )
	{ return set_sub_pop( 1, event, context ); }
static int
set_medium_mark( char *state, int event, char **next_state, _context *context )
	{ return set_sub_mark( 1, event, context ); }
static int
set_medium_null( char *state, int event, char **next_state, _context *context )
	{ return set_sub_null( 1, event, context ); }
static int
set_medium_any( char *state, int event, char **next_state, _context *context )
	{ return set_sub_any( 1, event, context ); }
static int
set_medium_identifier( char *state, int event, char **next_state, _context *context )
	{ return set_sub_identifier( 1, event, context ); }
static int
set_medium_variable( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variable( 1, event, context ); }
static int
set_medium_variator( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variator( 1, event, context ); }
static int
set_medium_this( char *state, int event, char **next_state, _context *context )
	{ return set_sub_this( 1, event, context ); }
static int
set_medium_results( char *state, int event, char **next_state, _context *context )
	{ return set_sub_this( 1, event, context ); }
static int
set_medium_collect( char *state, int event, char **next_state, _context *context )
	{ return set_sub_collect( 1, event, context ); }

/*---------------------------------------------------------------------------
	target actions
---------------------------------------------------------------------------*/
static int
swap_source_target( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return set_sub_pop( 2, event, context );

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	memcpy( &expression->sub[ 2 ], &expression->sub[ 0 ], sizeof( ExpressionSub ) );
	memset( &expression->sub[ 0 ], 0, sizeof( ExpressionSub ) );
	expression->sub[ 0 ].result.none = 1;	// none meaning: none specified
	return set_sub_pop( 2, event, context );
}
static int
set_target_pop( char *state, int event, char **next_state, _context *context )
	{ return set_sub_pop( 2, event, context ); }
static int
set_target_mark( char *state, int event, char **next_state, _context *context )
	{ return set_sub_mark( 2, event, context ); }
static int
set_target_null( char *state, int event, char **next_state, _context *context )
	{ return set_sub_null( 2, event, context ); }
static int
set_target_any( char *state, int event, char **next_state, _context *context )
	{ return set_sub_any( 2, event, context ); }
static int
set_target_identifier( char *state, int event, char **next_state, _context *context )
	{ return set_sub_identifier( 2, event, context ); }
static int
set_target_variable( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variable( 2, event, context ); }
static int
set_target_variator( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variator( 2, event, context ); }
static int
set_target_this( char *state, int event, char **next_state, _context *context )
	{ return set_sub_this( 2, event, context ); }
static int
set_target_results( char *state, int event, char **next_state, _context *context )
	{ return set_sub_results( 2, event, context ); }
static int
set_target_collect( char *state, int event, char **next_state, _context *context )
	{ return set_sub_collect( 2, event, context ); }

/*---------------------------------------------------------------------------
	instance actions
---------------------------------------------------------------------------*/
static int
set_instance_pop( char *state, int event, char **next_state, _context *context )
	{ return set_sub_pop( 3, event, context ); }
static int
set_instance_mark( char *state, int event, char **next_state, _context *context )
	{ return set_sub_mark( 3, event, context ); }
static int
set_instance_null( char *state, int event, char **next_state, _context *context )
	{ return set_sub_null( 3, event, context ); }
static int
set_instance_any( char *state, int event, char **next_state, _context *context )
	{ return set_sub_any( 3, event, context ); }
static int
set_instance_identifier( char *state, int event, char **next_state, _context *context )
	{ return set_sub_identifier( 3, event, context ); }
static int
set_instance_variator( char *state, int event, char **next_state, _context *context )
	{ return set_sub_variator( 3, event, context ); }
static int
set_instance_this( char *state, int event, char **next_state, _context *context )
	{ return set_sub_this( 3, event, context ); }
static int
set_instance_variable( char *state, int event, char **next_state, _context *context )
{
	if ( context->expression.mode != BuildMode ) {
		event = set_sub_variable( 3, event, context );
		return instance_solve( event, context );
	}
	else return set_sub_variable_ref( 3, event, context );
}
static int
set_instance_results( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return reset_flags( context );

	if ( context->expression.ptr == NULL )
		return expression_abort( context );

	Expression *e = context->expression.ptr;
	if ( just_mark( e ) )
	/*
		optimization: in this case read_query() did not solve the query
	*/
	{
		StackVA *stack = (StackVA*) context->command.stack->ptr;
		Expression *expression = stack->expression.ptr;
		expression->result.as_sub = e->result.mark;
		set_flags( stack, expression, 3 );
		if ( expression->sub[ 3 ].result.not ) {
			expression->result.as_sub <<= 4;
			expression->sub[ 3 ].result.not = 0;
		}
		return 0;
	}
	else if  ( context->expression.mode != BuildMode ) {
		event = set_sub_results( 3, event, context );
		return instance_solve( event, context );
	}
	else return set_sub_results( 3, event, context );
}
static int
set_instance_collect( char *state, int event, char **next_state, _context *context )
	{ return set_sub_collect( 3, event, context ); }

/*---------------------------------------------------------------------------
	flag actions
---------------------------------------------------------------------------*/
static int
set_flag_not( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->command.stack->ptr;
	if ( stack->expression.flag.not ) {
		return output( Error, "redundant '~' in expression" );
	}
	stack->expression.flag.not = 1;
	return 0;
}
static int
set_flag_active( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->command.stack->ptr;
	if ( stack->expression.flag.not ) {
		return output( Error, "'*' must precede '~' in expression" );
	}
	if ( stack->expression.flag.passive ) {
		return output( Error, "conflicting '_' and '*' in expression" );
	}
	stack->expression.flag.active = 1;
	return 0;
}
static int
set_flag_passive( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->command.stack->ptr;
	if ( stack->expression.flag.not ) {
		return output( Error, "'_' must precede '~' in expression" );
	}
	if ( stack->expression.flag.active ) {
		return output( Error, "conflicting '*' and '_' in expression" );
	}
	stack->expression.flag.passive = 1;
	return 0;
}

/*---------------------------------------------------------------------------
	read_collect
---------------------------------------------------------------------------*/
static int
read_collect( char *state, int event, char **next_state, _context *context )
{
	asprintf( &state, "%s{_", state );

	// re-enter read_expression
	// ------------------------
	StackVA *stack = context->command.stack->ptr;
	listItem *collection = NULL;
	do {
		struct { Expression *expression; int mode; } backup;
		backup.expression = context->expression.ptr;
		backup.mode = context->expression.mode;
		context->expression.ptr = NULL;

		event = read_expression( base, 0, &same, context );
		if ( event < 0 )
			freeExpression( context->expression.ptr );
		else if (( event != ',' ) && ( event != '}' )) {
			event = error( state, event, next_state, context );
			freeExpression( context->expression.ptr );
		}
		else if (( context->command.mode == ExecutionMode ) && (context->expression.ptr))
			addItem( &collection, context->expression.ptr );

		context->expression.ptr = backup.expression;
		context->expression.mode = backup.mode;
	}
	while ( event == ',' );
	reorderListItem( &collection );
	stack->expression.collect = collection;

	free( state );
	return ( event < 0 ) ? event : 0;
}

/*---------------------------------------------------------------------------
	read_query
---------------------------------------------------------------------------*/
static int
read_query( char *state, int event, char **next_state, _context *context )
{
	asprintf( &state, "%s[_", state );

	// re-enter read_expression
	// ------------------------
	event = read_expression( base, 0, &same, context );
	if ( event < 0 )
		;
	else if ( event != ']' )
		event = error( state, event, next_state, context );
	else if (( context->command.mode == ExecutionMode ) &&
		 ( context->expression.mode != BuildMode ))
	{
		Expression *e = context->expression.ptr;
		int count = strcmp( state, "source-medium->target: %[_" ) ? 0 : 3;
		if (( count == 3 ) && just_mark( e ))
			; // optimization: set_instance_results() will check this condition
		else {
			context->expression.mode = EvaluateMode;
			event = expression_solve( e, 3, NULL, context );
		}
	}
	free( state );
	return ( event < 0 ) ? event : 0;
}

/*---------------------------------------------------------------------------
	set_shorty
---------------------------------------------------------------------------*/
#define EPUSH	int backup_event = event;
#define EPOP	if ( event >= 0 ) event = backup_event;
static int
set_shorty( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

	bgn_
	in_( ".." )		do_( nop, same )

	in_( "[_]." )		do_( set_source_pop, same )
	in_( "{_}." )		do_( set_source_collect, same )
	in_( "%[_]." )		do_( set_source_results, same )
	in_( "identifier." )	do_( set_source_identifier, same )
	in_( "%!." )		do_( set_source_this, same )
	in_( "%?." )		do_( set_source_variator, same )
	in_( "%variable." )	do_( set_source_variable, same )

	in_( ".[_]" )		do_( set_medium_pop, same )
	in_( ".{_}" )		do_( set_medium_collect, same )
	in_( ".%[_]" )		do_( set_medium_results, same )
	in_( ".identifier" )	do_( set_medium_identifier, same )
	in_( ".%?" )		do_( set_medium_variator, same )
	in_( ".%!" )		do_( set_medium_this, same )
	in_( ".%variable" )	do_( set_medium_variable, same )

	in_( "..[_]" )		EPUSH do_( set_target_pop, same ) EPOP
	in_( "..{_}" )		do_( set_target_collect, same )
	in_( "..%[_]" )		EPUSH do_( set_target_results, same ) EPOP
	in_( "..identifier" )	EPUSH do_( set_target_identifier, same ) EPOP
	in_( "..%?" )		EPUSH do_( set_target_variator, same ) EPOP
	in_( "..%!" )		EPUSH do_( set_target_this, same ) EPOP
	in_( "..%variable" )	EPUSH do_( set_target_variable, same ) EPOP
	end

	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].result.none ) {
			sub[ i ].result.any = 1;
			sub[ i ].result.none = 0;
		}
	}

	return event;
}

/*---------------------------------------------------------------------------
	set_mark
---------------------------------------------------------------------------*/
static int
set_mark( char *state, int event, char **next_state, _context *context )
{
	if ( context->expression.marked ) {
		return output( Error, "'?' too many question marks" );
	}
	context->expression.marked = 1;
	if ( context->command.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;

	bgn_
	in_( "?" ) 		expression->result.mark = 8;
				return ( event == ':' ) ? 0 : event;
	in_( "?." ) bgn_
		on_( '.' )	expression->result.mark = 1;
		on_( '?' )	expression->result.mark = 5;
		end
	in_( ".?" ) bgn_
		on_( '.' )	expression->result.mark = 2;
		on_( '?' )	expression->result.mark = 6;
		end
	in_( "??" ) bgn_
		on_( '.' )	expression->result.mark = 3;
		on_( '?' )	expression->result.mark = 7;
		end
	in_( ".." ) bgn_
		on_( '?' )	expression->result.mark = 4;
		end
	in_( "?|?." ) bgn_
		on_( '.' )	expression->result.mark = 9;
		on_( '?' )	expression->result.mark = 13;
		end
	in_( "?|.?" ) bgn_
		on_( '.' )	expression->result.mark = 10;
		on_( '?' )	expression->result.mark = 14;
		end
	in_( "?|??" ) bgn_
		on_( '.' )	expression->result.mark = 11;
		on_( '?' )	expression->result.mark = 15;
		end
	in_( "?|.." ) bgn_
		on_( '?' )	expression->result.mark = 12;
		end
	end

	return 0;
}

/*---------------------------------------------------------------------------
	set_super, test_super
---------------------------------------------------------------------------*/
static int
set_super( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;

	bgn_
	in_( "!." ) bgn_
		on_( '.' )	expression->result.as_sup = 1;
		on_( '!' )	expression->result.as_sup = 5;
		end
	in_( ".!" ) bgn_
		on_( '.' )	expression->result.as_sup = 2;
		on_( '!' )	expression->result.as_sup = 6;
		end
	in_( "!!" ) bgn_
		on_( '.' )	expression->result.as_sup = 3;
		on_( '!' )	expression->result.as_sup = 7;
		end
	in_( ".." ) bgn_
		on_( '!' )	expression->result.as_sup = 4;
		end
	end

	return 0;
}

static int
test_super( char *state, int event, char **next_state, _context *context )
{
	if ( context->command.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;
	char *super;
	switch ( expression->result.as_sup ) {
		case 0: return 0;
		case 1: super = "!.."; break;
		case 2: super = ".!."; break;
		case 3: super = "!!."; break;
		case 4: super = "..!"; break;
		case 5: super = "!.!"; break;
		case 6: super = ".!!"; break;
		case 7: super = "!!!"; break;
		default: return output( Debug, "***** Error: in test_super" );
	}
	return outputf( Error, "instance is not allowed after super ('%s') in expression", super );
}

/*---------------------------------------------------------------------------
	read_shorty
---------------------------------------------------------------------------*/
static int
read_shorty( char *state, int event, char **next_state, _context *context )
{
	do {
	event = read_input( state, event, &same, context );
#ifdef DEBUG
	outputf( Debug, "read_shorty: in \"%s\", on '%c'", state, event );
#endif
	bgn_
	in_( base ) bgn_
		on_( '?' )	do_( nop, "?" )
		on_( '!' )	do_( nop, "!" )
		end
	in_( "!" ) bgn_
		on_( '!' )	do_( nop, "!!" )
		on_( '.' )	do_( nop, "!." )
		on_other	do_( error, "" )
		end
		in_( "!!" ) bgn_
			on_( '!' )	do_( set_super, "!_" )
			on_( '.' )	do_( set_super, "!_" )
			on_other	do_( error, "" )
			end
		in_( "!." ) bgn_
			on_( '!' )	do_( set_super, "!_" )
			on_( '.' )	do_( set_super, "!_" )
			on_other	do_( error, "" )
			end
		in_( "!_" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( ':' )	do_( nop, base )
			on_other	do_( error, "" )
			end
	in_( "?" ) bgn_
		on_( ' ' )	do_( set_mark, "?_" )
		on_( '\t' )	do_( set_mark, "?_" )
		on_( ':' )	do_( set_mark, base )
		on_( ']' )	do_( set_mark, "" )
		on_( '-' )	do_( set_source_mark, "source-" )
		on_( '<' )	do_( set_target_mark, "target<" )
		on_( '|' )	do_( nop, "?|" )
		on_( '?' )	do_( nop, "??" )
		on_( '.' )	do_( nop, "?." )
		on_( '\n' )	do_( set_mark, "" )
		on_other	do_( error, "" )
		end
		in_( "?." ) bgn_
			on_( '?' )	do_( set_mark, "?_" )
			on_( '.' )	do_( set_mark, "?_" )
			on_other	do_( error, "" )
			end
		in_( "??" ) bgn_
			on_( '?' )	do_( set_mark, "?_" )
			on_( '.' )	do_( set_mark, "?_" )
			on_other	do_( error, "" )
			end
		in_( "?|" ) bgn_
			on_( '?' )	do_( nop, "?|?" )
			on_( '.' )	do_( nop, "?|." )
			on_other	do_( error, "" )
			end
			in_( "?|?" ) bgn_
				on_( '?' )	do_( nop, "?|??" )
				on_( '.' )	do_( nop, "?|?." )
				on_other	do_( error, "" )
				end
				in_( "?|??" ) bgn_
					on_( '?' )	do_( set_mark, "?_" )
					on_( '.' )	do_( set_mark, "?_" )
					on_other	do_( error, "" )
					end
				in_( "?|?." ) bgn_
					on_( '?' )	do_( set_mark, "?_" )
					on_( '.' )	do_( set_mark, "?_" )
					on_other	do_( error, "" )
					end
			in_( "?|." ) bgn_
				on_( '?' )	do_( nop, "?|.?" )
				on_( '.' )	do_( nop, "?|.." )
				on_other	do_( error, "" )
				end
				in_( "?|.?" ) bgn_
					on_( '?' )	do_( set_mark, "?_" )
					on_( '.' )	do_( set_mark, "?_" )
					on_other	do_( error, "" )
					end
				in_( "?|.." ) bgn_
					on_( '?' )	do_( set_mark, "?_" )
					on_other	do_( error, "" )
					end
		in_( "?_" ) bgn_
			on_( ' ' )	do_( nop, same )
			on_( '\t' )	do_( nop, same )
			on_( ':' )	do_( nop, base )
			on_( ']' )	do_( nothing, base )
			on_other	do_( nothing, "source-medium->target" )
			end
	in_( "identifier" ) bgn_
		on_( '.' )	do_( nop, "identifier." )
		end
		in_( "identifier." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "[_]" ) bgn_
		on_( '.' )	do_( nop, "[_]." )
		end
		in_( "[_]." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "%[_]" ) bgn_
		on_( '.' )	do_( nop, "%[_]." )
		end
		in_( "%[_]." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "{_}" ) bgn_
		on_( '.' )	do_( nop, "{_}." )
		end
		in_( "{_}." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "%!" ) bgn_
		on_( '.' )	do_( nop, "%!." )
		end
		in_( "%!." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "%variable" ) bgn_
		on_( '.' )	do_( nop, "%variable." )
		end
		in_( "%variable." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "." ) bgn_
		on_( '.' )	do_( nop, ".." )
		on_( '!' )	do_( nop, ".!" )
		on_( '?' )	do_( nop, ".?" )
		on_( '*' )	do_( set_flag_active, ".*" )
		on_( '_' )	do_( set_flag_passive, "._" )
		on_( '~' )	do_( set_flag_not, ".~" )
		on_( '%' )	do_( nop, ".%" )
		on_( '[' )	do_( push, ".[_]" )
		on_( '{' )	do_( read_collect, ".{_}" )
		on_other	do_( read_1, ".identifier" )
		end
		in_( ".*" ) bgn_
			on_( '~' )	do_( set_flag_not, ".~" )
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[_]" )
			on_( '{' )	do_( read_collect, ".{_}" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( "._" ) bgn_
			on_( '~' )	do_( set_flag_not, ".~" )
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[_]" )
			on_( '{' )	do_( read_collect, ".{_}" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( ".~" ) bgn_
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[_]" )
			on_( '{' )	do_( read_collect, ".{_}" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( ".[_]" ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
		in_( ".{_}" ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
		in_( ".identifier" ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
		in_( ".%" ) bgn_
			on_( '?' )	do_( nop, ".%?" )
			on_( '!' )	do_( nop, ".%!" )
			on_( '[' )	do_( read_query, ".%[_]" )
			on_other	do_( read_1, ".%variable" )
			end
			in_( ".%?" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
			in_( ".%!" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
			in_( ".%variable" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
			in_( ".%[_]" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
		in_( ".?" ) bgn_
			on_( '.' )	do_( set_mark, "?_" )
			on_( '?' )	do_( set_mark, "?_" )
			on_other	do_( error, "" )
			end
		in_( ".!" ) bgn_
			on_( '.' )	do_( set_super, "!_" )
			on_( '!' )	do_( set_super, "!_" )
			on_other	do_( error, "" )
			end
		in_( ".." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_( '?' )	do_( set_mark, "?_" )
			on_( '!' )	do_( set_super, "!_" )
			on_( '*' )	do_( set_flag_active, "..*" )
			on_( '_' )	do_( set_flag_passive, ".._" )
			on_( '~' )	do_( set_flag_not, "..~" )
			on_( '%' )	do_( nop, "..%" )
			on_( '[' )	do_( push, "..[_]" )
			on_( '{' )	do_( read_collect, "..{_}" )
			on_other	do_( read_1, "..identifier" )
			end
			in_( "..*" ) bgn_
				on_( '~' )	do_( set_flag_not, "..~" )
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( push, "..[_]" )
				on_( '{' )	do_( read_collect, "..{_}" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( ".._" ) bgn_
				on_( '~' )	do_( set_flag_not, "..~" )
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( push, "..[_]" )
				on_( '{' )	do_( read_collect, "..{_}" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( "..~" ) bgn_
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( push, "..[_]" )
				on_( '{' )	do_( read_collect, "..{_}" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( "..[_]" ) bgn_
				on_any		do_( set_shorty, "" )
				end
			in_( "..{_}" ) bgn_
				on_any		do_( set_shorty, "" )
				end
			in_( "..identifier" ) bgn_
				on_any		do_( set_shorty, "" )
				end
			in_( "..%" ) bgn_
				on_( '?' )	do_( nop, "..%?" )
				on_( '!' )	do_( nop, "..%!" )
				on_( '[' )	do_( read_query, "..%[_]" )
				on_other	do_( read_1, "..%variable" )
				end
				in_( "..%?" ) bgn_
					on_any	do_( set_shorty, "" )
					end
				in_( "..%!" ) bgn_
					on_any	do_( set_shorty, "" )
					end
				in_( "..%variable" ) bgn_
					on_any	do_( set_shorty, "" )
					end
				in_( "..%[_]" ) bgn_
					on_any	do_( set_shorty, "" )
					end
	end

	// return control to read_expression after push or when mark set or other conditions
	bgn_
		in_( base )			{ *next_state = state; state = ""; }
		in_( "source-" )		{ *next_state = state; state = ""; }
		in_( "target<" )		{ *next_state = state; state = ""; }
		in_( "source-medium->target" )	{ *next_state = state; state = ""; }
	end
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	expression_backup, expression_restore
---------------------------------------------------------------------------*/
typedef struct {
	struct {
		Expression *ptr;
		int level, marked, flags;
		struct {
			unsigned int active : 1;
			unsigned int passive : 1;
			unsigned int not : 1;
		} flag;
	} expression;
	struct {
		int mode;
	} command;
} ExpressionBackup;
	
static void
expression_backup( _context *context )
{
	StackVA *stack = (StackVA *) context->command.stack->ptr;

	ExpressionBackup *backup = malloc( sizeof( ExpressionBackup ) );
	backup->expression.ptr = stack->expression.ptr;
	backup->expression.flag.active = stack->expression.flag.active;
	backup->expression.flag.passive = stack->expression.flag.passive;
	backup->expression.flag.not = stack->expression.flag.not;
	backup->expression.level = context->expression.level;
	backup->expression.marked = context->expression.marked;
	backup->command.mode = context->command.mode;
	addItem( &context->expression.backup, backup );

	context->expression.level = context->command.level;
	context->expression.marked = 0;
	stack->expression.ptr = NULL;
	reset_flags( context );
}

static void
expression_restore( _context *context )
{
	ExpressionBackup *backup = popListItem( &context->expression.backup );
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->expression.ptr = backup->expression.ptr;
	stack->expression.flag.active = backup->expression.flag.active;
	stack->expression.flag.passive = backup->expression.flag.passive;
	stack->expression.flag.not = backup->expression.flag.not;
	context->expression.level = backup->expression.level;
	context->expression.marked = backup->expression.marked;
	context->command.mode = backup->command.mode;
}

/*---------------------------------------------------------------------------
	expression_init
---------------------------------------------------------------------------*/
static int
expression_init( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "expression_init: setting expression level to %d", context->command.level );
#endif
	expression_backup( context );
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	if ( context->command.mode == ExecutionMode )
	{
		StackVA *stack = (StackVA *) context->command.stack->ptr;
		stack->expression.ptr = newExpression( context );
		reset_flags( context );
	}
	return event;
}

/*---------------------------------------------------------------------------
	expression_exit
---------------------------------------------------------------------------*/
static int
expression_exit( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "expression_exit: on the way: returning event='%c' in mode=%d...", event, context->expression.mode );
#endif
	StackVA *stack = context->command.stack->ptr;
	Expression *expression = stack->expression.ptr;

	if (( event < 0 ) || ( context->expression.mode == ErrorMode )) {
		context->expression.mode = ErrorMode;
		while ( context->command.level != context->expression.level )
		{
			freeExpression( stack->expression.ptr );
			freeExpressions( &stack->expression.collect );
			pop( state, event, next_state, context );
		}
	}
	else if ( context->command.mode == ExecutionMode ) {
		context->expression.ptr = take_expression( context );
	}
	expression_restore( context );
	return event;
}

/*---------------------------------------------------------------------------
	read_expression
---------------------------------------------------------------------------*/
int
read_expression( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "read_expression: entering" );
#endif
	do_( expression_init, base )

	do {
	event = read_input( state, event, &same, context );
#ifdef DEBUG
	outputf( Debug, "read_expression: in \"%s\", on '%c'", state, event );
#endif
	bgn_
	// errors are handled in expression_exit
	on_( -1 )	do_( nothing, "" )

	// return control to read_shorty after pop
	in_( ".[_]" )	do_( read_shorty, "source-medium->target" )
	in_( "..[_]" )	do_( read_shorty, "source-medium->target" )

	in_( base ) bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '*' )	do_( set_flag_active, "*" )
		on_( '_' )	do_( set_flag_passive, "_" )
		on_( '~' )	do_( set_flag_not, "~" )
		on_( '[' )	do_( push, "[_]" )
		on_( '{' )	do_( read_collect, "{_}" )
		on_( '-' )	do_( set_source_null, "source-" )
		on_( '<' )	do_( set_target_null, "target<" )
		on_( '.' )	do_( nop, "." )
		on_( '%' )	do_( nop, "%" )
		on_( '?' )	do_( read_shorty, base )
		on_( '!' )	do_( read_shorty, base )
		on_separator	do_( nothing, pop_state )
		on_other	do_( read_1, "identifier" )
		end
		in_( "*" ) bgn_
			on_( '~' )	do_( set_flag_not, "~" )
			on_( '[' )	do_( push, "[_]" )
			on_( '{' )	do_( read_collect, "{_}" )
			on_( '.' )	do_( nop, "." )
			on_( '%' )	do_( nop, "%" )
			on_other	do_( read_1, "identifier" )
			end
		in_( "_" ) bgn_
			on_( '~' )	do_( set_flag_not, "~" )
			on_( '[' )	do_( push, "[_]" )
			on_( '{' )	do_( read_collect, "{_}" )
			on_( '.' )	do_( nop, "." )
			on_( '%' )	do_( nop, "%" )
			on_other	do_( read_1, "identifier" )
			end
		in_( "~" ) bgn_
			on_( ' ' )	do_( set_source_null, "source-medium->target" )
			on_( '\t' )	do_( set_source_null, "source-medium->target" )
			on_( ':' )	do_( set_source_null, "source-medium->target:" )
			on_( ' ' )	do_( nop, "~ " )
			on_( '\t' )	do_( nop, "~ " )
			on_( '[' )	do_( push, "[_]" )
			on_( '{' )	do_( read_collect, "{_}" )
			on_( '-' )	do_( set_source_null, "source-" )
			on_( '<' )	do_( set_target_null, "target<" )
			on_( '.' )	do_( nop, "~." )
			on_( '%' )	do_( nop, "%" )
			on_separator	do_( set_source_null, pop_state )
			on_other	do_( read_1, "identifier" )
			end
			in_( "~." ) bgn_
				on_( '-' )	do_( set_source_any, "source-" )
				on_( '<' )	do_( set_target_any, "target<" )
				on_( ':' )	do_( set_source_any, "source-medium->target:" )
				on_( ' ' )	do_( set_source_any, "source-medium->target" )
				on_( '\t' )	do_( set_source_any, "source-medium->target" )
				on_other	do_( set_source_any, pop_state )
				end
			in_( "~ " ) bgn_
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_other	do_( set_source_null, pop_state )
				end
		in_( "." ) bgn_
			on_( '-' )	do_( set_source_any, "source-" )
			on_( '<' )	do_( set_target_any, "target<" )
			on_( ':' )	do_( set_source_any, "source-medium->target:" )
			on_( ' ' )	do_( set_source_any, "source-medium->target" )
			on_( '\t' )	do_( set_source_any, "source-medium->target" )
			on_( '[' )	do_( read_shorty, "source-medium->target" )
			on_( '{' )	do_( read_shorty, "source-medium->target" )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_( '?' )	do_( read_shorty, "source-medium->target" )
			on_( '!' )	do_( read_shorty, "source-medium->target" )
			on_( '*' )	do_( read_shorty, "source-medium->target" )
			on_( '_' )	do_( read_shorty, "source-medium->target" )
			on_( '~' )	do_( read_shorty, "source-medium->target" )
			on_( '%' )	do_( read_shorty, "source-medium->target" )
			on_separator	do_( set_source_any, pop_state )
			on_other	do_( read_shorty, "source-medium->target" )
			end

		in_( "[_]" ) bgn_
			on_( '-' )	do_( set_source_pop, "source-" )
			on_( '<' )	do_( swap_source_target, "target<" )
			on_( ':' )	do_( set_source_pop, "source-medium->target:" )
			on_( ' ' )	do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	do_( set_source_pop, "source-medium->target" )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( set_source_pop, pop_state )
			end

		in_( "{_}" ) bgn_
			on_( '-' )	do_( set_source_collect, "source-" )
			on_( '<' )	do_( swap_source_target, "target<" )
			on_( ':' )	do_( set_source_collect, "source-medium->target:" )
			on_( ' ' )	do_( set_source_collect, "source-medium->target" )
			on_( '\t' )	do_( set_source_collect, "source-medium->target" )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( set_source_collect, pop_state )
			end

		in_( "identifier" ) bgn_
			on_( '-' )	do_( set_source_identifier, "source-" )
			on_( '<' )	do_( set_target_identifier, "target<" )
			on_( ':' )	do_( set_source_identifier, "source-medium->target:" )
			on_( ' ' )	do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_source_identifier, "source-medium->target" )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( set_source_identifier, pop_state )
			end

		in_( "%" ) bgn_
			on_( '!' )	do_( nop, "%!" )
			on_( '?' )	do_( nop, "%?" )
			on_( '[' )	do_( read_query, "%[_]" )
			on_other	do_( read_1, "%variable" )
			end
			in_( "%!" ) bgn_
				on_( '-' )	do_( set_source_this, "source-" )
				on_( '<' )	do_( set_target_this, "target<" )
				on_( ':' )	do_( set_source_this, "source-medium->target:" )
				on_( ' ' )	do_( set_source_this, "source-medium->target" )
				on_( '\t' )	do_( set_source_this, "source-medium->target" )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_this, pop_state )
				end
			in_( "%?" ) bgn_
				on_( '-' )	do_( set_source_variator, "source-" )
				on_( '<' )	do_( set_target_variator, "target<" )
				on_( ':' )	do_( set_source_variator, "source-medium->target:" )
				on_( ' ' )	do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	do_( set_source_variator, "source-medium->target" )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_variator, pop_state )
				end
			in_( "%[_]" ) bgn_
				on_( '-' )	do_( set_source_results, "source-" )
				on_( '<' )	do_( set_target_results, "target<" )
				on_( ':' )	do_( set_source_results, "source-medium->target:" )
				on_( ' ' )	do_( set_source_results, "source-medium->target" )
				on_( '\t' )	do_( set_source_results, "source-medium->target" )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_results, pop_state )
				end
			in_( "%variable" ) bgn_
				on_( '-' )	do_( set_source_variable, "source-" )
				on_( '<' )	do_( set_target_variable, "target<" )
				on_( ':' )	do_( set_source_variable, "source-medium->target:" )
				on_( ' ' )	do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	do_( set_source_variable, "source-medium->target" )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_variable, pop_state )
				end

	in_( "source-" ) bgn_
		on_( '*' )	do_( set_flag_active, "source-*" )
		on_( '_' )	do_( set_flag_passive, "source-_" )
		on_( '~' )	do_( set_flag_not, "source-~" )
		on_( '[' )	do_( push, "source-[_]" )
		on_( '{' )	do_( read_collect, "source-{_}" )
		on_( '-' )	do_( set_medium_null, "source-medium-" )
		on_( '.' )	do_( nop, "source-." )
		on_( '%' )	do_( nop, "source-%" )
		on_( '?' )	do_( nop, "source-?" )
		on_other	do_( read_1, "source-identifier" )
		end
		in_( "source-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-~" )
			on_( '[' )	do_( push, "source-[_]" )
			on_( '{' )	do_( read_collect, "source-{_}" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-~" )
			on_( '[' )	do_( push, "source-[_]" )
			on_( '{' )	do_( read_collect, "source-{_}" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-~" ) bgn_
			on_( '-' )	do_( set_medium_null, "source-medium-" )
			on_( '[' )	do_( push, "source-[_]" )
			on_( '{' )	do_( read_collect, "source-{_}" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-?" ) bgn_
			on_( '-' )	do_( set_medium_mark, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-[_]" ) bgn_
			on_( '-' )	do_( set_medium_pop, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-{_}" ) bgn_
			on_( '-' )	do_( set_medium_collect, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-." ) bgn_
			on_( '-' )	do_( set_medium_any, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-identifier" ) bgn_
			on_( '-' )	do_( set_medium_identifier, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-%" ) bgn_
			on_( '!' )	do_( nop, "source-%!" )
			on_( '?' )	do_( nop, "source-%?" )
			on_( '[' )	do_( read_query, "source-%[_]" )
			on_other	do_( read_1, "source-%variable" )
			end
			in_( "source-%!" ) bgn_
				on_( '-' )	do_( set_medium_this, "source-medium-" )
				on_other	do_( error, "" )
				end
			in_( "source-%?" ) bgn_
				on_( '-' )	do_( set_medium_variator, "source-medium-" )
				on_other	do_( error, "" )
				end
			in_( "source-%[_]" ) bgn_
				on_( '-' )	do_( set_medium_results, "source-medium-" )
				on_other	do_( error, "" )
				end
			in_( "source-%variable" ) bgn_
				on_( '-' )	do_( set_medium_variable, "source-medium-" )
				on_other	do_( error, "" )
				end

	in_( "source-medium-" ) bgn_
		on_( '>' )	do_( nop, "source-medium->" )
		on_other	do_( error, "" )
		end

	in_( "source-medium->" ) bgn_
		on_( '*' )	do_( set_flag_active, "source-medium->*" )
		on_( '_' )	do_( set_flag_passive, "source-medium->_" )
		on_( '~' )	do_( set_flag_not, "source-medium->~" )
		on_( ' ' )	do_( set_target_null, "source-medium->target" )
		on_( '\t' )	do_( set_target_null, "source-medium->target" )
		on_( '[' )	do_( push, "source-medium->[_]" )
		on_( '{' )	do_( read_collect, "source-medium->{_}" )
		on_( '%' )	do_( nop, "source-medium->%" )
		on_( '.' )	do_( set_target_any, "source-medium->target" )
		on_( '?' )	do_( set_target_mark, "source-medium->target" )
		on_( ':' )	do_( set_target_null, "source-medium->target:" )
		on_separator	do_( set_target_null, pop_state )
		on_other	do_( read_1, "source-medium->identifier" )
		end
		in_( "source-medium->*" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->~" )
			on_( '[' )	do_( push, "source-medium->[_]" )
			on_( '{' )	do_( read_collect, "source-medium->{_}" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->_" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->~" )
			on_( '[' )	do_( push, "source-medium->[_]" )
			on_( '{' )	do_( read_collect, "source-medium->{_}" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->~" ) bgn_
			on_( ' ' )	do_( set_target_null, "source-medium->target" )
			on_( '\t' )	do_( set_target_null, "source-medium->target" )
			on_( '[' )	do_( push, "source-medium->[_]" )
			on_( '{' )	do_( read_collect, "source-medium->{_}" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_( ':' )	do_( set_target_null, "source-medium->target:" )
			on_separator	do_( set_target_null, pop_state )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->[_]" ) bgn_
			on_( ' ' )	do_( set_target_pop, "source-medium->target" )
			on_( '\t' )	do_( set_target_pop, "source-medium->target" )
			on_( ':' )	do_( set_target_pop, "source-medium->target:" )
			on_other	do_( set_target_pop, pop_state )
			end
		in_( "source-medium->{_}" ) bgn_
			on_( ' ' )	do_( set_target_collect, "source-medium->target" )
			on_( '\t' )	do_( set_target_collect, "source-medium->target" )
			on_( ':' )	do_( set_target_collect, "source-medium->target:" )
			on_other	do_( set_target_collect, pop_state )
			end
		in_( "source-medium->identifier" ) bgn_
			on_( ' ' )	do_( set_target_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_target_identifier, "source-medium->target" )
			on_( ':' )	do_( set_target_identifier, "source-medium->target:" )
			on_other	do_( set_target_identifier, pop_state )
			end
		in_( "source-medium->%" ) bgn_
				on_( '!' )	do_( nop, "source-medium->%!" )
				on_( '?' )	do_( nop, "source-medium->%?" )
				on_( '[' )	do_( read_query, "source-medium->%[_]" )
				on_other	do_( read_1, "source-medium->%variable" )
				end
				in_( "source-medium->%!" ) bgn_
					on_( ' ' )	do_( set_target_this, "source-medium->target" )
					on_( '\t' )	do_( set_target_this, "source-medium->target" )
					on_( ':' )	do_( set_target_this, "source-medium->target:" )
					on_other	do_( set_target_this, pop_state )
					end
				in_( "source-medium->%?" ) bgn_
					on_( ' ' )	do_( set_target_variator, "source-medium->target" )
					on_( '\t' )	do_( set_target_variator, "source-medium->target" )
					on_( ':' )	do_( set_target_variator, "source-medium->target:" )
					on_other	do_( set_target_variator, pop_state )
					end
				in_( "source-medium->%[_]" ) bgn_
					on_( ' ' )	do_( set_target_results, "source-medium->target" )
					on_( '\t' )	do_( set_target_results, "source-medium->target" )
					on_( ':' )	do_( set_target_results, "source-medium->target:" )
					on_other	do_( set_target_results, pop_state )
					end
				in_( "source-medium->%variable" ) bgn_
					on_( ' ' )	do_( set_target_variable, "source-medium->target" )
					on_( '\t' )	do_( set_target_variable, "source-medium->target" )
					on_( ':' )	do_( set_target_variable, "source-medium->target:" )
					on_other	do_( set_target_variable, pop_state )
					end

	in_( "target<" ) bgn_
		on_( '-' )	do_( nop, "target<-" )
		on_other	do_( error, "" )
		end

	in_( "target<-" ) bgn_
		on_( '*' )	do_( set_flag_active, "target<-*" )
		on_( '_' )	do_( set_flag_passive, "target<-_" )
		on_( '~' )	do_( set_flag_not, "target<-~" )
		on_( '[' )	do_( push, "target<-[_]" )
		on_( '{' )	do_( read_collect, "target<-{_}" )
		on_( '-' )	do_( set_medium_null, "target<-medium-" )
		on_( '.' )	do_( set_medium_any, "target<-." )
		on_( '%' )	do_( nop, "target<-%" )
		on_( '?' )	do_( nop, "target<-?" )
		on_other	do_( read_1, "target<-identifier" )
		end
		in_( "target<-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-~" )
			on_( '[' )	do_( push, "target<-[_]" )
			on_( '{' )	do_( read_collect, "target<-{_}" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-~" )
			on_( '[' )	do_( push, "target<-[_]" )
			on_( '{' )	do_( read_collect, "target<-{_}" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-~" ) bgn_
			on_( '[' )	do_( push, "target<-[_]" )
			on_( '{' )	do_( read_collect, "target<-{_}" )
			on_( '-' )	do_( set_medium_null, "target<-medium-" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-?" ) bgn_
			on_( '-' )	do_( set_medium_mark, "target<-medium-" )
			on_other	do_( error, "" )
			end
		in_( "target<-[_]" ) bgn_
			on_( '-' )	do_( set_medium_pop, "target<-medium-" )
			on_other	do_( error, "" )
			end
		in_( "target<-{_}" ) bgn_
			on_( '-' )	do_( set_medium_collect, "target<-medium-" )
			on_other	do_( error, "" )
			end
		in_( "target<-." ) bgn_
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_( '-' )	do_( nop, "target<-.-" )
			on_other	do_( error, "" )
			end
		in_( "target<-identifier" ) bgn_
			on_( '-' )	do_( set_medium_identifier, "target<-medium-" )
			on_other	do_( error, "" )
			end
		in_( "target<-%" ) bgn_
			on_( '!' )	do_( nop, "target<-%!" )
			on_( '?' )	do_( nop, "target<-%?" )
			on_( '[' )	do_( read_query, "target<-%[_]" )
			on_other	do_( read_1, "target<-%variable" )
			end
			in_( "target<-%!" ) bgn_
				on_( '-' )	do_( set_medium_this, "target<-medium-" )
				on_other	do_( error, "" )
				end
			in_( "target<-%?" ) bgn_
				on_( '-' )	do_( set_medium_variator, "target<-medium-" )
				on_other	do_( error, "" )
				end
			in_( "target<-%[_]" ) bgn_
				on_( '-' )	do_( set_medium_results, "target<-medium-" )
				on_other	do_( error, "" )
				end
			in_( "target<-%variable" ) bgn_
				on_( '-' )	do_( set_medium_variable, "target<-medium-" )
				on_other	do_( error, "" )
				end

	in_( "target<-medium-" ) bgn_
		on_( '*' )	do_( set_flag_active, "target<-medium-*" )
		on_( '_' )	do_( set_flag_passive, "target<-medium-_" )
		on_( '~' )	do_( set_flag_not, "target<-medium-~" )
		on_( ' ' )	do_( set_source_null, "source-medium->target" )
		on_( '\t' )	do_( set_source_null, "source-medium->target" )
		on_( '[' )	do_( push, "target<-medium-[_]" )
		on_( '{' )	do_( read_collect, "target<-medium-{_}" )
		on_( '%' )	do_( nop, "target<-medium-%" )
		on_( '.' )	do_( set_source_any, "source-medium->target" )
		on_( '?' )	do_( set_source_mark, "source-medium->target" )
		on_( ':' )	do_( set_source_null, "source-medium->target:" )
		on_separator	do_( set_source_null, pop_state )
		on_other	do_( read_1, "target<-medium-identifier" )
		end
		in_( "target<-medium-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	do_( push, "target<-medium-[_]" )
			on_( '{' )	do_( read_collect, "target<-medium-{_}" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	do_( push, "target<-medium-[_]" )
			on_( '{' )	do_( read_collect, "target<-medium-{_}" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-~" ) bgn_
			on_( ' ' )	do_( set_source_null, "source-medium->target" )
			on_( '\t' )	do_( set_source_null, "source-medium->target" )
			on_( '[' )	do_( push, "target<-medium-[_]" )
			on_( '{' )	do_( read_collect, "target<-medium-{_}" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_( ':' )	do_( set_source_null, "source-medium->target:" )
			on_separator	do_( set_source_null, pop_state )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-[_]" ) bgn_
			on_( ' ' )	do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	do_( set_source_pop, "source-medium->target" )
			on_( ':' )	do_( set_source_pop, "source-medium->target:" )
			on_other	do_( set_source_pop, pop_state )
			end
		in_( "target<-medium-{_}" ) bgn_
			on_( ' ' )	do_( set_source_collect, "source-medium->target" )
			on_( '\t' )	do_( set_source_collect, "source-medium->target" )
			on_( ':' )	do_( set_source_collect, "source-medium->target:" )
			on_other	do_( set_source_collect, pop_state )
			end
		in_( "target<-medium-identifier" ) bgn_
			on_( ' ' )	do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_source_identifier, "source-medium->target" )
			on_( ':' )	do_( set_source_identifier, "source-medium->target:" )
			on_other	do_( set_source_identifier, pop_state )
			end
		in_( "target<-medium-%" ) bgn_
			on_( '!' )	do_( nop, "target<-medium-%!" )
			on_( '?' )	do_( nop, "target<-medium-%?" )
			on_( '[' )	do_( read_query, "target<-medium-%[_]" )
			on_other	do_( read_1, "target<-medium-%variable" )
			end
			in_( "target<-medium-%!" ) bgn_
				on_( ' ' )	do_( set_source_this, "source-medium->target" )
				on_( '\t' )	do_( set_source_this, "source-medium->target" )
				on_( ':' )	do_( set_source_this, "source-medium->target:" )
				on_other	do_( set_source_this, pop_state )
				end
			in_( "target<-medium-%?" ) bgn_
				on_( ' ' )	do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	do_( set_source_variator, "source-medium->target" )
				on_( ':' )	do_( set_source_variator, "source-medium->target:" )
				on_other	do_( set_source_variator, pop_state )
				end
			in_( "target<-medium-%[_]" ) bgn_
				on_( ' ' )	do_( set_source_results, "source-medium->target" )
				on_( '\t' )	do_( set_source_results, "source-medium->target" )
				on_( ':' )	do_( set_source_results, "source-medium->target:" )
				on_other	do_( set_source_results, pop_state )
				end
			in_( "target<-medium-%variable" ) bgn_
				on_( ' ' )	do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	do_( set_source_variable, "source-medium->target" )
				on_( ':' )	do_( set_source_variable, "source-medium->target:" )
				on_other	do_( set_source_variable, pop_state )
				end

	in_( "source-medium->target" ) bgn_
		on_( ':' )	do_( nop, "source-medium->target:" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( ']' )	do_( nop, pop_state )
		on_other	do_( nothing, pop_state )
		end

	in_( "source-medium->target:" ) bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '*' )	do_( set_flag_active, "source-medium->target: *" )
		on_( '_' )	do_( set_flag_passive, "source-medium->target: _" )
		on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
		on_( '[' )	do_( push, "source-medium->target: [_]" )
		on_( '{' )	do_( read_collect, "source-medium->target: {_}" )
		on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
		on_( '%' )	do_( nop, "source-medium->target: %" )
		on_separator	do_( error, "" )
		on_other	do_( read_1, "source-medium->target: identifier" )
		end
		in_( "source-medium->target: *" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	do_( push, "source-medium->target: [_]" )
			on_( '{' )	do_( read_collect, "source-medium->target: {_}" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: _" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	do_( push, "source-medium->target: [_]" )
			on_( '{' )	do_( read_collect, "source-medium->target: {_}" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: ~" ) bgn_
			on_( ' ' )	do_( set_instance_null, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_null, "source-medium->target: instance" )
			on_( '[' )	do_( push, "source-medium->target: [_]" )
			on_( '{' )	do_( read_collect, "source-medium->target: {_}" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_separator	do_( set_instance_null, pop_state )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: [_]" ) bgn_
			on_( ' ' )	do_( set_instance_pop, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_pop, "source-medium->target: instance" )
			on_other	do_( set_instance_pop, pop_state )
			end
		in_( "source-medium->target: {_}" ) bgn_
			on_( ' ' )	do_( set_instance_collect, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_collect, "source-medium->target: instance" )
			on_other	do_( set_instance_collect, pop_state )
			end
		in_( "source-medium->target: %" ) bgn_
			on_( '!' )	do_( nop, "source-medium->target: %!" )
			on_( '?' )	do_( nop, "source-medium->target: %?" )
			on_( '[' )	do_( read_query, "source-medium->target: %[_]" )
			on_other	do_( read_1, "source-medium->target: %variable" )
			end
			in_( "source-medium->target: %!" ) bgn_
				on_( ' ' )	do_( set_instance_this, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_this, "source-medium->target: instance" )
				on_other	do_( set_instance_this, pop_state )
				end
			in_( "source-medium->target: %?" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_instance_variator, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_variator, "source-medium->target: instance" )
				on_other	do_( set_instance_variator, pop_state )
				end
			in_( "source-medium->target: %[_]" ) bgn_
				on_( ' ' )	do_( set_instance_results, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_results, "source-medium->target: instance" )
				on_other	do_( set_instance_results, pop_state )
				end
			in_( "source-medium->target: %variable" ) bgn_
				on_( ' ' )	do_( set_instance_variable, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_variable, "source-medium->target: instance" )
				on_other	do_( set_instance_variable, pop_state )
				end
		in_( "source-medium->target: identifier" ) bgn_
			on_( ' ' )	do_( set_instance_identifier, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_identifier, "source-medium->target: instance" )
			on_other	do_( set_instance_identifier, pop_state )
			end

	in_( "source-medium->target: instance" ) bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_other	do_( nothing, pop_state )
		end
	end
	}
	while ( strcmp( state, "" ) );

	do_( expression_exit, same );

#ifdef DEBUG
	outputf( Debug, "exiting read_expression '%c'", event );
#endif
	return event;
}

