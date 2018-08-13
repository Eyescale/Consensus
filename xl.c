#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "expression.h"
#include "expression_util.h"
#include "input.h"
#include "output_util.h"
#include "string_util.h"
#include "xl.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	l_sub
---------------------------------------------------------------------------*/
Literal *
l_sub( Literal *l, int count )
/*
	Assumptions: l != NULL, count < 3
*/
{
	if (( l->e ) && (l->e->sub[ 1 ].result.none ))
		l = &l->e->sub[ 0 ];
	return ( l->e == NULL ) ? NULL : &l->e->sub[ count ];
}

/*---------------------------------------------------------------------------
	l_inst
---------------------------------------------------------------------------*/
Entity *
l_inst( Literal *l )
{
	if (( l->e ) && (l->e->sub[ 1 ].result.none ))
		l = &l->e->sub[ 0 ];
	if ( l->e == NULL ) {
		if ( l->result.identifier.type == NullIdentifier )
			return CN.nil;
		else {
			char *name = l->result.identifier.value;
			Entity *instance = cn_entity( name );
			if ( instance == NULL )
				instance = cn_new( strdup( name ) );
			return instance;
		}
	}
	ExpressionSub *sub = l->e->sub;
	Entity *cmp[ 3 ];
	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].e ) {
			Expression *e = sub[ i ].e;
			cmp[ i ] = l_inst( &e->sub[ 3 ] );
		}
		else if ( sub[ i ].result.identifier.type == NullIdentifier )
			cmp[ i ] = CN.nil;
		else {
			char *name = sub[ i ].result.identifier.value;
			cmp[ i ] = cn_entity( name );
			if ( cmp[ i ] == NULL )
				cmp[ i ] = cn_new( strdup( name ) );
		}
	}
	Entity *instance = cn_instance( cmp[ 0 ], cmp[ 1 ], cmp[ 2 ] );
	if ( instance == NULL ) {
		instance = cn_instantiate( cmp[ 0 ], cmp[ 1 ], cmp[ 2 ], l->e->result.twist );
	}
	return instance;
}

/*---------------------------------------------------------------------------
	s2x	- string to expression
---------------------------------------------------------------------------*/
Expression *
s2x( char *string, _context *context )
{
	struct { Expression *expression; int mode; } backup;
	backup.expression = context->expression.ptr;
	backup.mode = context->expression.mode;
	context->expression.ptr = NULL;
	context->expression.mode = BuildMode;

	int level = context->input.level;
	push_input( "", string, InstructionOne, context );
	int event = read_expression( base, 0, &same, context );

	// DO_LATER: that would be the place to do something clever
	// in case read_expression returns before the end of string
	while ( event != '\n' )
		switch ( event ) {
		case ' ':
		case '\t':
			event = read_input( base, 0, &same, context );
			break;
		default:
			freeExpression( context->expression.ptr );
			Expression *e = newExpression( context );
			e->sub[ 0 ].result.identifier.type = DefaultIdentifier;
			e->sub[ 0 ].result.identifier.value = strdup( string );
			e->sub[ 0 ].result.none = 0;
			context->expression.ptr = e;
			event = '\n';
		}

	while ( context->input.level > level )
		pop_input( base, 0, &same, context );

	Expression *expression = context->expression.ptr;
	context->expression.ptr = backup.expression;
	context->expression.mode = backup.mode;
	return expression;
}

/*---------------------------------------------------------------------------
	l2e
---------------------------------------------------------------------------*/
Entity *
l2e( Literal *l )
{
	if (( l->e ) && (l->e->sub[ 1 ].result.none ))
		l = &l->e->sub[ 0 ];
	if ( l->e == NULL ) {
		return ( l->result.identifier.type == NullIdentifier ) ?
			CN.nil : cn_entity( l->result.identifier.value );
	}
	ExpressionSub *sub = l->e->sub;
	Entity *cmp[ 3 ];
	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].e ) {
			Expression *e = sub[ i ].e;
			cmp[ i ] = l2e( &e->sub[ 3 ] );
		}
		else if ( sub[ i ].result.identifier.type == NullIdentifier )
			cmp[ i ] = CN.nil;
		else {
			char *name = sub[ i ].result.identifier.value;
			cmp[ i ] = cn_entity( name );
		}
		if ( cmp[ i ] == NULL ) return NULL;
	}
	return cn_instance( cmp[ 0 ], cmp[ 1 ], cmp[ 2 ] );
}

