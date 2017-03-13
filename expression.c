#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "string_util.h"

#include "input.h"
#include "expression.h"
#include "expression_util.h"
#include "api.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	parser engine
---------------------------------------------------------------------------*/
#define	expression_do_( a, s )	\
	event = expression_execute( a, &state, event, s, context );

static Expression *
set_sub_expression( char *state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	Expression *e = calloc( 1, sizeof(Expression) );
	e->sub[ 3 ].result.any = 1;
	for ( int i=0; i<3; i++ ) {
		e->sub[ i ].result.none = 1;	// none meaning: none specified
	}

	bgn_
	in_( base )			expression->sub[ 0 ].e = e;
	in_( "*" )			expression->sub[ 0 ].e = e;
	in_( "_" )			expression->sub[ 0 ].e = e;
	in_( "~" )			expression->sub[ 0 ].e = e;
	in_( "target<-medium-" )	expression->sub[ 0 ].e = e;
	in_( "target<-medium-*" )	expression->sub[ 0 ].e = e;
	in_( "target<-medium-_" )	expression->sub[ 0 ].e = e;
	in_( "target<-medium-~" )	expression->sub[ 0 ].e = e;
	in_( "source-")			expression->sub[ 1 ].e = e;
	in_( "source-*")		expression->sub[ 1 ].e = e;
	in_( "source-~")		expression->sub[ 1 ].e = e;
	in_( "source-_")		expression->sub[ 1 ].e = e;
	in_( "target<-" )		expression->sub[ 1 ].e = e;
	in_( "target<-*" )		expression->sub[ 1 ].e = e;
	in_( "target<-_" )		expression->sub[ 1 ].e = e;
	in_( "target<-~" )		expression->sub[ 1 ].e = e;
	in_( ".")			expression->sub[ 1 ].e = e;
	in_( ".." )			expression->sub[ 2 ].e = e;
	in_( "source-medium->" )	expression->sub[ 2 ].e = e;
	in_( "source-medium->*" )	expression->sub[ 2 ].e = e;
	in_( "source-medium->_" )	expression->sub[ 2 ].e = e;
	in_( "source-medium->~" )	expression->sub[ 2 ].e = e;
	in_( "source-medium->target:" ) expression->sub[ 3 ].e = e;
	in_( "source-medium->target: *" ) expression->sub[ 3 ].e = e;
	in_( "source-medium->target: _" ) expression->sub[ 3 ].e = e;
	in_( "source-medium->target: ~" ) expression->sub[ 3 ].e = e;
	end

	return e;
}

static int
expression_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval = 0;

	if ( action == push )
	{
#ifdef DEBUG
		fprintf( stderr, "debug> parser: pushing from level %d\n", context->control.level );
#endif
		Expression *e = ( context->control.mode == ExecutionMode ) ?
			set_sub_expression( *state, context ) : NULL;

		event = push( *state, event, &next_state, context );
		*state = base;

		StackVA *stack = (StackVA *) context->control.stack->ptr;
		stack->expression.ptr = e;
	}
	else if ( !strcmp( next_state, pop_state ) )
	{
		if ( ( event != ']' ) && ( context->control.level != context->expression.level ) )
		{
			event = error( *state, event, &next_state, context );
		}
		else if ( context->control.level == context->expression.level )
		{
#ifdef DEBUG
			fprintf( stderr, "debug> parser: popping from base level %d\n", context->control.level );
#endif
			if ( context->control.mode == ExecutionMode ) {
				retval = action( *state, event, &next_state, context );
				StackVA *stack = (StackVA *) context->control.stack->ptr;
				expression_collapse( stack->expression.ptr );
				if ( retval < 0 ) context->expression.mode = ErrorMode;
			}
			*state = "";
		}
		else
		{
#ifdef DEBUG
			fprintf( stderr, "debug> parser: popping from level %d\n", context->control.level );
#endif
			if ( context->control.mode == ExecutionMode ) {
				retval = action( *state, event, &next_state, context );
				StackVA *stack = (StackVA *) context->control.stack->ptr;
				expression_collapse( stack->expression.ptr );
			}
			event = pop( *state, event, &next_state, context );
			if ( retval < 0 ) event = retval;
			*state = next_state;
		}
	}
	else
	{
		event = action( *state, event, &next_state, context );
		if ( strcmp( next_state, same ) )
			*state = next_state;
	}
	// just so that we can output the expression the way it was entered...
	if ( !strcmp( next_state, "target<" ) && ( context->control.mode == ExecutionMode ))
	{
		StackVA *stack = (StackVA*) context->control.stack->ptr;
		Expression *expression = stack->expression.ptr;
		expression->result.output_swap = 1;
	}
	return event;
}

static int
pop_all( char *state, int event, char **next_state, _context *context )
{
	while ( context->control.level != context->expression.level )
		pop( state, event, next_state, context );

	context->expression.mode = ErrorMode;
	return 0;
}

/*---------------------------------------------------------------------------
	utility functions
---------------------------------------------------------------------------*/
static int
reset_flags( _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	stack->expression.flags.not = 0;
	stack->expression.flags.active = 0;
	stack->expression.flags.inactive = 0;
	return 0;
}
static void
set_flags( StackVA *stack, Expression *expression, int count )
{
	expression->sub[ count ].result.not = stack->expression.flags.not;
	expression->sub[ count ].result.active = stack->expression.flags.active;
	expression->sub[ count ].result.inactive = stack->expression.flags.inactive;
	stack->expression.flags.not = 0;
	stack->expression.flags.active = 0;
	stack->expression.flags.inactive = 0;
}

