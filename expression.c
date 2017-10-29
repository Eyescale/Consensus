#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "output.h"
#include "expression.h"
#include "expression_util.h"
#include "api.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	parser engine
---------------------------------------------------------------------------*/
#define	do_( a, s )	\
	event = expression_execute( a, &state, event, s, context );

static Expression *
set_sub_expression( char *state, _context *context )
{
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

	Expression *e = calloc( 1, sizeof(Expression) );
	e->sub[ 3 ].result.any = 1;
	for ( int i=0; i<3; i++ ) {
		e->sub[ i ].result.none = 1;	// none meaning: none specified
	}

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

static int
test_super( char *state, int event, char **next_state, _context *context )
{
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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
		default: return output( Debug, "in test_super" );
	}
	return outputf( Error, "instance is not allowed after super ('%s') in expression", super );
}

static int
expression_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	int retval = 0;

	if ( action == push )
	{
#ifdef DEBUG
		outputf( Debug, "parser: pushing from level %d", context->control.level );
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
			outputf( Debug, "parser: popping from base level %d", context->control.level );
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
			outputf( Debug, "parser: popping from level %d", context->control.level );
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
	else if ( !strcmp( next_state, "source-medium->target:" ) ) {
		int retval = test_super( *state, event, &next_state, context );
		if ( !retval ) {
			event = action( *state, event, &next_state, context );
			if ( strcmp( next_state, same ) )
				*state = next_state;
		}
		else event =  retval;
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
	stack->expression.flag.not = 0;
	stack->expression.flag.active = 0;
	stack->expression.flag.inactive = 0;
	return 0;
}
static void
set_flags( StackVA *stack, Expression *expression, int count )
{
	expression->sub[ count ].result.not = stack->expression.flag.not;
	expression->sub[ count ].result.active = stack->expression.flag.active;
	expression->sub[ count ].result.inactive = stack->expression.flag.inactive;
	stack->expression.flag.not = 0;
	stack->expression.flag.active = 0;
	stack->expression.flag.inactive = 0;
}

static int
set_sub_mark( int count, int event, _context *context )
{
	if (( count == 3 ) && ( event == ':' )) {
		StackVA *stack = (StackVA*) context->control.stack->ptr;
		if ( stack->expression.flag.not )
			return output( Error, "extraneous '~' in expression" );
		if ( stack->expression.flag.active )
			return output( Error, "extraneous '*' in expression" );
		if ( stack->expression.flag.inactive )
			return output( Error, "extraneous '_' in expression" );
	}
	if ( context->expression.marked ) {
		return output( Error, "too many '?''s" );
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
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	if ( stack->expression.flag.not ) {
		expression->sub[ count ].result.identifier.type = NullIdentifier;
		expression->sub[ count ].result.any = 0;
		expression->sub[ count ].result.none = 0;
		stack->expression.flag.not = 0;
		stack->expression.flag.active = 0;
		if ( stack->expression.flag.inactive ) {
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
	if ( context->control.mode != ExecutionMode ) {
		return reset_flags( context );
	}

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = DefaultIdentifier;
	expression->sub[ count ].result.identifier.value = context->identifier.id[ 1 ].ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	context->identifier.id[ 1 ].ptr = NULL;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_variable( int count, int event, _context *context )
{
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.value = context->identifier.id[ 1 ].ptr;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	context->identifier.id[ 1 ].ptr = NULL;
	set_flags( stack, expression, count );
	return 0;
}

static int
set_sub_pop( int count, int event, _context *context )
{
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
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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
	if ( context->control.mode != ExecutionMode )
		return reset_flags( context );

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	expression->sub[ count ].result.identifier.type = VariableIdentifier;
	expression->sub[ count ].result.identifier.value = this_symbol;
	expression->sub[ count ].result.any = 0;
	expression->sub[ count ].result.none = 0;
	set_flags( stack, expression, count );
	return 0;
}

static int
test_scheme( char *state, int event, char **next_state, _context *context )
{
	if ( strcmp( state, "identifier" ) || strcmp( *next_state, "source-medium->target:" ))
		return 0;

	char *identifier = context->identifier.id[ 1 ].ptr;
 	if ( strcmp( identifier, "file" ) && strcmp( identifier, "session" ) )
		return 0;

	if ( context->expression.level != context->control.level )
		return outputf( Error, "term '%s:' in expression is reserved", identifier );

	context->expression.scheme = identifier;
	context->identifier.id[ 1 ].ptr = NULL;
	*next_state = "";
	return event;
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
	int retval = test_scheme( state, event, next_state, context );
	return ( retval ? retval : set_sub_identifier( 0, event, context ) );
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
	if ( stack->expression.flag.not ) {
		return output( Error, "redundant '~' in expression" );
	}
	stack->expression.flag.not = 1;
	return 0;
}
static int
set_flag_active( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->control.stack->ptr;
	if ( stack->expression.flag.not ) {
		return output( Error, "'*' must precede '~' in expression" );
	}
	if ( stack->expression.flag.inactive ) {
		return output( Error, "conflicting '_' and '*' in expression" );
	}
	stack->expression.flag.active = 1;
	return 0;
}
static int
set_flag_inactive( char *state, int event, char **next_state, _context *context )
{
	StackVA *stack = (StackVA*) context->control.stack->ptr;
	if ( stack->expression.flag.not ) {
		return output( Error, "'_' must precede '~' in expression" );
	}
	if ( stack->expression.flag.active ) {
		return output( Error, "conflicting '*' and '_' in expression" );
	}
	stack->expression.flag.inactive = 1;
	return 0;
}

/*---------------------------------------------------------------------------
	set_as_sub
---------------------------------------------------------------------------*/
static int
set_as_sub( char *state, int event, char **next_state, _context *context )
{
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
	outputf( Debug, "read_as_sub: in \"%s\", on '%c'", state, event );
#endif

	bgn_
	in_( "%" ) bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '.' )	do_( nop, "%[." )
		on_( '?' )	do_( nop, "%[?" )
		on_other	do_( error, "" )
		end
	in_( "source-medium->target: %" ) bgn_
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '.' )	do_( nop, "%[." )
		on_( '?' )	do_( nop, "%[?" )
		on_other	do_( error, "" )
		end
		in_( "%[." ) bgn_
			on_( '.' )	do_( nop, "%[.." )
			on_( '?' )	do_( nop, "%[.?" )
			on_other	do_( error, "" )
			end
			in_( "%[.." ) bgn_
				on_( '?' )	do_( set_as_sub, "%[_" )
				on_other	do_( error, "" )
				end
			in_( "%[.?" ) bgn_
				on_( '.' )	do_( set_as_sub, "%[_" )
				on_( '?' )	do_( set_as_sub, "%[_" )
				on_other	do_( error, "" )
				end
		in_( "%[?" ) bgn_
			on_( '.' )	do_( nop, "%[?." )
			on_( '?' )	do_( nop, "%[??" )
			on_other	do_( error, "" )
			end
			in_( "%[?." ) bgn_
				on_( '.' )	do_( set_as_sub, "%[_" )
				on_( '?' )	do_( set_as_sub, "%[_" )
				on_other	do_( error, "" )
				end
			in_( "%[??" ) bgn_
				on_( '.' )	do_( set_as_sub, "%[_" )
				on_( '?' )	do_( set_as_sub, "%[_" )
				on_other	do_( error, "" )
				end
		in_( "%[_" ) bgn_
			on_( ' ' )		do_( nop, same )
			on_( '\t' )		do_( nop, same )
			on_( ']' )		do_( nop, "" )
			on_other		do_( error, "" )
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
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
	Expression *expression = stack->expression.ptr;
	ExpressionSub *sub = expression->sub;

	bgn_
	in_( ".." )		do_( nop, same )

	in_( "[]." )		do_( set_source_pop, same )
	in_( "identifier." )	do_( set_source_identifier, same )
	in_( "%!." )		do_( set_source_this, same )
	in_( "%?." )		do_( set_source_variator, same )
	in_( "%identifier." )	do_( set_source_variable, same )

	in_( ".[]" )		do_( set_medium_pop, same )
	in_( ".identifier" )	do_( set_medium_identifier, same )
	in_( ".%?" )		do_( set_medium_variator, same )
	in_( ".%!" )		do_( set_medium_this, same )
	in_( ".%identifier" )	do_( set_medium_variable, same )

	in_( "..[]" )		EPUSH do_( set_target_pop, same ) EPOP
	in_( "..identifier" )	EPUSH do_( set_target_identifier, same ) EPOP
	in_( "..%?" )		EPUSH do_( set_target_variator, same ) EPOP
	in_( "..%!" )		EPUSH do_( set_target_this, same ) EPOP
	in_( "..%identifier" )	EPUSH do_( set_target_variable, same ) EPOP
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
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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
	set_super
---------------------------------------------------------------------------*/
static int
set_super( char *state, int event, char **next_state, _context *context )
{
	if ( context->control.mode != ExecutionMode )
		return 0;

	StackVA *stack = (StackVA*) context->control.stack->ptr;
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

/*---------------------------------------------------------------------------
	read_shorty
---------------------------------------------------------------------------*/
static int
read_shorty( char *state, int event, char **next_state, _context *context )
{
	do {
	event = input( state, event, NULL, context );
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
	in_( "[]" ) bgn_
		on_( '.' )	do_( nop, "[]." )
		end
		in_( "[]." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "identifier" ) bgn_
		on_( '.' )	do_( nop, "identifier." )
		end
		in_( "identifier." ) bgn_
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
	in_( "%identifier" ) bgn_
		on_( '.' )	do_( nop, "%identifier." )
		end
		in_( "%identifier." ) bgn_
			on_( '.' )	do_( set_shorty, "" )
			on_other	do_( error, "" )
			end
	in_( "." ) bgn_
		on_( '.' )	do_( nop, ".." )
		on_( '!' )	do_( nop, ".!" )
		on_( '?' )	do_( nop, ".?" )
		on_( '*' )	do_( set_flag_active, ".*" )
		on_( '_' )	do_( set_flag_inactive, "._" )
		on_( '~' )	do_( set_flag_not, ".~" )
		on_( '%' )	do_( nop, ".%" )
		on_( '[' )	do_( push, ".[]" )
		on_other	do_( read_1, ".identifier" )
		end
		in_( ".*" ) bgn_
			on_( '~' )	do_( set_flag_not, ".~" )
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[]" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( "._" ) bgn_
			on_( '~' )	do_( set_flag_not, ".~" )
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[]" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( ".~" ) bgn_
			on_( '%' )	do_( nop, ".%" )
			on_( '[' )	do_( push, ".[]" )
			on_other	do_( read_1, ".identifier" )
			end
		in_( ".[]" ) bgn_
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
#ifdef SUBVAR
			on_other	do_( read_variable_ref, ".%identifier" )
#else
			on_other	do_( read_1, ".%identifier" )
#endif
			end
			in_( ".%?" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
			in_( ".%!" ) bgn_
				on_( '.' )	do_( set_shorty, "" )
				on_other	do_( error, "" )
				end
			in_( ".%identifier" ) bgn_
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
			on_( '_' )	do_( set_flag_inactive, ".._" )
			on_( '~' )	do_( set_flag_not, "..~" )
			on_( '%' )	do_( nop, "..%" )
			on_( '[' )	do_( push, "..[]" )
			on_other	do_( read_1, "..identifier" )
			end
			in_( "..*" ) bgn_
				on_( '~' )	do_( set_flag_not, "..~" )
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( push, "..[]" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( ".._" ) bgn_
				on_( '~' )	do_( set_flag_not, "..~" )
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( nop, "..[]" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( "..~" ) bgn_
				on_( '%' )	do_( nop, "..%" )
				on_( '[' )	do_( nop, "..[]" )
				on_other	do_( read_1, "..identifier" )
				end
			in_( "..[]" ) bgn_
				on_any		do_( set_shorty, "" )
				end
			in_( "..identifier" ) bgn_
				on_any		do_( set_shorty, "" )
				end
			in_( "..%" ) bgn_
				on_( '?' )	do_( nop, "..%?" )
				on_( '!' )	do_( nop, "..%!" )
#ifdef SUBVAR
				on_other	do_( read_variable_ref, "..%identifier" )
#else
				on_other	do_( read_1, "..%identifier" )
#endif
				end
				in_( "..%?" ) bgn_
					on_any	do_( set_shorty, "" )
					end
				in_( "..%!" ) bgn_
					on_any	do_( set_shorty, "" )
					end
				in_( "..%identifier" ) bgn_
					on_any	do_( set_shorty, "" )
					end
		end

		// return control to read_expression after push or when mark set
		if (( state == base ) || !strcmp( state, "source-" ) || !strcmp( state, "target<" ) || !strcmp( state, "source-medium->target" ))
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
	outputf( Debug, "parser_init: setting parser level to %d", context->control.level );
#endif
	context->expression.level = context->control.level;
	context->expression.mode = ReadMode;
	context->expression.marked = 0;

	free( context->expression.scheme );
	context->expression.scheme = NULL;
	freeExpression( context->expression.ptr );
	context->expression.ptr = NULL;
	context->expression.filter = NULL;
	if ( context->control.mode != ExecutionMode )
		return event;

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	Expression *expression = (Expression *) calloc( 1, sizeof(Expression) );
	expression->sub[ 3 ].result.any = 1;
	for ( int i=0; i<3; i++ )
		expression->sub[ i ].result.none = 1;	// none meaning: none specified

	stack->expression.ptr = expression;
	stack->expression.flag.not = 0;
	stack->expression.flag.active = 0;
	stack->expression.flag.inactive = 0;

	return event;

}

/*---------------------------------------------------------------------------
	parser_exit
---------------------------------------------------------------------------*/
static int
parser_exit( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	outputf( Debug, "parser_exit: on the way: returning event='%c' in mode=%d...", event, context->expression.mode );
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
			context->expression.ptr = NULL;
		} else {
			context->expression.ptr = expression;
			expression->result.marked = context->expression.marked;
			if ( expression->result.marked ) {
				trace_mark( expression );
				if ( mark_negated( expression, 0 ) ) {
					output( Warning, "'?' inside of negative clause - "
						"will not yield result..." );
				}
			}
		}
	}
	stack->expression.ptr = NULL;
	return event;
}

/*---------------------------------------------------------------------------
	read_expression
---------------------------------------------------------------------------*/
int
read_expression( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "parser: entering" );
#endif
	// set own state according to commander's state
	bgn_
	in_( "!. %" )			do_( nothing, "%" )
	in_( ": identifier : %" )	do_( nothing, "%" )
	in_other 			do_( nothing, base )
	end
	do_( parser_init, same )

	do {
	event = input( state, event, NULL, context );
#ifdef DEBUG
	outputf( Debug, "read_expression: in \"%s\", on '%c'", state, event );
#endif

	bgn_
	// errors are handled in parser_exit
	on_( -1 )	do_( nothing, "" )

	// return control to read_shorty after pop
	in_( ".[]" )	do_( read_shorty, "source-medium->target" )
	in_( "..[]" )	do_( read_shorty, "source-medium->target" )

	in_( base ) bgn_
		on_( '(' )	do_( error, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '*' )	do_( set_flag_active, "*" )
		on_( '_' )	do_( set_flag_inactive, "_" )
		on_( '~' )	do_( set_flag_not, "~" )
		on_( ']' )	do_( nothing, pop_state )
		on_( '[' )	do_( push, "[]" )
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
			on_( '[' )	do_( push, "[]" )
			on_( '.' )	do_( nop, "." )
			on_( '%' )	do_( nop, "%" )
			on_other	do_( read_1, "identifier" )
			end
		in_( "_" ) bgn_
			on_( '~' )	do_( set_flag_not, "~" )
			on_( '[' )	do_( push, "[]" )
			on_( '.' )	do_( nop, "." )
			on_( '%' )	do_( nop, "%" )
			on_other	do_( read_1, "identifier" )
			end
		in_( "~" ) bgn_
			on_( ' ' )	do_( set_source_null, "source-medium->target" )
			on_( '\t' )	do_( set_source_null, "source-medium->target" )
			on_( ':' )	do_( set_source_null, "source-medium->target:" )
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( nop, "~ " )
			on_( '\t' )	do_( nop, "~ " )
			on_( ']' )	do_( set_source_null, pop_state )
			on_( '[' )	do_( push, "[]" )
			on_( '-' )	do_( set_source_null, "source-" )
			on_( '<' )	do_( set_target_null, "target<" )
			on_( '.' )	do_( nop, "~." )
			on_( '%' )	do_( nop, "%" )
			on_separator	do_( set_source_null, pop_state )
			on_other	do_( read_1, "identifier" )
			end
			in_( "~." ) bgn_
				on_( '(' )	do_( error, "" )
				on_( '-' )	do_( set_source_any, "source-" )
				on_( '<' )	do_( set_target_any, "target<" )
				on_( ':' )	do_( set_source_any, "source-medium->target:" )
				on_( ' ' )	do_( set_source_any, "source-medium->target" )
				on_( '\t' )	do_( set_source_any, "source-medium->target" )
				on_( ']' )	do_( set_source_any, pop_state )
				on_other	do_( set_source_any, pop_state )
				end
			in_( "~ " ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( nop, same )
				on_( '\t' )	do_( nop, same )
				on_( ']' )	do_( set_source_null, pop_state )
				on_other	do_( set_source_null, pop_state )
				end
		in_( "." ) bgn_
			on_( '-' )	do_( set_source_any, "source-" )
			on_( '<' )	do_( set_target_any, "target<" )
			on_( ':' )	do_( set_source_any, "source-medium->target:" )
			on_( ' ' )	do_( set_source_any, "source-medium->target" )
			on_( '\t' )	do_( set_source_any, "source-medium->target" )
			on_( ']' )	do_( set_source_any, pop_state )
			on_( '\n' )	do_( set_source_any, pop_state )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_( '?' )	do_( read_shorty, "source-medium->target" )
			on_( '!' )	do_( read_shorty, "source-medium->target" )
			on_( '*' )	do_( read_shorty, "source-medium->target" )
			on_( '_' )	do_( read_shorty, "source-medium->target" )
			on_( '~' )	do_( read_shorty, "source-medium->target" )
			on_( '%' )	do_( read_shorty, "source-medium->target" )
			on_( '[' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( read_shorty, "source-medium->target" )
			end

		in_( "[]" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( '-' )	do_( set_source_pop, "source-" )
			on_( '<' )	do_( swap_source_target, "target<" )
			on_( ':' )	do_( set_source_pop, "source-medium->target:" )
			on_( ' ' )	do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	do_( set_source_pop, "source-medium->target" )
			on_( ']' )	do_( set_source_pop, pop_state )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( set_source_pop, pop_state )
			end

		in_( "identifier" ) bgn_
			on_( '(' )	do_( nothing, pop_state )
			on_( '-' )	do_( set_source_identifier, "source-" )
			on_( '<' )	do_( set_target_identifier, "target<" )
			on_( ':' )	do_( set_source_identifier, "source-medium->target:" )
			on_( ' ' )	do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_source_identifier, "source-medium->target" )
			on_( ']' )	do_( set_source_identifier, pop_state )
			on_( '.' )	do_( read_shorty, "source-medium->target" )
			on_other	do_( set_source_identifier, pop_state )
			end

		in_( "%" ) bgn_
			on_( '!' )	do_( nop, "%!" )
			on_( '?' )	do_( nop, "%?" )
#ifdef SUBVAR
			on_other	do_( read_variable_ref, "%identifier" )
#else
			on_other	do_( read_1, "%identifier" )
#endif
			end
			in_( "%!" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( '-' )	do_( set_source_this, "source-" )
				on_( '<' )	do_( set_target_this, "target<" )
				on_( ':' )	do_( set_source_this, "source-medium->target:" )
				on_( ' ' )	do_( set_source_this, "source-medium->target" )
				on_( '\t' )	do_( set_source_this, "source-medium->target" )
				on_( ']' )	do_( set_source_this, pop_state )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_this, pop_state )
				end
			in_( "%?" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( '-' )	do_( set_source_variator, "source-" )
				on_( '<' )	do_( set_target_variator, "target<" )
				on_( ':' )	do_( set_source_variator, "source-medium->target:" )
				on_( ' ' )	do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	do_( set_source_variator, "source-medium->target" )
				on_( ']' )	do_( set_source_variator, pop_state )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_variator, pop_state )
				end
			in_( "%identifier" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( '-' )	do_( set_source_variable, "source-" )
				on_( '<' )	do_( set_target_variable, "target<" )
				on_( ':' )	do_( set_source_variable, "source-medium->target:" )
				on_( ' ' )	do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	do_( set_source_variable, "source-medium->target" )
				on_( ']' )	do_( set_source_variable, pop_state )
				on_( '.' )	do_( read_shorty, "source-medium->target" )
				on_other	do_( set_source_variable, pop_state )
				end

	in_( "source-" ) bgn_
		on_( '*' )	do_( set_flag_active, "source-*" )
		on_( '_' )	do_( set_flag_inactive, "source-_" )
		on_( '~' )	do_( set_flag_not, "source-~" )
		on_( '[' )	do_( push, "source-[]" )
		on_( '-' )	do_( set_medium_null, "source-medium-" )
		on_( '.' )	do_( nop, "source-." )
		on_( '%' )	do_( nop, "source-%" )
		on_( '?' )	do_( nop, "source-?" )
		on_other	do_( read_1, "source-identifier" )
		end
		in_( "source-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-~" )
			on_( '[' )	do_( push, "source-[]" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-~" )
			on_( '[' )	do_( push, "source-[]" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-~" ) bgn_
			on_( '-' )	do_( set_medium_null, "source-medium-" )
			on_( '[' )	do_( push, "source-[]" )
			on_( '.' )	do_( nop, "source-." )
			on_( '%' )	do_( nop, "source-%" )
			on_other	do_( read_1, "source-identifier" )
			end
		in_( "source-?" ) bgn_
			on_( '-' )	do_( set_medium_mark, "source-medium-" )
			on_other	do_( error, "" )
			end
		in_( "source-[]" ) bgn_
			on_( '-' )	do_( set_medium_pop, "source-medium-" )
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
#ifdef SUBVAR
			on_other	do_( read_variable_ref, "source-%identifier" )
#else
			on_other	do_( read_1, "source-%identifier" )
#endif
			end
			in_( "source-%!" ) bgn_
				on_( '-' )	do_( set_medium_this, "source-medium-" )
				on_other	do_( error, "" )
				end
			in_( "source-%?" ) bgn_
				on_( '-' )	do_( set_medium_variator, "source-medium-" )
				on_other	do_( error, "" )
				end
			in_( "source-%identifier" ) bgn_
				on_( '-' )	do_( set_medium_variable, "source-medium-" )
				on_other	do_( error, "" )
				end

	in_( "source-medium-" ) bgn_
		on_( '>' )	do_( nop, "source-medium->" )
		on_other	do_( error, "" )
		end

	in_( "source-medium->" ) bgn_
		on_( '(' )	do_( error, "" )
		on_( '*' )	do_( set_flag_active, "source-medium->*" )
		on_( '_' )	do_( set_flag_inactive, "source-medium->_" )
		on_( '~' )	do_( set_flag_not, "source-medium->~" )
		on_( ' ' )	do_( set_target_null, "source-medium->target" )
		on_( '\t' )	do_( set_target_null, "source-medium->target" )
		on_( ']' )	do_( set_target_null, pop_state )
		on_( '[' )	do_( push, "source-medium->[]" )
		on_( '%' )	do_( nop, "source-medium->%" )
		on_( '.' )	do_( set_target_any, "source-medium->target" )
		on_( '?' )	do_( set_target_mark, "source-medium->target" )
		on_( ':' )	do_( set_target_null, "source-medium->target:" )
		on_( '(' )	do_( error, "" )
		on_separator	do_( set_target_null, pop_state )
		on_other	do_( read_1, "source-medium->identifier" )
		end
		in_( "source-medium->*" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->~" )
			on_( '[' )	do_( push, "source-medium->[]" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->_" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->~" )
			on_( '[' )	do_( push, "source-medium->[]" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->~" ) bgn_
			on_( ' ' )	do_( set_target_null, "source-medium->target" )
			on_( '\t' )	do_( set_target_null, "source-medium->target" )
			on_( '(' )	do_( error, "" )
			on_( ']' )	do_( set_target_null, pop_state )
			on_( '[' )	do_( push, "source-medium->[]" )
			on_( '%' )	do_( nop, "source-medium->%" )
			on_( '.' )	do_( set_target_any, "source-medium->target" )
			on_( ':' )	do_( set_target_null, "source-medium->target:" )
			on_separator	do_( set_target_null, pop_state )
			on_other	do_( read_1, "source-medium->identifier" )
			end
		in_( "source-medium->[]" ) bgn_
			on_( ' ' )	do_( set_target_pop, "source-medium->target" )
			on_( '\t' )	do_( set_target_pop, "source-medium->target" )
			on_( ']' )	do_( set_target_pop, pop_state )
			on_( ':' )	do_( set_target_pop, "source-medium->target:" )
			on_other	do_( set_target_pop, pop_state )
			end
		in_( "source-medium->identifier" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_target_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_target_identifier, "source-medium->target" )
			on_( ']' )	do_( set_target_identifier, pop_state )
			on_( ':' )	do_( set_target_identifier, "source-medium->target:" )
			on_other	do_( set_target_identifier, pop_state )
			end
		in_( "source-medium->%" ) bgn_
				on_( '!' )	do_( nop, "source-medium->%!" )
				on_( '?' )	do_( nop, "source-medium->%?" )
#ifdef SUBVAR
				on_other	do_( read_variable_ref, "source-medium->%identifier" )
#else
				on_other	do_( read_1, "source-medium->%identifier" )
#endif
				end
				in_( "source-medium->%!" ) bgn_
					on_( '(' )	do_( error, "" )
					on_( ' ' )	do_( set_target_this, "source-medium->target" )
					on_( '\t' )	do_( set_target_this, "source-medium->target" )
					on_( ']' )	do_( set_target_this, pop_state )
					on_( ':' )	do_( set_target_this, "source-medium->target:" )
					on_other	do_( set_target_this, pop_state )
					end
				in_( "source-medium->%?" ) bgn_
					on_( '(' )	do_( error, "" )
					on_( ' ' )	do_( set_target_variator, "source-medium->target" )
					on_( '\t' )	do_( set_target_variator, "source-medium->target" )
					on_( ']' )	do_( set_target_variator, pop_state )
					on_( ':' )	do_( set_target_variator, "source-medium->target:" )
					on_other	do_( set_target_variator, pop_state )
					end
				in_( "source-medium->%identifier" ) bgn_
					on_( '(' )	do_( error, "" )
					on_( ' ' )	do_( set_target_variable, "source-medium->target" )
					on_( '\t' )	do_( set_target_variable, "source-medium->target" )
					on_( ']' )	do_( set_target_variable, pop_state )
					on_( ':' )	do_( set_target_variable, "source-medium->target:" )
					on_other	do_( set_target_variable, pop_state )
					end

	in_( "target<" ) bgn_
		on_( '-' )	do_( nop, "target<-" )
		on_other	do_( error, "" )
		end

	in_( "target<-" ) bgn_
		on_( '*' )	do_( set_flag_active, "target<-*" )
		on_( '_' )	do_( set_flag_inactive, "target<-_" )
		on_( '~' )	do_( set_flag_not, "target<-~" )
		on_( '[' )	do_( push, "target<-[]" )
		on_( '-' )	do_( set_medium_null, "target<-medium-" )
		on_( '.' )	do_( set_medium_any, "target<-." )
		on_( '%' )	do_( nop, "target<-%" )
		on_( '?' )	do_( nop, "target<-?" )
		on_other	do_( read_1, "target<-identifier" )
		end
		in_( "target<-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-~" )
			on_( '[' )	do_( push, "target<-[]" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-~" )
			on_( '[' )	do_( push, "target<-[]" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-~" ) bgn_
			on_( '[' )	do_( push, "target<-[]" )
			on_( '-' )	do_( set_medium_null, "target<-medium-" )
			on_( '.' )	do_( set_medium_any, "target<-medium" )
			on_( '%' )	do_( nop, "target<-%" )
			on_other	do_( read_1, "target<-identifier" )
			end
		in_( "target<-?" ) bgn_
			on_( '-' )	do_( set_medium_mark, "target<-medium-" )
			on_other	do_( error, "" )
			end
		in_( "target<-[]" ) bgn_
			on_( '-' )	do_( set_medium_pop, "target<-medium-" )
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
#ifdef SUBVAR
			on_other	do_( read_variable_ref, "target<-%identifier" )
#else
			on_other	do_( read_1, "target<-%identifier" )
#endif
			end
			in_( "target<-%!" ) bgn_
				on_( '-' )	do_( set_medium_this, "target<-medium-" )
				on_other	do_( error, "" )
				end
			in_( "target<-%?" ) bgn_
				on_( '-' )	do_( set_medium_variator, "target<-medium-" )
				on_other	do_( error, "" )
				end
			in_( "target<-%identifier" ) bgn_
				on_( '-' )	do_( set_medium_variable, "target<-medium-" )
				on_other	do_( error, "" )
				end

	in_( "target<-medium-" ) bgn_
		on_( '(' )	do_( error, "" )
		on_( '*' )	do_( set_flag_active, "target<-medium-*" )
		on_( '_' )	do_( set_flag_inactive, "target<-medium-_" )
		on_( '~' )	do_( set_flag_not, "target<-medium-~" )
		on_( ' ' )	do_( set_source_null, "source-medium->target" )
		on_( '\t' )	do_( set_source_null, "source-medium->target" )
		on_( ']' )	do_( set_source_null, pop_state )
		on_( '[' )	do_( push, "target<-medium-[]" )
		on_( '%' )	do_( nop, "target<-medium-%" )
		on_( '.' )	do_( set_source_any, "source-medium->target" )
		on_( '?' )	do_( set_source_mark, "source-medium->target" )
		on_( ':' )	do_( set_source_null, "source-medium->target:" )
		on_separator	do_( set_source_null, pop_state )
		on_other	do_( read_1, "target<-medium-identifier" )
		end
		in_( "target<-medium-*" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	do_( push, "target<-medium-[]" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-_" ) bgn_
			on_( '~' )	do_( set_flag_not, "target<-medium-~" )
			on_( '[' )	do_( push, "target<-medium-[]" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-~" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_source_null, "source-medium->target" )
			on_( '\t' )	do_( set_source_null, "source-medium->target" )
			on_( ']' )	do_( set_source_null, pop_state )
			on_( '[' )	do_( push, "target<-medium-[]" )
			on_( '%' )	do_( nop, "target<-medium-%" )
			on_( '.' )	do_( set_source_any, "source-medium->target" )
			on_( ':' )	do_( set_source_null, "source-medium->target:" )
			on_separator	do_( set_source_null, pop_state )
			on_other	do_( read_1, "target<-medium-identifier" )
			end
		in_( "target<-medium-[]" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_source_pop, "source-medium->target" )
			on_( '\t' )	do_( set_source_pop, "source-medium->target" )
			on_( ']' )	do_( set_source_pop, pop_state )
			on_( ':' )	do_( set_source_pop, "source-medium->target:" )
			on_other	do_( set_source_pop, pop_state )
			end
		in_( "target<-medium-identifier" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_source_identifier, "source-medium->target" )
			on_( '\t' )	do_( set_source_identifier, "source-medium->target" )
			on_( ']' )	do_( set_source_identifier, pop_state )
			on_( ':' )	do_( set_source_identifier, "source-medium->target:" )
			on_other	do_( set_source_identifier, pop_state )
			end
		in_( "target<-medium-%" ) bgn_
			on_( '!' )	do_( nop, "target<-medium-%!" )
			on_( '?' )	do_( nop, "target<-medium-%?" )
#ifdef SUBVAR
			on_other	do_( read_variable_ref, "target<-medium-%identifier" )
#else
			on_other	do_( read_1, "target<-medium-%identifier" )
#endif
			end
			in_( "target<-medium-%!" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_source_this, "source-medium->target" )
				on_( '\t' )	do_( set_source_this, "source-medium->target" )
				on_( ']' )	do_( set_source_this, pop_state )
				on_( ':' )	do_( set_source_this, "source-medium->target:" )
				on_other	do_( set_source_this, pop_state )
				end
			in_( "target<-medium-%?" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_source_variator, "source-medium->target" )
				on_( '\t' )	do_( set_source_variator, "source-medium->target" )
				on_( ']' )	do_( set_source_variator, pop_state )
				on_( ':' )	do_( set_source_variator, "source-medium->target:" )
				on_other	do_( set_source_variator, pop_state )
				end
			in_( "target<-medium-%identifier" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_source_variable, "source-medium->target" )
				on_( '\t' )	do_( set_source_variable, "source-medium->target" )
				on_( ']' )	do_( set_source_variable, pop_state )
				on_( ':' )	do_( set_source_variable, "source-medium->target:" )
				on_other	do_( set_source_variable, pop_state )
				end

	in_( "source-medium->target" ) bgn_
		on_( '(' )	do_( error, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( ']' )	do_( nop, pop_state )
		on_( ':' )	do_( nop, "source-medium->target:" )
		on_other	do_( nothing, pop_state )
		end

	in_( "source-medium->target:" ) bgn_
		on_( '(' )	do_( error, "" )
		on_( '*' )	do_( set_flag_active, "source-medium->target: *" )
		on_( '_' )	do_( set_flag_inactive, "source-medium->target: _" )
		on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( '[' )	do_( push, "source-medium->target: []" )
		on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
		on_( '%' )	do_( nop, "source-medium->target: %" )
		on_separator	do_( nothing, pop_state )
		on_other	do_( read_1, "source-medium->target: identifier" )
		end
		in_( "source-medium->target: *" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	do_( push, "source-medium->target: []" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: _" ) bgn_
			on_( '~' )	do_( set_flag_not, "source-medium->target: ~" )
			on_( '[' )	do_( push, "source-medium->target: []" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: ~" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_instance_null, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_null, "source-medium->target: instance" )
			on_( '[' )	do_( push, "source-medium->target: []" )
			on_( '.' )	do_( set_instance_any, "source-medium->target: instance" )
			on_( '%' )	do_( nop, "source-medium->target: %" )
			on_separator	do_( set_instance_null, pop_state )
			on_other	do_( read_1, "source-medium->target: identifier" )
			end
		in_( "source-medium->target: []" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_instance_pop, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_pop, "source-medium->target: instance" )
			on_( ']' )	do_( set_instance_pop, pop_state )
			on_other	do_( set_instance_pop, pop_state )
			end
		in_( "source-medium->target: %" ) bgn_
			on_( '!' )	do_( nop, "source-medium->target: %!" )
			on_( '?' )	do_( nop, "source-medium->target: %?" )
			on_( '[' )	do_( read_as_sub, "source-medium->target: instance" )
#ifdef SUBVAR
			on_other	do_( read_variable_ref, "source-medium->target: %identifier" )
#else
			on_other	do_( read_1, "source-medium->target: %identifier" )
#endif
			end
			in_( "source-medium->target: %!" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_instance_this, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_this, "source-medium->target: instance" )
				on_( ']' )	do_( set_instance_this, pop_state )
				on_other	do_( set_instance_this, pop_state )
				end
			in_( "source-medium->target: %?" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_instance_variator, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_variator, "source-medium->target: instance" )
				on_( ']' )	do_( set_instance_variator, pop_state )
				on_other	do_( set_instance_variator, pop_state )
				end
			in_( "source-medium->target: %identifier" ) bgn_
				on_( '(' )	do_( error, "" )
				on_( ' ' )	do_( set_instance_variable, "source-medium->target: instance" )
				on_( '\t' )	do_( set_instance_variable, "source-medium->target: instance" )
				on_( ']' )	do_( set_instance_variable, pop_state )
				on_other	do_( set_instance_variable, pop_state )
				end
		in_( "source-medium->target: identifier" ) bgn_
			on_( '(' )	do_( error, "" )
			on_( ' ' )	do_( set_instance_identifier, "source-medium->target: instance" )
			on_( '\t' )	do_( set_instance_identifier, "source-medium->target: instance" )
			on_( ']' )	do_( set_instance_identifier, pop_state )
			on_other	do_( set_instance_identifier, pop_state )
			end

	in_( "source-medium->target: instance" ) bgn_
		on_( '(' )	do_( error, "" )
		on_( ' ' )	do_( nop, same )
		on_( '\t' )	do_( nop, same )
		on_( ']' )	do_( nothing, pop_state )
		on_other	do_( nothing, pop_state )
		end
	end
	}
	while ( strcmp( state, "" ) );

	do_( parser_exit, same );

#ifdef DEBUG
	outputf( Debug, "exiting read_expression '%c'", event );
#endif

	return event;
}