/*---------------------------------------------------------------------------
	lcmp
---------------------------------------------------------------------------*/
int
lcmp( Literal *l1, Literal *l2 )
{
	if (( l1->e ) && ( l1->e->sub[ 1 ].result.none ))
		l1 = &l1->e->sub[ 0 ];
	if (( l2->e ) && ( l2->e->sub[ 1 ].result.none ))
		l2 = &l2->e->sub[ 0 ];
	if ( l1->e == NULL ) {
		return	( l2->e != NULL ) ?
			1 :
			l1->result.identifier.type == NullIdentifier ?
			l2->result.identifier.type != NullIdentifier :
			l2->result.identifier.type == NullIdentifier ?
			1 :
			strcmp( l1->result.identifier.value, l2->result.identifier.value );
	}
	else if ( l2->e == NULL ) {
		return 1;
	}
	else {
		for ( int i=0; i<3; i++ ) {
			if ( lcmp( &l1->e->sub[ i ], &l2->e->sub[ i ] ) )
				return 1;
		}
		return 0;
	}
}

/*---------------------------------------------------------------------------
	ldup
---------------------------------------------------------------------------*/
static Expression *xldup( ExpressionSub *s, ExpressionSub *super );
Literal *
ldup( Literal *l )
{
	Expression *e = xldup( l, NULL );
	return &e->sub[ 3 ];
}
static Expression *
xldup( ExpressionSub *s, ExpressionSub *super )
{
	Expression *expression = (Expression *) calloc( 1, sizeof(Expression));
	ExpressionSub *sub = expression->sub;
	sub[ 3 ].result.any = 1;
	sub[ 3 ].e = expression;
	sub[ 3 ].super = super;
	if ( s->e == NULL ) {
		switch ( s->result.identifier.type ) {
		case NullIdentifier:
			sub[ 0 ].result.identifier.type = NullIdentifier;
			break;
		case DefaultIdentifier:
			sub[ 0 ].result.identifier.type = DefaultIdentifier;
			sub[ 0 ].result.identifier.value = strdup( s->result.identifier.value );
			break;
		default:
			break;
		}
		sub[ 1 ].result.none = 1;
		sub[ 2 ].result.none = 1;
	}
	else {
		expression->result.twist = s->e->result.twist;
		for ( int i=0; i<3; i++ ) {
			ExpressionSub *s_sub = &s->e->sub[ i ];
			if ( s_sub->e == NULL ) {
				switch ( s_sub->result.identifier.type ) {
				case NullIdentifier:
					sub[ i ].result.identifier.type = NullIdentifier;
					break;
				case DefaultIdentifier:
					sub[ i ].result.identifier.type = DefaultIdentifier;
					sub[ i ].result.identifier.value = strdup( s_sub->result.identifier.value );
					break;
				default:
					sub[ i ].result.none = 1;
					break;
				}
			} else {
				sub[ i ].e = xldup( s_sub, &sub[ 3 ] );
			}
		}
	}
	return expression;
}

/*---------------------------------------------------------------------------
	e2l	- entity to literal
---------------------------------------------------------------------------*/
static Expression *e2xl( Entity *e );
Literal *
e2l( Entity *entity )
{
	Expression *xl = e2xl( entity );
	return (( xl == NULL ) ? NULL : &xl->sub[ 3 ] );
}

/*---------------------------------------------------------------------------
	e2xl	- entity to expression (literalized)
---------------------------------------------------------------------------*/
static Expression * express( Entity *e, ExpressionSub *super );
static Expression *
e2xl( Entity *e )
{
	return express( e, NULL );
}
static Expression *
express( Entity *e, ExpressionSub *super )
{
	if ( e == NULL ) return NULL;

	Expression *expression = (Expression *) calloc( 1, sizeof(Expression));
	expression->result.twist = e->twist;
	ExpressionSub *sub = expression->sub;
	sub[ 3 ].result.any = 1;
	sub[ 3 ].e = expression;
	sub[ 3 ].super = super;

	if ( e == CN.nil ) {
		sub[ 0 ].result.identifier.type = NullIdentifier;
		sub[ 1 ].result.none = 1;
		sub[ 2 ].result.none = 1;
	}
	else
	{
		char *name = cn_name( e );
		if ( name != NULL ) {
			sub[ 0 ].result.identifier.type = DefaultIdentifier;
			sub[ 0 ].result.identifier.value = strdup( name );
			sub[ 1 ].result.none = 1;
			sub[ 2 ].result.none = 1;
		}
		else for ( int i=0; i<3; i++ ) {
			name = cn_name( e->sub[ i ] );
			if ( name != NULL ) {
				sub[ i ].result.identifier.type = DefaultIdentifier;
				sub[ i ].result.identifier.value = strdup( name );
			} else {
				sub[ i ].e = express( e->sub[ i ], &sub[ 3 ] );
				sub[ i ].result.none = ( sub[ i ].e == NULL );
			}
		}
	}
	return expression;
}