static int
set_sub_mark( int count, int event, _context *context )
{
	if (( count == 3 ) && ( event == ':' )) {
		StackVA *stack = (StackVA*) context->control.stack->ptr;
		if ( stack->expression.flags.not )
			return log_error( context, event, "extraneous '~' in expression" );
		if ( stack->expression.flags.active )
			return log_error( context, event, "extraneous '*' in expression" );
		if ( stack->expression.flags.inactive )
			return log_error( context, event, "extraneous '_' in expression" );
	}
	if ( context->expression.mode == InstantiateMode ) {
		return log_error( context, event, "'?' not allowed in instantiation mode" );
	}
	if ( context->expression.marked ) {
		return log_error( context, event, "too many '?''s" );
	}
	context->expression.marked = 1;
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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
	if ( context->expression.mode == InstantiateMode ) {
		return log_error( context, event, "'.' not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	if ( stack->expression.flags.not ) {
		expression->sub[ count ].result.identifier.type = NullIdentifier;
		expression->sub[ count ].result.any = 0;
		expression->sub[ count ].result.none = 0;
		stack->expression.flags.not = 0;
		stack->expression.flags.active = 0;
		if ( stack->expression.flags.inactive ) {
			return log_error( context, event, "(nil) is always active" );
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
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode ) {
		return reset_flags( context );
	}

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.name = context->identifier.id[ 1 ].ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	context->identifier.id[ 1 ].ptr = NULL;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variable( int count, int event, _context *context )
{
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.name = context->identifier.id[ 1 ].ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	context->identifier.id[ 1 ].ptr = NULL;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_pop( int count, int event, _context *context )
{
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variator( int count, int event, _context *context )
{
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.name = variator_symbol;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_this( int count, int event, _context *context )
{
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.name = this_symbol;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

/*---------------------------------------------------------------------------
	source actions
---------------------------------------------------------------------------*/
static int
set_source_pop( char *state, int event, char **next_state, _context *context )
{
	return set_sub_pop( 0, event, context );
}
static int
set_source_mark( char *state, int event, char **next_state, _context *context )
{
	return set_sub_mark( 0, event, context );
	
}
static int
set_source_null( char *state, int event, char **next_state, _context *context )
{
	return set_sub_null( 0, event, context );
}
static int
set_source_any( char *state, int event, char **next_state, _context *context )
{
	return set_sub_any( 0, event, context );
}
static int
set_source_identifier( char *state, int event, char **next_state, _context *context )
{
	return set_sub_identifier( 0, event, context );
}
static int
set_source_variable( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variable( 0, event, context );
}
static int
set_source_variator( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variator( 0, event, context );
}
static int
set_source_this( char *state, int event, char **next_state, _context *context )
{
	return set_sub_this( 0, event, context );
}

/*---------------------------------------------------------------------------
	medium actions
---------------------------------------------------------------------------*/
static int
set_medium_pop( char *state, int event, char **next_state, _context *context )
{
	return set_sub_pop( 1, event, context );
}
static int
set_medium_mark( char *state, int event, char **next_state, _context *context )
{
	return set_sub_mark( 1, event, context );
}
static int
set_medium_null( char *state, int event, char **next_state, _context *context )
{
	return set_sub_null( 1, event, context );
}
static int
set_medium_any( char *state, int event, char **next_state, _context *context )
{
	return set_sub_any( 1, event, context );
}
static int
set_medium_identifier( char *state, int event, char **next_state, _context *context )
{
	return set_sub_identifier( 1, event, context );
}
static int
set_medium_variable( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variable( 1, event, context );
}
static int
set_medium_variator( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variator( 1, event, context );
}
static int
set_medium_this( char *state, int event, char **next_state, _context *context )
{
	return set_sub_this( 1, event, context );
}

/*---------------------------------------------------------------------------
	target actions
---------------------------------------------------------------------------*/
static int
swap_source_target( char *state, int event, char **next_state, _context *context )
{
	if ( context->control.mode != ExecutionMode )
		return set_sub_pop( 2, event, context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	bcopy( &expression->sub[ 0 ], &expression->sub[ 2 ], sizeof( ExpressionSub ) );
	bzero( &expression->sub[ 0 ], sizeof( ExpressionSub ) );
	expression->sub[ 0 ].result.none = 1;	// none meaning: none specified
	return set_sub_pop( 2, event, context );
}
static int
set_target_pop( char *state, int event, char **next_state, _context *context )
{
	return set_sub_pop( 2, event, context );
}
static int
set_target_mark( char *state, int event, char **next_state, _context *context )
{
	return set_sub_mark( 2, event, context );
}
static int
set_target_null( char *state, int event, char **next_state, _context *context )
{
	return set_sub_null( 2, event, context );
}
static int
set_target_any( char *state, int event, char **next_state, _context *context )
{
	return set_sub_any( 2, event, context );
}
static int
set_target_identifier( char *state, int event, char **next_state, _context *context )
{
	return set_sub_identifier( 2, event, context );
}
static int
set_target_variable( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variable( 2, event, context );
}
static int
set_target_variator( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variator( 2, event, context );
}
static int
set_target_this( char *state, int event, char **next_state, _context *context )
{
	return set_sub_this( 2, event, context );
}

/*---------------------------------------------------------------------------
	instance actions
---------------------------------------------------------------------------*/
static int
set_instance_pop( char *state, int event, char **next_state, _context *context )
{
	return set_sub_pop( 3, event, context );
}
static int
set_instance_mark( char *state, int event, char **next_state, _context *context )
{
	return set_sub_mark( 3, event, context );
}
static int
set_instance_null( char *state, int event, char **next_state, _context *context )
{
	return set_sub_null( 3, event, context );
}
static int
set_instance_any( char *state, int event, char **next_state, _context *context )
{
	return set_sub_any( 3, event, context );
}
static int
set_instance_identifier( char *state, int event, char **next_state, _context *context )
{
	return set_sub_identifier( 3, event, context );
}
static int
set_instance_variable( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variable( 3, event, context );
}
static int
set_instance_variator( char *state, int event, char **next_state, _context *context )
{
	return set_sub_variator( 3, event, context );
}
static int
set_instance_this( char *state, int event, char **next_state, _context *context )
{
	return set_sub_this( 3, event, context );
}

/*---------------------------------------------------------------------------
	flag actions
---------------------------------------------------------------------------*/
static int
set_flag_not( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->control.stack->ptr;
	if ( stack->expression.flags.not ) {
		return log_error( context, event, "redundant '~' in expression" );
	}
	stack->expression.flags.not = 1;
	return 0;
}
static int
set_flag_active( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->control.stack->ptr;
	if ( stack->expression.flags.not ) {
		return log_error( context, event, "'*' must precede '~' in expression" );
	}
	if ( stack->expression.flags.inactive ) {
		return log_error( context, event, "conflicting '_' and '*' in expression" );
	}
	stack->expression.flags.active = 1;
	return 0;
}
static int
set_flag_inactive( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->control.stack->ptr;
	if ( stack->expression.flags.not ) {
		return log_error( context, event, "'_' must precede '~' in expression" );
	}
	if ( stack->expression.flags.active ) {
		return log_error( context, event, "conflicting '*' and '_' in expression" );
	}
	stack->expression.flags.inactive = 1;
	return 0;
}

/*---------------------------------------------------------------------------
	test_mode_instantiate
---------------------------------------------------------------------------*/
static int
test_mode_instantiate( char *state, int event, char **next_state, _context *context )
{
	if (( context->expression.mode == InstantiateMode ) && ( event == ':' )) {
		return log_error( context, event, "instance designation not allowed in instantiation mode" );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	set_as_sub
---------------------------------------------------------------------------*/
static int
set_as_sub( char *state, int event, char **next_state, _context *context )
{
	if ( context->expression.mode == InstantiateMode ) {
		return log_error( context, event, "neither '?' nor '.' are allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;

	bgn_
	in_( "%[.." ) bgn_
		on_( '?' )	expression->result.as_sub = 4;
		end
	in_( "%[?." ) bgn_
		on_( '.' )	expression->result.as_sub = 1;
		on_( '?' )	expression->result.as_sub = 5;
		end
	in_( "%[.?" ) bgn_
		on_( '.' )	expression->result.as_sub = 2;
		on_( '?' )	expression->result.as_sub = 6;
		end
	in_( "%[??" ) bgn_
		on_( '.' )	expression->result.as_sub = 3;
		on_( '?' )	expression->result.as_sub = 7;
		end
	end

	set_flags( stack, expression, 3 );
	if ( expression->sub[ 3 ].result.not ) {
		expression->result.as_sub <<= 4;
		expression->sub[ 3 ].result.not = 0;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	read_as_sub
---------------------------------------------------------------------------*/
static int
read_as_sub( char *state, int event, char **next_state, _context *context )
{
	event = 0;
	do
	{
	event = input( state, event, NULL, context );
#ifdef DEBUG
	fprintf( stderr, "debug> read_as_sub: in \"%s\", on '%c'\n", state, event );
#endif

	bgn_
	in_( "%" ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( '.' )	expression_do_( nop, "%[." )
		on_( '?' )	expression_do_( nop, "%[?" )
		on_other	expression_do_( error, "" )
		end
	in_( "source-medium->target: %" ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( '.' )	expression_do_( nop, "%[." )
		on_( '?' )	expression_do_( nop, "%[?" )
		on_other	expression_do_( error, "" )
		end
		in_( "%[." ) bgn_
			on_( '.' )	expression_do_( nop, "%[.." )
			on_( '?' )	expression_do_( nop, "%[.?" )
			on_other	expression_do_( error, "" )
			end
			in_( "%[.." ) bgn_
				on_( '?' )	expression_do_( set_as_sub, "%[_" )
				on_other	expression_do_( error, "" )
				end
			in_( "%[.?" ) bgn_
				on_( '.' )	expression_do_( set_as_sub, "%[_" )
				on_( '?' )	expression_do_( set_as_sub, "%[_" )
				on_other	expression_do_( error, "" )
				end
		in_( "%[?" ) bgn_
			on_( '.' )	expression_do_( nop, "%[?." )
			on_( '?' )	expression_do_( nop, "%[??" )
			on_other	expression_do_( error, "" )
			end
			in_( "%[?." ) bgn_
				on_( '.' )	expression_do_( set_as_sub, "%[_" )
				on_( '?' )	expression_do_( set_as_sub, "%[_" )
				on_other	expression_do_( error, "" )
				end
			in_( "%[??" ) bgn_
				on_( '.' )	expression_do_( set_as_sub, "%[_" )
				on_( '?' )	expression_do_( set_as_sub, "%[_" )
				on_other	expression_do_( error, "" )
				end
		in_( "%[_" ) bgn_
			on_( ' ' )		expression_do_( nop, same )
			on_( '\t' )		expression_do_( nop, same )
			on_( ']' )		expression_do_( nop, "" )
			on_other		expression_do_( error, "" )
			end
	end
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	set_shorty
---------------------------------------------------------------------------*/
#define EPUSH	int backup_event = event;
#define EPOP	if ( event >= 0 ) event = backup_event;
static int
set_shorty( char *state, int event, char **next_state, _context *context )
{
	if ( context->expression.mode == InstantiateMode ) {
		return log_error( context, event, "neither '?' nor '.' are allowed in instantiation mode" );
	}
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

	bgn_
	in_( ".." )		expression_do_( nop, same )

	in_( "[]." )		expression_do_( set_source_pop, same )
	in_( "identifier." )	expression_do_( set_source_identifier, same )
	in_( "%%." )		expression_do_( set_source_this, same )
	in_( "%?." )		expression_do_( set_source_variator, same )
	in_( "%identifier." )	expression_do_( set_source_variable, same )

	in_( ".[]" )		expression_do_( set_medium_pop, same )
	in_( ".identifier" )	expression_do_( set_medium_identifier, same )
	in_( ".%?" )		expression_do_( set_medium_variator, same )
	in_( ".%%" )		expression_do_( set_medium_this, same )
	in_( ".%identifier" )	expression_do_( set_medium_variable, same )

	in_( "..[]" )		EPUSH expression_do_( set_target_pop, same ) EPOP
	in_( "..identifier" )	EPUSH expression_do_( set_target_identifier, same ) EPOP
	in_( "..%?" )		EPUSH expression_do_( set_target_variator, same ) EPOP
	in_( "..%%" )		EPUSH expression_do_( set_target_this, same ) EPOP
	in_( "..%identifier" )	EPUSH expression_do_( set_target_variable, same ) EPOP
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
	if ( context->expression.mode == InstantiateMode ) {
		return log_error( context, event, "neither '?' nor '.' are allowed in instantiation mode" );
	}
	if ( context->expression.marked ) {
		return log_error( context, event, "'?' too many question marks" );
	}
	context->expression.marked = 1;
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

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
	read_shorty
---------------------------------------------------------------------------*/
static int
read_shorty( char *state, int event, char **next_state, _context *context )
{
	do {
	event = input( state, event, NULL, context );
#ifdef DEBUG
	fprintf( stderr, "debug> read_shorty: in \"%s\", on '%c'\n", state, event );
#endif

	bgn_
	in_( base ) bgn_
		on_( '?' )	expression_do_( nop, "?" )
		end
	in_( "?" ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( ':' )	expression_do_( set_mark, base )
		on_( ']' )	expression_do_( set_mark, "" )
		on_( '-' )	expression_do_( set_source_mark, "source-" )
		on_( '<' )	expression_do_( set_target_mark, "target<" )
		on_( '|' )	expression_do_( nop, "?|" )
		on_( '?' )	expression_do_( nop, "??" )
		on_( '.' )	expression_do_( nop, "?." )
		on_other	expression_do_( error, "" )
		end
		in_( "?." ) bgn_
			on_( '?' )	expression_do_( set_mark, "?_" )
			on_( '.' )	expression_do_( set_mark, "?_" )
			on_other	expression_do_( error, "" )
			end
		in_( "??" ) bgn_
			on_( '?' )	expression_do_( set_mark, "?_" )
			on_( '.' )	expression_do_( set_mark, "?_" )
			on_other	expression_do_( error, "" )
			end
		in_( "?|" ) bgn_
			on_( '?' )	expression_do_( nop, "?|?" )
			on_( '.' )	expression_do_( nop, "?|." )
			on_other	expression_do_( error, "" )
			end
			in_( "?|?" ) bgn_
				on_( '?' )	expression_do_( nop, "?|??" )
				on_( '.' )	expression_do_( nop, "?|?." )
				on_other	expression_do_( error, "" )
				end
				in_( "?|??" ) bgn_
					on_( '?' )	expression_do_( set_mark, "?_" )
					on_( '.' )	expression_do_( set_mark, "?_" )
					on_other	expression_do_( error, "" )
					end
				in_( "?|?." ) bgn_
					on_( '?' )	expression_do_( set_mark, "?_" )
					on_( '.' )	expression_do_( set_mark, "?_" )
					on_other	expression_do_( error, "" )
					end
			in_( "?|." ) bgn_
				on_( '?' )	expression_do_( nop, "?|.?" )
				on_( '.' )	expression_do_( nop, "?|.." )
				on_other	expression_do_( error, "" )
				end
				in_( "?|.?" ) bgn_
					on_( '?' )	expression_do_( set_mark, "?_" )
					on_( '.' )	expression_do_( set_mark, "?_" )
					on_other	expression_do_( error, "" )
					end
				in_( "?|.." ) bgn_
					on_( '?' )	expression_do_( set_mark, "?_" )
					on_other	expression_do_( error, "" )
					end
		in_( "?_" ) bgn_
			on_( ' ' )	expression_do_( nop, same )
			on_( '\t' )	expression_do_( nop, same )
			on_( ':' )	expression_do_( nop, base )
			on_( ']' )	expression_do_( nothing, base )
			on_other	expression_do_( nothing, "" )
			end
	in_( "[]" ) bgn_
		on_( '.' )	expression_do_( nop, "[]." )
		end
		in_( "[]." ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
	in_( "identifier" ) bgn_
		on_( '.' )	expression_do_( nop, "identifier." )
		end
		in_( "identifier." ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
	in_( "%%" ) bgn_
		on_( '.' )	expression_do_( nop, "%%." )
		end
		in_( "%%." ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
	in_( "%identifier" ) bgn_
		on_( '.' )	expression_do_( nop, "%identifier." )
		end
		in_( "%identifier." ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
	in_( "." ) bgn_
		on_( '.' )	expression_do_( nop, ".." )
		on_( '?' )	expression_do_( nop, ".?" )
		on_( '*' )	expression_do_( set_flag_active, ".*" )
		on_( '_' )	expression_do_( set_flag_inactive, "._" )
		on_( '~' )	expression_do_( set_flag_not, ".~" )
		on_( '%' )	expression_do_( nop, ".%" )
		on_( '[' )	expression_do_( push, ".[]" )
		on_other	expression_do_( read_argument, ".identifier" )
		end
		in_( ".*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, ".~" )
			on_( '%' )	expression_do_( nop, ".%" )
			on_( '[' )	expression_do_( push, ".[]" )
			on_other	expression_do_( read_argument, ".identifier" )
			end
		in_( "._" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, ".~" )
			on_( '%' )	expression_do_( nop, ".%" )
			on_( '[' )	expression_do_( push, ".[]" )
			on_other	expression_do_( read_argument, ".identifier" )
			end
		in_( ".~" ) bgn_
			on_( '%' )	expression_do_( nop, ".%" )
			on_( '[' )	expression_do_( push, ".[]" )
			on_other	expression_do_( read_argument, ".identifier" )
			end
		in_( ".[]" ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
		in_( ".identifier" ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_other	expression_do_( error, "" )
			end
		in_( ".%" ) bgn_
			on_( '?' )	expression_do_( nop, ".%?" )
			on_( '%' )	expression_do_( nop, ".%%" )
			on_other	expression_do_( read_argument, ".%identifier" )
			end
			in_( ".%?" ) bgn_
				on_( '.' )	expression_do_( set_shorty, "" )
				on_other	expression_do_( error, "" )
				end
			in_( ".%%" ) bgn_
				on_( '.' )	expression_do_( set_shorty, "" )
				on_other	expression_do_( error, "" )
				end
			in_( ".%identifier" ) bgn_
				on_( '.' )	expression_do_( set_shorty, "" )
				on_other	expression_do_( error, "" )
				end
		in_( ".?" ) bgn_
			on_( '.' )	expression_do_( set_mark, "?_" )
			on_( '?' )	expression_do_( set_mark, "?_" )
			on_other	expression_do_( error, "" )
			end
		in_( ".." ) bgn_
			on_( '.' )	expression_do_( set_shorty, "" )
			on_( '?' )	expression_do_( set_mark, "?_" )
			on_( '*' )	expression_do_( set_flag_active, "..*" )
			on_( '_' )	expression_do_( set_flag_inactive, ".._" )
			on_( '~' )	expression_do_( set_flag_not, "..~" )
			on_( '%' )	expression_do_( nop, "..%" )
			on_( '[' )	expression_do_( push, "..[]" )
			on_other	expression_do_( read_argument, "..identifier" )
			end
			in_( "..*" ) bgn_
				on_( '~' )	expression_do_( set_flag_not, "..~" )
				on_( '%' )	expression_do_( nop, "..%" )
				on_( '[' )	expression_do_( push, "..[]" )
				on_other	expression_do_( read_argument, "..identifier" )
				end
			in_( ".._" ) bgn_
				on_( '~' )	expression_do_( set_flag_not, "..~" )
				on_( '%' )	expression_do_( nop, "..%" )
				on_( '[' )	expression_do_( nop, "..[]" )
				on_other	expression_do_( read_argument, "..identifier" )
				end
			in_( "..~" ) bgn_
				on_( '%' )	expression_do_( nop, "..%" )
				on_( '[' )	expression_do_( nop, "..[]" )
				on_other	expression_do_( read_argument, "..identifier" )
				end
			in_( "..[]" ) bgn_
				on_any		expression_do_( set_shorty, "" )
				end
			in_( "..identifier" ) bgn_
				on_any		expression_do_( set_shorty, "" )
				end
			in_( "..%" ) bgn_
				on_( '?' )	expression_do_( nop, "..%?" )
				on_( '%' )	expression_do_( nop, "..%%" )
				on_other	expression_do_( read_argument, "..%identifier" )
				end
				in_( "..%?" ) bgn_
					on_any	expression_do_( set_shorty, "" )
					end
				in_( "..%%" ) bgn_
					on_any	expression_do_( set_shorty, "" )
					end
				in_( "..%identifier" ) bgn_
					on_any	expression_do_( set_shorty, "" )
					end
		end

		// return control to parse_expression after push or when mark set
		if (( state == base ) || !strcmp( state, "source-" ) || !strcmp( state, "target<" ))
			{ *next_state = state; state = ""; }
	}
	while ( strcmp( state, "" ) );

	return event;
}

/*---------------------------------------------------------------------------
	parser_init
---------------------------------------------------------------------------*/
static int
parser_init( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> parser_init: setting parser level to %d\n", context->control.level );
#endif
	context->expression.level = context->control.level;
	context->expression.marked = 0;
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	if ( context->control.mode != ExecutionMode )
		return event;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Expression *expression = (Expression *) calloc( 1, sizeof(Expression) );
	expression->sub[ 3 ].result.any = 1;
	for ( int i=0; i<3; i++ )
		expression->sub[ i ].result.none = 1;	// none meaning: none specified

	stack->expression.ptr = expression;
	stack->expression.flags.not = 0;
	stack->expression.flags.active = 0;
	stack->expression.flags.inactive = 0;

	return event;

}

/*---------------------------------------------------------------------------
	parser_exit
---------------------------------------------------------------------------*/
static int
parser_exit( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> parser_exit: on the way: returning %d, in mode %d...\n", event, context->expression.mode );
#endif
	if (( event < 0 ) || ( context->expression.mode == ErrorMode ))
		pop_all( state, event, next_state, context );

	if ( context->control.mode != ExecutionMode )
		return event;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	if ( context->expression.mode == ErrorMode )
	{
		freeExpression( expression );
		context->expression.ptr = NULL;
	}
	else if ( expression != NULL ) {
		if ( expression->sub[ 0 ].result.none ) {
			free( expression );
			expression = NULL;
		} else {
			context->expression.ptr = expression;
			expression->result.no_mark = !context->expression.marked;
			if ( !expression->result.no_mark ) {
				trace_mark( expression );
				if ( mark_negated( expression, 0 ) ) {
					fprintf( stderr, "consensus: Warning: '?' inside of negative clause - will not yield result...\n" );
				}
			}
		}
	}
	stack->expression.ptr = NULL;
	return event;
}

/*---------------------------------------------------------------------------
	parse_expression
---------------------------------------------------------------------------*/
int
parse_expression( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	fprintf( stderr, "debug> parser: entering\n" );
#endif
	bgn_
	in_( "!. %" )			expression_do_( parser_init, "%" )
	in_( ": identifier : %" )	expression_do_( parser_init, "%" )
	in_other 			expression_do_( parser_init, base )
	end

	do {
	event = input( state, event, NULL, context );
#ifdef DEBUG
	fprintf( stderr, "debug> parse_expression: in \"%s\", on '%c'\n", state, (event==-1)?'E':event );
#endif

	bgn_
	// errors are handled in parser_exit
	on_( -1 )	expression_do_( nothing, "" )

	// return control to read_shorty after pop
	in_( ".[]" )	expression_do_( read_shorty, "source-medium->target" )
	in_( "..[]" )	expression_do_( read_shorty, "source-medium->target" )

	in_( base ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( '*' )	expression_do_( set_flag_active, "*" )
		on_( '_' )	expression_do_( set_flag_inactive, "_" )
		on_( '~' )	expression_do_( set_flag_not, "~" )
		on_( ']' )	expression_do_( nothing, pop_state )
		on_( '[' )	expression_do_( push, "[]" )
		on_( '-' )	expression_do_( set_source_null, "source-" )
		on_( '<' )	expression_do_( set_target_null, "target<" )
		on_( '.' )	expression_do_( nop, "." )
		on_( '%' )	expression_do_( nop, "%" )
		on_( '?' )	expression_do_( read_shorty, base )
		on_separator	expression_do_( nothing, pop_state )
		on_other	expression_do_( read_argument, "identifier" )
		end
		in_( "*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "~" )
			on_( '[' )	expression_do_( push, "[]" )
			on_( '.' )	expression_do_( nop, "." )
			on_( '%' )	expression_do_( nop, "%" )
			on_other	expression_do_( read_argument, "identifier" )
			end
		in_( "_" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "~" )
			on_( '[' )	expression_do_( push, "[]" )
			on_( '.' )	expression_do_( nop, "." )
			on_( '%' )	expression_do_( nop, "%" )
			on_other	expression_do_( read_argument, "identifier" )
			end
		in_( "~" ) bgn_
			on_( ' ' )	expression_do_( nop, "~ " )
			on_( '\t' )	expression_do_( nop, "~ " )
			on_( ']' )	expression_do_( set_source_null, pop_state )
			on_( '[' )	expression_do_( push, "[]" )
			on_( '-' )	expression_do_( set_source_null, "source-" )
			on_( '<' )	expression_do_( set_target_null, "target<" )
			on_( '.' )	expression_do_( nop, "~." )
			on_( '%' )	expression_do_( nop, "%" )
			on_separator	expression_do_( set_source_null, pop_state )
			on_other	expression_do_( read_argument, "identifier" )
			end
			in_( "~." ) bgn_
				on_( '-' )	expression_do_( set_source_any, "source-" )
				on_( '<' )	expression_do_( set_target_any, "target<" )
				on_( ':' )	expression_do_( set_source_any, "source-medium->target:" )
				on_( ' ' )	expression_do_( set_source_any, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_any, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_any, pop_state )
				on_other	expression_do_( set_source_any, pop_state )
				end
			in_( "~ " ) bgn_
				on_( ' ' )	expression_do_( nop, same )
				on_( '\t' )	expression_do_( nop, same )
				on_( ']' )	expression_do_( set_source_null, pop_state )
				on_other	expression_do_( set_source_null, pop_state )
				end
		in_( "." ) bgn_
			on_( '-' )	expression_do_( set_source_any, "source-" )
			on_( '<' )	expression_do_( set_target_any, "target<" )
			on_( ':' )	expression_do_( set_source_any, "source-medium->target:" )
			on_( ' ' )	expression_do_( set_source_any, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_any, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_any, pop_state )
			on_( '\n' )	expression_do_( set_source_any, pop_state )
			on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '?' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '*' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '_' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '~' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '%' )	expression_do_( read_shorty, "source-medium->target" )
			on_( '[' )	expression_do_( read_shorty, "source-medium->target" )
			on_other	expression_do_( read_shorty, "source-medium->target" )
			end

		in_( "[]" ) bgn_
			on_( '-' )	expression_do_( set_source_pop, "source-" )
			on_( '<' )	expression_do_( swap_source_target, "target<" )
			on_( ':' )	expression_do_( set_source_pop, "source-medium->target:" )
			on_( ' ' )	expression_do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_pop, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_pop, pop_state )
			on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
			on_other	expression_do_( set_source_pop, pop_state )
			end

		in_( "identifier" ) bgn_
			on_( '(' )	expression_do_( nothing, pop_state )
			on_( '-' )	expression_do_( set_source_identifier, "source-" )
			on_( '<' )	expression_do_( set_target_identifier, "target<" )
			on_( ':' )	expression_do_( set_source_identifier, "source-medium->target:" )
			on_( ' ' )	expression_do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_identifier, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_identifier, pop_state )
			on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
			on_other	expression_do_( set_source_identifier, pop_state )
			end

		in_( "%" ) bgn_
			on_( '%' )	expression_do_( nop, "%%" )
			on_( '?' )	expression_do_( nop, "%?" )
			on_other	expression_do_( read_argument, "%identifier" )
			end
			in_( "%%" ) bgn_
				on_( '-' )	expression_do_( set_source_this, "source-" )
				on_( '<' )	expression_do_( set_target_this, "target<" )
				on_( ':' )	expression_do_( set_source_this, "source-medium->target:" )
				on_( ' ' )	expression_do_( set_source_this, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_this, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_this, pop_state )
				on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
				on_other	expression_do_( set_source_this, pop_state )
				end
			in_( "%?" ) bgn_
				on_( '-' )	expression_do_( set_source_variator, "source-" )
				on_( '<' )	expression_do_( set_target_variator, "target<" )
				on_( ':' )	expression_do_( set_source_variator, "source-medium->target:" )
				on_( ' ' )	expression_do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_variator, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_variator, pop_state )
				on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
				on_other	expression_do_( set_source_variator, pop_state )
				end
			in_( "%identifier" ) bgn_
				on_( '-' )	expression_do_( set_source_variable, "source-" )
				on_( '<' )	expression_do_( set_target_variable, "target<" )
				on_( ':' )	expression_do_( set_source_variable, "source-medium->target:" )
				on_( ' ' )	expression_do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_variable, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_variable, pop_state )
				on_( '.' )	expression_do_( read_shorty, "source-medium->target" )
				on_other	expression_do_( set_source_variable, pop_state )
				end

	in_( "source-" ) bgn_
		on_( '*' )	expression_do_( set_flag_active, "source-*" )
		on_( '_' )	expression_do_( set_flag_inactive, "source-_" )
		on_( '~' )	expression_do_( set_flag_not, "source-~" )
		on_( '[' )	expression_do_( push, "source-[]" )
		on_( '-' )	expression_do_( set_medium_null, "source-medium-" )
		on_( '.' )	expression_do_( nop, "source-." )
		on_( '%' )	expression_do_( nop, "source-%" )
		on_( '?' )	expression_do_( nop, "source-?" )
		on_other	expression_do_( read_argument, "source-identifier" )
		end
		in_( "source-*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-~" )
			on_( '[' )	expression_do_( push, "source-[]" )
			on_( '.' )	expression_do_( nop, "source-." )
			on_( '%' )	expression_do_( nop, "source-%" )
			on_other	expression_do_( read_argument, "source-identifier" )
			end
		in_( "source-_" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-~" )
			on_( '[' )	expression_do_( push, "source-[]" )
			on_( '.' )	expression_do_( nop, "source-." )
			on_( '%' )	expression_do_( nop, "source-%" )
			on_other	expression_do_( read_argument, "source-identifier" )
			end
		in_( "source-~" ) bgn_
			on_( '-' )	expression_do_( set_medium_null, "source-medium-" )
			on_( '[' )	expression_do_( push, "source-[]" )
			on_( '.' )	expression_do_( nop, "source-." )
			on_( '%' )	expression_do_( nop, "source-%" )
			on_other	expression_do_( read_argument, "source-identifier" )
			end
		in_( "source-?" ) bgn_
			on_( '-' )	expression_do_( set_medium_mark, "source-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "source-[]" ) bgn_
			on_( '-' )	expression_do_( set_medium_pop, "source-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "source-." ) bgn_
			on_( '-' )	expression_do_( set_medium_any, "source-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "source-identifier" ) bgn_
			on_( '-' )	expression_do_( set_medium_identifier, "source-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "source-%" ) bgn_
			on_( '%' )	expression_do_( nop, "source-%%" )
			on_( '?' )	expression_do_( nop, "source-%?" )
			on_other	expression_do_( read_argument, "source-%identifier" )
			end
			in_( "source-%%" ) bgn_
				on_( '-' )	expression_do_( set_medium_this, "source-medium-" )
				on_other	expression_do_( error, "" )
				end
			in_( "source-%?" ) bgn_
				on_( '-' )	expression_do_( set_medium_variator, "source-medium-" )
				on_other	expression_do_( error, "" )
				end
			in_( "source-%identifier" ) bgn_
				on_( '-' )	expression_do_( set_medium_variable, "source-medium-" )
				on_other	expression_do_( error, "" )
				end

	in_( "source-medium-" ) bgn_
		on_( '>' )	expression_do_( nop, "source-medium->" )
		on_other	expression_do_( error, "" )
		end

	in_( "source-medium->" ) bgn_
		on_( '*' )	expression_do_( set_flag_active, "source-medium->*" )
		on_( '_' )	expression_do_( set_flag_inactive, "source-medium->_" )
		on_( '~' )	expression_do_( set_flag_not, "source-medium->~" )
		on_( ' ' )	expression_do_( set_target_null, "source-medium->target" )
		on_( '\t' )	expression_do_( set_target_null, "source-medium->target" )
		on_( ']' )	expression_do_( set_target_null, pop_state )
		on_( '[' )	expression_do_( push, "source-medium->[]" )
		on_( '%' )	expression_do_( nop, "source-medium->%" )
		on_( '.' )	expression_do_( set_target_any, "source-medium->target" )
		on_( '?' )	expression_do_( set_target_mark, "source-medium->target" )
		on_( ':' )	expression_do_( set_target_null, "source-medium->target:" )
		on_separator	expression_do_( set_target_null, pop_state )
		on_other	expression_do_( read_argument, "source-medium->identifier" )
		end
		in_( "source-medium->*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-medium->~" )
			on_( '[' )	expression_do_( push, "source-medium->[]" )
			on_( '%' )	expression_do_( nop, "source-medium->%" )
			on_( '.' )	expression_do_( set_target_any, "source-medium->target" )
			on_other	expression_do_( read_argument, "source-medium->identifier" )
			end
		in_( "source-medium->_" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-medium->~" )
			on_( '[' )	expression_do_( push, "source-medium->[]" )
			on_( '%' )	expression_do_( nop, "source-medium->%" )
			on_( '.' )	expression_do_( set_target_any, "source-medium->target" )
			on_other	expression_do_( read_argument, "source-medium->identifier" )
			end
		in_( "source-medium->~" ) bgn_
			on_( ' ' )	expression_do_( set_target_null, "source-medium->target" )
			on_( '\t' )	expression_do_( set_target_null, "source-medium->target" )
			on_( ']' )	expression_do_( set_target_null, pop_state )
			on_( '[' )	expression_do_( push, "source-medium->[]" )
			on_( '%' )	expression_do_( nop, "source-medium->%" )
			on_( '.' )	expression_do_( set_target_any, "source-medium->target" )
			on_( ':' )	expression_do_( set_target_null, "source-medium->target:" )
			on_separator	expression_do_( set_target_null, pop_state )
			on_other	expression_do_( read_argument, "source-medium->identifier" )
			end
		in_( "source-medium->[]" ) bgn_
			on_( ' ' )	expression_do_( set_target_pop, "source-medium->target" )
			on_( '\t' )	expression_do_( set_target_pop, "source-medium->target" )
			on_( ']' )	expression_do_( set_target_pop, pop_state )
			on_( ':' )	expression_do_( set_target_pop, "source-medium->target:" )
			on_other	expression_do_( set_target_pop, pop_state )
			end
		in_( "source-medium->identifier" ) bgn_
			on_( ' ' )	expression_do_( set_target_identifier, "source-medium->target" )
			on_( '\t' )	expression_do_( set_target_identifier, "source-medium->target" )
			on_( ']' )	expression_do_( set_target_identifier, pop_state )
			on_( ':' )	expression_do_( set_target_identifier, "source-medium->target:" )
			on_other	expression_do_( set_target_identifier, pop_state )
			end
		in_( "source-medium->%" ) bgn_
				on_( '%' )	expression_do_( nop, "source-medium->%%" )
				on_( '?' )	expression_do_( nop, "source-medium->%?" )
				on_other	expression_do_( read_argument, "source-medium->%identifier" )
				end
				in_( "source-medium->%%" ) bgn_
					on_( ' ' )	expression_do_( set_target_this, "source-medium->target" )
					on_( '\t' )	expression_do_( set_target_this, "source-medium->target" )
					on_( ']' )	expression_do_( set_target_this, pop_state )
					on_( ':' )	expression_do_( set_target_this, "source-medium->target:" )
					on_other	expression_do_( set_target_this, pop_state )
					end
				in_( "source-medium->%?" ) bgn_
					on_( ' ' )	expression_do_( set_target_variator, "source-medium->target" )
					on_( '\t' )	expression_do_( set_target_variator, "source-medium->target" )
					on_( ']' )	expression_do_( set_target_variator, pop_state )
					on_( ':' )	expression_do_( set_target_variator, "source-medium->target:" )
					on_other	expression_do_( set_target_variator, pop_state )
					end
				in_( "source-medium->%identifier" ) bgn_
					on_( ' ' )	expression_do_( set_target_variable, "source-medium->target" )
					on_( '\t' )	expression_do_( set_target_variable, "source-medium->target" )
					on_( ']' )	expression_do_( set_target_variable, pop_state )
					on_( ':' )	expression_do_( set_target_variable, "source-medium->target:" )
					on_other	expression_do_( set_target_variable, pop_state )
					end

	in_( "target<" ) bgn_
		on_( '-' )	expression_do_( nop, "target<-" )
		on_other	expression_do_( error, "" )
		end

	in_( "target<-" ) bgn_
		on_( '*' )	expression_do_( set_flag_active, "target<-*" )
		on_( '_' )	expression_do_( set_flag_inactive, "target<-_" )
		on_( '~' )	expression_do_( set_flag_not, "target<-~" )
		on_( '[' )	expression_do_( push, "target<-[]" )
		on_( '-' )	expression_do_( set_medium_null, "target<-medium-" )
		on_( '.' )	expression_do_( set_medium_any, "target<-." )
		on_( '%' )	expression_do_( nop, "target<-%" )
		on_( '?' )	expression_do_( nop, "target<-?" )
		on_other	expression_do_( read_argument, "target<-identifier" )
		end
		in_( "target<-*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "target<-~" )
			on_( '[' )	expression_do_( push, "target<-[]" )
			on_( '.' )	expression_do_( set_medium_any, "target<-medium" )
			on_( '%' )	expression_do_( nop, "target<-%" )
			on_other	expression_do_( read_argument, "target<-identifier" )
			end
		in_( "target<-_" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "target<-~" )
			on_( '[' )	expression_do_( push, "target<-[]" )
			on_( '.' )	expression_do_( set_medium_any, "target<-medium" )
			on_( '%' )	expression_do_( nop, "target<-%" )
			on_other	expression_do_( read_argument, "target<-identifier" )
			end
		in_( "target<-~" ) bgn_
			on_( '[' )	expression_do_( push, "target<-[]" )
			on_( '-' )	expression_do_( set_medium_null, "target<-medium-" )
			on_( '.' )	expression_do_( set_medium_any, "target<-medium" )
			on_( '%' )	expression_do_( nop, "target<-%" )
			on_other	expression_do_( read_argument, "target<-identifier" )
			end
		in_( "target<-?" ) bgn_
			on_( '-' )	expression_do_( set_medium_mark, "target<-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "target<-[]" ) bgn_
			on_( '-' )	expression_do_( set_medium_pop, "target<-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "target<-." ) bgn_
			on_( '.' )	expression_do_( set_source_any, "source-medium->target" )
			on_( '-' )	expression_do_( nop, "target<-.-" )
			on_other	expression_do_( error, "" )
			end
		in_( "target<-identifier" ) bgn_
			on_( '-' )	expression_do_( set_medium_identifier, "target<-medium-" )
			on_other	expression_do_( error, "" )
			end
		in_( "target<-%" ) bgn_
			on_( '%' )	expression_do_( nop, "target<-%%" )
			on_( '?' )	expression_do_( nop, "target<-%?" )
			on_other	expression_do_( read_argument, "target<-%identifier" )
			end
			in_( "target<-%%" ) bgn_
				on_( '-' )	expression_do_( set_medium_this, "target<-medium-" )
				on_other	expression_do_( error, "" )
				end
			in_( "target<-%?" ) bgn_
				on_( '-' )	expression_do_( set_medium_variator, "target<-medium-" )
				on_other	expression_do_( error, "" )
				end
			in_( "target<-%identifier" ) bgn_
				on_( '-' )	expression_do_( set_medium_variable, "target<-medium-" )
				on_other	expression_do_( error, "" )
				end

	in_( "target<-medium-" ) bgn_
		on_( '*' )	expression_do_( set_flag_active, "target<-medium-*" )
		on_( '_' )	expression_do_( set_flag_inactive, "target<-medium-_" )
		on_( '~' )	expression_do_( set_flag_not, "target<-medium-~" )
		on_( ' ' )	expression_do_( set_source_null, "source-medium->target" )
		on_( '\t' )	expression_do_( set_source_null, "source-medium->target" )
		on_( ']' )	expression_do_( set_source_null, pop_state )
		on_( '[' )	expression_do_( push, "target<-medium-[]" )
		on_( '%' )	expression_do_( nop, "target<-medium-%" )
		on_( '.' )	expression_do_( set_source_any, "source-medium->target" )
		on_( '?' )	expression_do_( set_source_mark, "source-medium->target" )
		on_( ':' )	expression_do_( set_source_null, "source-medium->target:" )
		on_separator	expression_do_( set_source_null, pop_state )
		on_other	expression_do_( read_argument, "target<-medium-identifier" )
		end
		in_( "target<-medium-*" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	expression_do_( push, "target<-medium-[]" )
			on_( '%' )	expression_do_( nop, "target<-medium-%" )
			on_( '.' )	expression_do_( set_source_any, "source-medium->target" )
			on_other	expression_do_( read_argument, "target<-medium-identifier" )
			end
		in_( "target<-medium-_" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	expression_do_( push, "target<-medium-[]" )
			on_( '%' )	expression_do_( nop, "target<-medium-%" )
			on_( '.' )	expression_do_( set_source_any, "source-medium->target" )
			on_other	expression_do_( read_argument, "target<-medium-identifier" )
			end
		in_( "target<-medium-~" ) bgn_
			on_( ' ' )	expression_do_( set_source_null, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_null, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_null, pop_state )
			on_( '[' )	expression_do_( push, "target<-medium-[]" )
			on_( '%' )	expression_do_( nop, "target<-medium-%" )
			on_( '.' )	expression_do_( set_source_any, "source-medium->target" )
			on_( ':' )	expression_do_( set_source_null, "source-medium->target:" )
			on_separator	expression_do_( set_source_null, pop_state )
			on_other	expression_do_( read_argument, "target<-medium-identifier" )
			end
		in_( "target<-medium-[]" ) bgn_
			on_( ' ' )	expression_do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_pop, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_pop, pop_state )
			on_( ':' )	expression_do_( set_source_pop, "source-medium->target:" )
			on_other	expression_do_( set_source_pop, pop_state )
			end
		in_( "target<-medium-identifier" ) bgn_
			on_( ' ' )	expression_do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	expression_do_( set_source_identifier, "source-medium->target" )
			on_( ']' )	expression_do_( set_source_identifier, pop_state )
			on_( ':' )	expression_do_( set_source_identifier, "source-medium->target:" )
			on_other	expression_do_( set_source_identifier, pop_state )
			end
		in_( "target<-medium-%" ) bgn_
			on_( '%' )	expression_do_( nop, "target<-medium-%%" )
			on_( '?' )	expression_do_( nop, "target<-medium-%?" )
			on_other	expression_do_( read_argument, "target<-medium-%identifier" )
			end
			in_( "target<-medium-%%" ) bgn_
				on_( ' ' )	expression_do_( set_source_this, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_this, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_this, pop_state )
				on_( ':' )	expression_do_( set_source_this, "source-medium->target:" )
				on_other	expression_do_( set_source_this, pop_state )
				end
			in_( "target<-medium-%?" ) bgn_
				on_( ' ' )	expression_do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_variator, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_variator, pop_state )
				on_( ':' )	expression_do_( set_source_variator, "source-medium->target:" )
				on_other	expression_do_( set_source_variator, pop_state )
				end
			in_( "target<-medium-%identifier" ) bgn_
				on_( ' ' )	expression_do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	expression_do_( set_source_variable, "source-medium->target" )
				on_( ']' )	expression_do_( set_source_variable, pop_state )
				on_( ':' )	expression_do_( set_source_variable, "source-medium->target:" )
				on_other	expression_do_( set_source_variable, pop_state )
				end

	in_( "source-medium->target" ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( ']' )	expression_do_( nop, pop_state )
		on_( ':' )	expression_do_( test_mode_instantiate, "source-medium->target:" )
		on_other	expression_do_( nothing, pop_state )
		end

	in_( "source-medium->target:" ) bgn_
		on_( '*' )	expression_do_( set_flag_active, "source-medium->target: *" )
		on_( '_' )	expression_do_( set_flag_inactive, "source-medium->target: _" )
		on_( '~' )	expression_do_( set_flag_not, "source-medium->target: ~" )
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( '[' )	expression_do_( push, "source-medium->target: []" )
		on_( '.' )	expression_do_( set_instance_any, "source-medium->target: instance" )
		on_( '%' )	expression_do_( nop, "source-medium->target: %" )
		on_separator	expression_do_( nothing, pop_state )
		on_other	expression_do_( read_argument, "source-medium->target: identifier" )
		end
		in_( "source-medium->target: *" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	expression_do_( push, "source-medium->target: []" )
			on_( '.' )	expression_do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	expression_do_( nop, "source-medium->target: %" )
			on_other	expression_do_( read_argument, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: _" ) bgn_
			on_( '~' )	expression_do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	expression_do_( push, "source-medium->target: []" )
			on_( '.' )	expression_do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	expression_do_( nop, "source-medium->target: %" )
			on_other	expression_do_( read_argument, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: ~" ) bgn_
			on_( ' ' )	expression_do_( set_instance_null, "source-medium->target: instance" )
			on_( '\t' )	expression_do_( set_instance_null, "source-medium->target: instance" )
			on_( '[' )	expression_do_( push, "source-medium->target: []" )
			on_( '.' )	expression_do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	expression_do_( nop, "source-medium->target: %" )
			on_separator	expression_do_( set_instance_null, pop_state )
			on_other	expression_do_( read_argument, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: []" ) bgn_
			on_( ' ' )	expression_do_( set_instance_pop, "source-medium->target: instance" )
			on_( '\t' )	expression_do_( set_instance_pop, "source-medium->target: instance" )
			on_( ']' )	expression_do_( set_instance_pop, pop_state )
			on_other	expression_do_( set_instance_pop, pop_state )
			end
		in_( "source-medium->target: %" ) bgn_
			on_( '%' )	expression_do_( nop, "source-medium->target: %%" )
			on_( '?' )	expression_do_( nop, "source-medium->target: %?" )
			on_( '[' )	expression_do_( read_as_sub, "source-medium->target: instance" )
			on_other	expression_do_( read_argument, "source-medium->target: %identifier" )
			end
			in_( "source-medium->target: %%" ) bgn_
				on_( ' ' )	expression_do_( set_instance_this, "source-medium->target: instance" )
				on_( '\t' )	expression_do_( set_instance_this, "source-medium->target: instance" )
				on_( ']' )	expression_do_( set_instance_this, pop_state )
				on_other	expression_do_( set_instance_this, pop_state )
				end
			in_( "source-medium->target: %?" ) bgn_
				on_( ' ' )	expression_do_( set_instance_variator, "source-medium->target: instance" )
				on_( '\t' )	expression_do_( set_instance_variator, "source-medium->target: instance" )
				on_( ']' )	expression_do_( set_instance_variator, pop_state )
				on_other	expression_do_( set_instance_variator, pop_state )
				end
			in_( "source-medium->target: %identifier" ) bgn_
				on_( ' ' )	expression_do_( set_instance_variable, "source-medium->target: instance" )
				on_( '\t' )	expression_do_( set_instance_variable, "source-medium->target: instance" )
				on_( ']' )	expression_do_( set_instance_variable, pop_state )
				on_other	expression_do_( set_instance_variable, pop_state )
				end
		in_( "source-medium->target: identifier" ) bgn_
			on_( ' ' )	expression_do_( set_instance_identifier, "source-medium->target: instance" )
			on_( '\t' )	expression_do_( set_instance_identifier, "source-medium->target: instance" )
			on_( ']' )	expression_do_( set_instance_identifier, pop_state )
			on_other	expression_do_( set_instance_identifier, pop_state )
			end

	in_( "source-medium->target: instance" ) bgn_
		on_( ' ' )	expression_do_( nop, same )
		on_( '\t' )	expression_do_( nop, same )
		on_( ']' )	expression_do_( nothing, pop_state )
		on_other	expression_do_( nothing, pop_state )
		end
	end
	}
	while ( strcmp( state, "" ) );

	expression_do_( parser_exit, same );

#ifdef DEBUG
	fprintf( stderr, "debug> exiting parse_expression '%c'\n", event );
#endif

	return event;
}