/*---------------------------------------------------------------------------
	xlisable
---------------------------------------------------------------------------*/
/*
	test if the passed expression is xlisable, ie.
	1. no mark nor flag of any kind
	2. sub[ 3 ].result.any = 1
	3. for i in { 0, 1, 2 }: sub [ i ].result.identifier.type verifies
		. sub[ i ] is not a variable
		. sub[ i ] is not a list of any kind
		. sub[ i ].e is xlisable
*/
int
xlisable( Expression *expression )
{
	if ( expression == NULL ) return 0;
	if ( expression->result.as_sub || expression->result.as_sup )
		return 0;
	if ( !just_any( expression, 3 ) )
		return 0;
	ExpressionSub *sub = expression->sub;
	if ( sub[ 3 ].e == expression )
		return 1;
	for ( int i=0; i<3; i++ ) {
		if ( flagged( expression, i ) )
			return 0;
		switch ( sub[ i ].result.identifier.type ) {
		case NullIdentifier:
			break;
		case VariableIdentifier:
		case ExpressionCollect:
		case LiteralResults:
		case EntityResults:
		case StringResults:
			return 0;
		case DefaultIdentifier:
			if ( sub[ i ].result.identifier.value == NULL )
				return 0;
			break;
		default:
			return xlisable( sub[ i ].e );
		}
	}
	return 1;
}

/*---------------------------------------------------------------------------
	xl	- expression literalized
---------------------------------------------------------------------------*/
static Expression *xlise( Expression *expression, ExpressionSub *super );
Literal *
xl( Expression *expression )
/*
	assumption: the expression is xlisable (see above)
*/
{
	xlise( expression, NULL );
	return &expression->sub[ 3 ];
}
static Expression *
xlise( Expression *expression, ExpressionSub *super )
{
	if ( expression == NULL ) return NULL;

	ExpressionSub *sub = expression->sub;
	if ( sub[ 3 ].e == expression )
		return expression;

	sub[ 3 ].e = expression;
	sub[ 3 ].super = super;

	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].result.identifier.type )
			continue;
		xlise( sub[ i ].e, &sub[ 3 ] );
	}
	return expression;
}

/*---------------------------------------------------------------------------
	literalisable
---------------------------------------------------------------------------*/
static int test_literalisable( Expression *expression );
int
literalisable( Expression *expression )
{
	if ( expression->result.marked ) return 0;
	return test_literalisable( expression );
}
static int
test_literalisable( Expression *expression )
/*
	test if the passed expression is literalisable, ie.
	1. no mark nor flag of any kind
	2. no instance
	3. for i in { 0, 1, 2 }:
		. sub[ i ] is not a variable
		. sub[ i ].e is literalisable (if there is)
		. in case of collection: all items are literalisable
*/
{
	if ( expression == NULL ) return 0;
	if ( expression->result.as_sub || expression->result.as_sup )
		return 0;
	if ( !just_any( expression, 3 ) )
		return 0;
	ExpressionSub *sub = expression->sub;
	if ( sub[ 3 ].e == expression )
		return 1;
	for ( int i=0; i<3; i++ ) {
		if ( sub[ i ].result.none )
			continue;
		if ( flagged( expression, i ) )
			return 0;
		switch ( sub[ i ].result.identifier.type ) {
		case NullIdentifier:
		case EntityResults:
		case LiteralResults:
		case DefaultIdentifier:
			break;
		case VariableIdentifier:
		case StringResults:
			return 0;	// DO_LATER
		case ExpressionCollect:
			for ( listItem *j = sub[ i ].result.identifier.value; j!=NULL; j=j->next )
				if ( !literalisable( j->ptr ) ) return 0;
			break;
		default:
			return test_literalisable( sub[ i ].e );
		}
	}
	return 1;
}

/*---------------------------------------------------------------------------
	x2l	- expression to literals
---------------------------------------------------------------------------*/
/*
	assumption: the expression is literalisable (see above)
*/
listItem *
x2l( Expression *expression, _context *context )
{
	listItem *literals = NULL;
	listItem *strings = x2s( expression, context, 1 );

	for ( listItem *i = strings; i!=NULL; i=i->next ) {
		Expression *expression = s2x( i->ptr, context );
		if (( expression )) {
			addItem( &literals, xl( expression ) );
		}
		free( i->ptr );
	}

	freeListItem( &strings );
	return literals;
}

/*---------------------------------------------------------------------------
	x2s	- expression to strings
---------------------------------------------------------------------------*/
listItem *
x2s( Expression *expression, _context *context, int expand )
{
	push_output( "", NULL, StringOutput, context );
	int event = output_expression( ExpressionAll, expression );
	pop_output( context, 0 );

	if ( event < 0 ) return NULL;
	if ( expand ) {
		listItem *list = string_expand( context->output.slist->ptr, NULL );
		for ( listItem *i = context->output.slist; i!=NULL; i=i->next )
			free( i->ptr );
		freeListItem( &context->output.slist );
		context->output.slist = list;
	}

	// the resulting string(s) is in context->output.slist
	listItem *list = context->output.slist;
	context->output.slist = NULL;

	return list;
}

/*---------------------------------------------------------------------------
	E2L	- entities to literals
---------------------------------------------------------------------------*/
int
E2L( listItem **list, listItem *filter, _context *context )
{
	listItem *results = NULL;
	if ( filter == NULL )
	{
		for ( listItem *i = *list; i!=NULL; i=i->next ) {
			Literal *l = e2l( i->ptr );
			if (( l )) addItem( &results, l );
		}
	}
	else for ( listItem *i = *list; i!=NULL; i=i->next ) {
		Literal *l = e2l( i->ptr );
		if ( l == NULL ) continue;
		for ( listItem *j = filter; j!=NULL; j=j->next )
			if ( !lcmp( l, i->ptr ) )
				addItem( &results, j->ptr );
	}
	freeListItem( list );
	*list = results;
	return ( results != NULL );
}

/*---------------------------------------------------------------------------
	L2E	- literals to entities
---------------------------------------------------------------------------*/
int
L2E( listItem **list, listItem *filter, _context *context )
{
	listItem *results = NULL;
	if ( filter == NULL )
	{
		for ( listItem *i = *list; i!=NULL; i=i->next ) {
			Entity *e = l2e( i->ptr );
			if (( e )) addItem( &results, e );
		}
	}
	else for ( listItem *i = *list; i!=NULL; i=i->next )
	{
		Entity *e = l2e( i->ptr );
		if ( e == NULL ) continue;
		for ( listItem *j = filter; j!=NULL; j=j->next )
			if ( e == (Entity *) j->ptr )
				addItem( &results, j->ptr );
	}
	freeLiterals( list );
	*list = results;
	return ( results != NULL );
}

/*---------------------------------------------------------------------------
	X2L	- expressions to literals
---------------------------------------------------------------------------*/
/*
	assumption: all expressions are literalisable (see above)
*/
listItem *
X2L( listItem *expressions, _context *context )
{
	listItem *literals = NULL;
	for ( listItem *i = expressions; i!=NULL; i=i->next ) {
		listItem *list = x2l( i->ptr, context );
		for ( listItem *j = list; j!=NULL; j=j->next )
			addItem( &literals, j->ptr );
		freeListItem( &list );
	}
	return literals;
}

/*---------------------------------------------------------------------------
	Xt2L	- expressions test & set to literals
---------------------------------------------------------------------------*/
/*
	assumption: all expressions are literalisable (see above)
*/
listItem *
Xt2L( listItem *expressions, _context *context )
{
	// see if all expressions can be made into literals
	for ( listItem *i = expressions; i!=NULL; i=i->next )
		if ( !literalisable( i->ptr ) )
			return NULL;

	// see below DO_LATER: xlready optimization
	listItem *literals = NULL;
	for ( listItem *i = expressions; i!=NULL; i=i->next ) {
		listItem *list = x2l( i->ptr, context );
		for ( listItem *j = list; j!=NULL; j=j->next )
			addItem( &literals, j->ptr );
		freeListItem( &list );
	}
	return literals;
}

/*---------------------------------------------------------------------------
	S2X	- strings to expressions, possibly literals
---------------------------------------------------------------------------*/
listItem *
S2X( listItem *strings, int *type, _context *context )
{
	listItem *expressions = NULL;
	for ( listItem *i = strings; i!=NULL; i=i->next )
	{
		Expression *expression = s2x( i->ptr, context );
		if (( expression )) addItem( &expressions, expression );
	}
	if (( expressions == NULL ) || ( type == NULL ))
		return expressions;

	// see if all expressions can be made into literals
	*type = ExpressionCollect;
	for ( listItem *i = expressions; i!=NULL; i=i->next )
		if ( !literalisable( i->ptr ) )
			return expressions;

#ifdef DO_LATER
	// a special case, but quite general, is when each expression is
	// made of a collection as source term of xlready expressions.
	// The conversion to literals would be substantially optimized
	// by flagging such expressions as xlready during or after the
	// read_expression() traversal invoked by s2x().
	// Here we are reconverting each sub-expression to string,
	// expanding as we go (though there is nothing to expand)
	// as if the original string(s) was created via
	// 	> string: whatever
#endif
	*type = LiteralResults;
	listItem *literals = X2L( expressions, context );
	freeExpressions( &expressions );
	return literals;
}

