#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "expression.h"
#include "variables.h"
#include "expression_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	just_any
---------------------------------------------------------------------------*/
int
just_any( Expression *expression, int i )
{
	ExpressionSub *sub = expression->sub;
	return	sub[ i ].result.any &&
		!sub[ i ].result.active &&
		!sub[ i ].result.inactive &&
		!sub[ i ].result.not;
}

/*---------------------------------------------------------------------------
	freeLiteral
---------------------------------------------------------------------------*/
void
freeLiteral( listItem **list )
{
	for ( listItem *i = *list; i!=NULL; i=i->next ) {
		ExpressionSub *s = (ExpressionSub *) i->ptr;
		freeExpression( s->e );
	}
	freeListItem( list );
}

/*---------------------------------------------------------------------------
	translateFromLiteral
---------------------------------------------------------------------------*/
int
translateFromLiteral( listItem **list, int as_sub, _context *context )
{
	listItem *last_results = NULL;
	if ( list == &context->expression.results ) {
		context->expression.results = NULL;
	}
	for ( listItem *i = *list; i!=NULL; i=i->next )
	{
		Expression *expression = ((ExpressionSub *) i->ptr )->e;
		int success = expression_solve( expression, as_sub, context );
		freeExpression( expression );
		if ( success > 0 ) {
			last_results = catListItem( context->expression.results, last_results );
			context->expression.results = NULL;
		}
	}
	freeListItem( list );
	*list = last_results;
	return ( last_results != NULL );
}

/*---------------------------------------------------------------------------
	freeExpression
---------------------------------------------------------------------------*/
static void
nullify( Expression *expression, listItem **residue )
{
	// residues are earlier versions of an expression which has superceded itself,
	// possibly more than once. Each must be freed only once.

	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		if ( sub[ i ].result.any || sub[ i ].result.none )
			continue;

		if ( sub[ i ].e == NULL )
			continue;

		nullify( sub[ i ].e, residue );

		if (( sub[ i ].result.identifier.type == VariableIdentifier ) && ( sub[ i ].result.identifier.value == NULL ))
		{
			addIfNotThere( residue, sub[ i ].e );
			sub[ i ].e = NULL;
		}
	}
}

static void
erase( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		if ( sub[ i ].result.any || sub[ i ].result.none )
			continue;

		erase( sub[ i ].e );

		switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			if (( sub[ i ].result.identifier.value == variator_symbol ) || ( sub[ i ].result.identifier.value == this_symbol ))
				break;
		case DefaultIdentifier:
			free( sub[ i ].result.identifier.value );
		default:
			break;
		}
	}
	freeListItem( &expression->result.list );
	free( expression );
}

void
freeExpression( Expression *expression )
{
	if ( expression == NULL )
		return;

	listItem *residue = NULL;
	nullify( expression, &residue );
	for ( listItem *i = residue; i!=NULL; i=i->next ) {
		erase( (Expression *) i->ptr );
	}
	erase( expression );
	freeListItem( &residue );
}

/*---------------------------------------------------------------------------
	trace_mark, mark_negated
---------------------------------------------------------------------------*/
int
trace_mark( Expression *expression )
{
	if ( expression == NULL ) return 0;
	if ( expression->result.mark )
		return 1;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		if ( sub[ i ].result.any )
			continue;
		if ( sub[ i ].result.identifier.type == VariableIdentifier )
			continue;
		if ( trace_mark( sub[ i ].e ) ) {
			sub[ i ].result.resolve = 1;
			return 1;
		}
	}
	return 0;
}
int
mark_negated( Expression *expression, int negated )
{
	if ( expression == NULL ) return 0;
	if ( expression->result.mark )
		return negated;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ ) {
		if ( sub[ i ].result.any )
			continue;
		if ( !sub[ i ].result.resolve )
			continue;
		if ( mark_negated( sub[ i ].e, negated || sub[ i ].result.not ) )
			return 1;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	expression_collapse
---------------------------------------------------------------------------*/
static void
collapse( Expression *expression, int *flags, int i, int count, int sub_i )
{
	ExpressionSub *sub = expression->sub;
	Expression *e = sub[ i ].e;
	ExpressionSub *e_sub = e->sub;
	flags[ 0 ] = flags[ 0 ] || e_sub[ 3 ].result.active;
	flags[ 1 ] = flags[ 1 ] || e_sub[ 3 ].result.inactive;
	if ( flags[ 0 ] && flags[ 1 ] )
	{
		; // in this case we don't do anything
	}
	else if ( count == 1 )
	{
#ifdef DEBUG
		output( Debug, "collapse: collapsing %d", count );
#endif
		bcopy( &e_sub[ sub_i ], &sub[ i ], sizeof( ExpressionSub ) );
		free( e );
		sub[ i ].result.active = flags[ 0 ];
		sub[ i ].result.inactive = flags[ 1 ];
		sub[ i ].result.not = flags[ 2 ];
	}
	else
	{
#ifdef DEBUG
		output( Debug, "collapse: collapsing %d", count );
#endif
		expression->result.mark |= e->result.mark;
		expression->result.output_swap = e->result.output_swap;
		for ( int j=0; j<count; j++ ) {
			if ( e_sub[ j ].result.none ) continue;
			bcopy( &e_sub[ j ],  &sub[ j ], sizeof( ExpressionSub ) );
		}
		free( e );
		sub[ 3 ].result.active = flags[ 0 ];
		sub[ 3 ].result.inactive = flags[ 1 ];
	}
}

void
expression_collapse( Expression *expression )
{
	if ( expression == NULL ) return;

	ExpressionSub *sub = expression->sub;
	int flags[ 3 ];

#ifdef DEBUG
	output( Debug, "expression_collapse: entering %0x", (int) expression );
#endif
	if (( sub[ 3 ].e != NULL ) && !sub[ 3 ].result.any &&
	     !sub[ 3 ].result.active && !sub[ 3 ].result.inactive && !sub[ 3 ].result.not &&
	     just_any( expression, 0 ) && just_any( expression, 1 ) && just_any( expression, 2 ))
	{
		// in this case we replace the whole current expression with its sub[ 3 ]
		flags[ 0 ] = 0;
		flags[ 1 ] = 0;
#ifdef DEBUG
		output( Debug, "expression_collapse: collapsing whole..." );
#endif
		collapse( expression, flags, 3, 4, -1 );
	}

	// collapse unnecessary levels
	for ( int i=0; i<4; i++ )	// look at each sub and see if it can be collapsed
	{
		// we want to replace sub[ i ] with its own sub-expression
		if (( sub[ i ].e == NULL ) || sub[ i ].result.any )
			continue;

		Expression *e = sub[ i ].e;
		ExpressionSub *e_sub = e->sub;
		if ( e_sub[ 1 ].result.none && e_sub[ 3 ].result.any && !e->result.as_sub && !e->result.mark )
		{
#ifdef DEBUG
			output( Debug, "expression_collapse: collapsing sub[ %d ]", i );
#endif
			flags[ 0 ] = sub[ i ].result.active || e_sub[ 0 ].result.active;
			flags[ 1 ] = sub[ i ].result.inactive || e_sub[ 0 ].result.inactive;
			flags[ 2 ] = sub[ i ].result.not ^ e_sub[ 0 ].result.not;
			if ( flags[ 0 ] && flags[ 1 ] )
			{
				; // in this case we don't do anything
			}
			else collapse( expression, flags, i, 1, 0 );
		}
	}

	if ( sub[ 0 ].result.none ) {
		int mark = expression->result.mark;
		if ( mark == 8 ) {
			sub[ 0 ].result.any = 1;
			sub[ 0 ].result.none = 0;
		}
		else if ( mark & 7 ) {
#ifdef DEBUG
			output( Debug, "expression_collapse: completing according to mark" );
#endif
			for ( int i=0; i<3; i++ ) {
				sub[ i ].result.any = 1;
				sub[ i ].result.none = 0;
			}
		}
	}

	if ( !sub[ 1 ].result.none )
		return;

	// we want to replace at least the body of the current expression with its sub[ 0 ]
	else if (( sub[ 0 ].e != NULL ) && !sub[ 0 ].result.not )
	{
		flags[ 0 ] = sub[ 3 ].result.active || sub[ 0 ].result.active;
		flags[ 1 ] = sub[ 3 ].result.inactive || sub[ 0 ].result.inactive;
		int as_sub = expression->result.as_sub | sub[ 0 ].e->result.as_sub;
		if ( flags[ 0 ] && flags[ 1 ] )
		{
			; // in this case we don't do anything
		}
		else if (( as_sub & 7 ) ? !( as_sub & 112 ) : 1 )
		{
			if ( sub[ 3 ].result.any )
			{
#ifdef DEBUG
				output( Debug, "expression_collapse: collapsing whole..." );
#endif
				// in this case we replace the whole current expression with its sub[ 0 ]
				collapse( expression, flags, 0, 4, -1 );
				expression->result.as_sub = as_sub;
			}
			else if ( sub[ 0 ].e->sub[ 3 ].result.any )
			{
#ifdef DEBUG
				output( Debug, "expression_collapse: collapsing body..." );
#endif
				// in this case we replace only the body of the current expression with sub[ 0 ]
				collapse( expression, flags, 0, 3, -1 );
				expression->result.as_sub = as_sub;
			}
		}
	}
	// we want to replace the current expression with its sub[ 3 ]
	else if ( sub[ 0 ].result.any && ( sub[ 3 ].e != NULL ) && !sub[ 3 ].result.any )
	{
		flags[ 0 ] = sub[ 0 ].result.active || sub[ 3 ].result.active;
		flags[ 1 ] = sub[ 0 ].result.inactive || sub[ 3 ].result.inactive;
		if ( flags[ 0 ] && flags[ 1 ] )
		{
			; // in this case we don't do anything
		}
		else if ( sub[ 3 ].result.not )
		{
			// in this case we swap sub [ 0 ] and sub[ 3 ] - to be collapsed later
#ifdef DEBUG
			output( Debug, "collapse: swapping sub[ 0 ] and sub[ 3 ]", count );
#endif
			bcopy( &sub[ 3 ], &sub[ 0 ], sizeof( ExpressionSub ) );
			sub[ 0 ].result.active = flags[ 0 ];
			sub[ 0 ].result.inactive = flags[ 1 ];
			bzero( &sub[ 3 ], sizeof( ExpressionSub ) );
			sub[ 3 ].result.any = 1;
		}
		else
		{
#ifdef DEBUG
			output( Debug, "expression_collapse: collapsing whole..." );
#endif
			// in this case we replace the whole current expression with its sub[ 3 ]
			collapse( expression, flags, 3, 4, -1 );
		}
	}
}

/*---------------------------------------------------------------------------
	instantiable
---------------------------------------------------------------------------*/
int
instantiable( Expression *expression, _context *context )
{
	if ( expression->result.marked ) {
		output( Error, "'?' is not instantiable" );
		return 0;
	}

	ExpressionSub *sub = expression->sub;
	if ( expression->result.as_sub || !sub[ 3 ].result.any ) {
		output( Error, "instance designation is not allowed in instantiation mode" );
		return 0;
	}

	if ( sub[ 0 ].result.any || sub[ 1 ].result.any || sub[ 2 ].result.any ) {
		output( Error, "'.' is not instantiable" );
		return 0;
	}

	for ( int i=0; i<4; i++ ) {
		if ( sub[ i ].result.active || sub[ i ].result.inactive || sub[ i ].result.not ) {
			output( Error, "expression flags are not allowed in instantiation mode" );
			return 0;
		}
	}

	for ( int i=0; i<3; i++ ) {
		if (( sub[ i ].e != NULL ) && !sub[ i ].result.any ) {
			if ( !instantiable( sub[ i ].e, context ) )
				return 0;
		}
		else switch ( sub[ i ].result.identifier.type ) {
		case VariableIdentifier:
			; char *identifier = sub[ i ].result.identifier.value;
			if ( identifier == NULL )
				break;
			registryEntry *entry;
			if ( sub[ i ].result.lookup ) {
				listItem *stack = context->control.stack;
				context->control.stack = stack->next;
				entry = lookupVariable( context, identifier );
				context->control.stack = stack;
			} else {
				entry = lookupVariable( context, identifier );
			}
			if ( entry == NULL ) {
				output( Error, "variable '%%%s' not found", identifier );
				return 0;
			}
			VariableVA *variable = (VariableVA *) entry->value;
			switch ( variable->type ) {
			case EntityVariable:
			case LiteralVariable:
				break;
			case NarrativeVariable:
				output( Error, "'%%%s' is a narrative variable - "
					"not allowed in expressions", identifier );
				return 0;
			case ExpressionVariable:
				if ( !instantiable( ((listItem * ) variable->data.value )->ptr, context ) )
					return 0;
				break;
			}
			break;
		default:
			break;
		}
	}
	return 1;
}

/*---------------------------------------------------------------------------
	test_as_sub
---------------------------------------------------------------------------*/
int
test_as_sub( void *eparam, int read_mode, int as_sub )
{
	if ( read_mode ) {
#if 0	// DO_LATER
		ExpressionSub *e = eparam;
		if ( e->super == NULL ) return 0;
		Expression *sup = ((ExpressionSub *) e->super )->e;
		if ( as_sub & 7 )
		{
			if ((( as_sub & 1 ) && ( &sup->sub[ 0 ] == e )) ||
			    (( as_sub & 2 ) && ( &sup->sub[ 1 ] == e )) ||
			    (( as_sub & 4 ) && ( &sup->sub[ 2 ] == e )))
			{
				return 1;
			}
			return 0;
		}
		if ( as_sub &  112 )
		{
			if ((( as_sub & 16 ) && ( &sup->sub[ 0 ] == e )) ||
			    (( as_sub & 32 ) && ( &sup->sub[ 1 ] == e )) ||
			    (( as_sub & 64 ) && ( &sup->sub[ 2 ] == e )))
			{
				return 0;
			}
		}
#endif	// DO_LATER
		return 1;
	} else {
		Entity *e = eparam;
		if ( as_sub & 7 )
		{
			if ((( as_sub & 1 ) && ( e->as_sub[ 0 ] != NULL )) ||
			    (( as_sub & 2 ) && ( e->as_sub[ 1 ] != NULL )) ||
			    (( as_sub & 4 ) && ( e->as_sub[ 2 ] != NULL )))
			{
				return 1;
			}
			return 0;
		}
		if ( as_sub &  112 )
		{
			if ((( as_sub & 16 ) && ( e->as_sub[ 0 ] != NULL )) ||
			    (( as_sub & 32 ) && ( e->as_sub[ 1 ] != NULL )) ||
			    (( as_sub & 64 ) && ( e->as_sub[ 2 ] != NULL )))
			{
				return 0;
			}
		}
		return 1;
	}
}

/*---------------------------------------------------------------------------
	invert_results
---------------------------------------------------------------------------*/
void
invert_results( ExpressionSub *sub, int as_sub, listItem *results )
{
	listItem *source = sub->result.list;
	listItem *dest = NULL;
	int active = sub->result.active;
	int inactive = sub->result.inactive;
	if ( CN.context->expression.mode == ReadMode )
	{
		for ( listItem *i = results; i!=NULL; i=i->next )
		{
			ExpressionSub *e = i->ptr;
			if ( !test_as_sub( e, 1, as_sub ) )
				continue;
			if (( active && !e->result.active ) ||
			    ( inactive && e->result.active ))
				continue;
			if ( lookupItem( source, e ) == NULL )
				addItem( &dest, e );
		}
	}
	else
	{
		listItem *all = ( results == NULL ) ? CN.DB : results;
		for ( listItem *i = all; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;

			if ( !test_as_sub( e, 0, as_sub ) )
				continue;
			if (( active && !cn_is_active(e) ) ||
			    ( inactive && cn_is_active(e) ) )
				continue;
			if ( lookupItem( source, e ) == NULL )
				addItem( &dest, e );
		}
	}
	freeListItem( &sub->result.list );
	sub->result.list = dest;
}

/*---------------------------------------------------------------------------
	take_all
---------------------------------------------------------------------------*/
listItem *
take_all( Expression *expression, int as_sub, listItem *results )
{
	listItem *dest = NULL;
	int active[ 4 ];
	int inactive[ 4 ];
	ExpressionSub *sub = expression->sub;
	int count =
		( expression->result.mark == 1 ) ? 0 :
		( expression->result.mark == 2 ) ? 1 :
		( expression->result.mark == 4 ) ? 2 :
		3;
	int check_instance = !sub[ 1 ].result.none;
	if ( check_instance ) {
		for ( int i=0; i<4; i++ ) {
			active[ i ] = sub[ i ].result.active;
			inactive[ i ] = sub[ i ].result.inactive;
		}
	} else {
		active[ 3 ] = sub[ 0 ].result.active || sub[ 3 ].result.active;
		inactive[ 3 ] = sub[ 0 ].result.inactive || sub[ 3 ].result.inactive;
	}

#ifdef DEBUG
	output( Debug, "take_all: init done, as_sub=%d, count=%d, check_instance=%d", as_sub, count, check_instance );
#endif

	if ( CN.context->expression.mode == ReadMode )
	{
		for ( listItem *i = results; i!=NULL; i=i->next )
		{
			ExpressionSub *s = i->ptr;
			if ( !test_as_sub( s, 1, as_sub ) )
				continue;
			if (( active[ 3 ] && !s->result.active ) ||
			    ( inactive[ 3 ] && !s->result.inactive ))
				continue;
			if (( s->e == NULL ) && (( count < 3 ) ||
			    ( inactive[ 0 ] || inactive[ 1 ] || inactive[ 2 ] )))
				continue;
			if (  check_instance &&
			    (( s->e == NULL ) ||
			     ( active[ 0 ] && !s->e->sub[ 0 ].result.active ) ||
			     ( active[ 1 ] && !s->e->sub[ 1 ].result.active ) ||
			     ( active[ 2 ] && !s->e->sub[ 2 ].result.active ) ||
			     ( inactive[ 0 ] && !s->e->sub[ 0 ].result.inactive ) ||
			     ( inactive[ 1 ] && !s->e->sub[ 1 ].result.inactive ) ||
			     ( inactive[ 2 ] && !s->e->sub[ 2 ].result.inactive )) )
				continue;
			if ( count == 3 ) {
				addItem( &dest, s );
			} else {
				addIfNotThere( &dest, &s->e->sub[ count ] );
			}
		}
	}
	else
	{
		listItem *all = ( results == NULL ) ? CN.DB : results;
		for ( listItem *i = all; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			if ( !test_as_sub( e, 0, as_sub ) )
				continue;
			if (( active[ 3 ] && !cn_is_active( e )) ||
			    ( inactive[ 3 ] && cn_is_active( e )))
				continue;
			if (( count < 3 ) && ( e->sub[ count ] == NULL ))
				continue;
			if (  check_instance &&
			    (( e->sub[ 1 ] == NULL ) ||
			     ( active[ 0 ] && !cn_is_active( e->sub[ 0 ] )) ||
			     ( active[ 1 ] && !cn_is_active( e->sub[ 1 ] )) ||
			     ( active[ 2 ] && !cn_is_active( e->sub[ 2 ] )) ||
			     ( inactive[ 0 ] && cn_is_active( e->sub[ 0 ] )) ||
			     ( inactive[ 1 ] && cn_is_active( e->sub[ 1 ] )) ||
			     ( inactive[ 2 ] && cn_is_active( e->sub[ 2 ] ))) )
				continue;
			if ( count == 3 ) {
				addItem( &dest, e );
			} else {
				addIfNotThere( &dest, e->sub[ count ] );
			}
		}
	}
	return dest;
}

/*---------------------------------------------------------------------------
	take_sup
---------------------------------------------------------------------------*/
void
take_sup( Expression *expression )
{
	int as_sup = expression->result.as_sup;
	if ( !as_sup ) return;

	listItem *super = NULL;

	if ( CN.context->expression.mode == ReadMode )
	{
		for ( listItem *i = expression->result.list; i!=NULL; i=i->next )
		{
			ExpressionSub *s = i->ptr;
			for ( int j=0; j<3; j++ )
			{
				if ( !( as_sup & ( 1<< j ) ) )
					continue;

				addIfNotThere( &super, s->super );
			}
		}
	} else {
		for ( listItem *i = expression->result.list; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			for ( int j=0; j<3; j++ )
			{
				if ( !( as_sup & ( 1<< j ) ) )
					continue;

				for ( listItem *k = e->as_sub[ j ]; k!=NULL; k=k->next ) {
					addIfNotThere( &super, (Entity *) k->ptr );
				}
			}
		}
	}
	freeListItem( &expression->result.list );
	expression->result.list = super;
}

/*---------------------------------------------------------------------------
	take_sub_results
---------------------------------------------------------------------------*/
void
take_sub_results( ExpressionSub *sub, int as_sub )
{
	listItem *dest = NULL, *last_i = NULL, *next_i;
	int active = sub->result.active;
	int inactive = sub->result.inactive;
	listItem **list = &sub->result.list;
	if ( CN.context->expression.mode == ReadMode )
	{
		for ( listItem *i = *list; i!=NULL; i=next_i )
		{
			ExpressionSub *e = i->ptr;
			next_i = i->next;
			if (( !test_as_sub( e, 1, as_sub ) ) ||
			    ( active && !e->result.active ) ||
			    ( inactive && e->result.active ))
			{
				clipListItem( list, i, last_i, next_i );
			}
			else last_i = i;
		}
	} else {
		for ( listItem *i = *list; i!=NULL; i=next_i )
		{
			Entity *e = i->ptr;
			next_i = i->next;
			if (( !test_as_sub( e, 0, as_sub ) ) ||
			    ( active && !cn_is_active( e ) ) ||
			    ( inactive && cn_is_active( e ) ) )
			{
				clipListItem( list, i, last_i, next_i );
			}
			else last_i = i;
		}
	}
}

/*---------------------------------------------------------------------------
	extract_sub_results
---------------------------------------------------------------------------*/
void
extract_sub_results( ExpressionSub *sub0, ExpressionSub *sub3, int as_sub, listItem *results )
{
	listItem **list = &sub0->result.list;
	if (( sub0->result.active && sub3->result.inactive ) ||
	    ( sub3->result.active && sub0->result.inactive ))
	{
		freeListItem( list );
		return;
	}

	// first we reduce both lists wrt their corresponding flags
	take_sub_results( sub0, 8 );
	take_sub_results( sub3, 8 );

	// then we process the results
	listItem *last_i = NULL, *next_i;
	int read_mode = ( CN.context->expression.mode == ReadMode );
	if ( sub0->result.not && sub3->result.not )
	{
		listItem *all = ( CN.context->expression.mode == ReadMode ) ?
			results : ( results == NULL ) ? CN.DB : results;
		for ( listItem *i = all; i!=NULL; i=i->next )
		{
			if ( !( as_sub & 8 ) && !test_as_sub( i->ptr, read_mode, as_sub ) )
				continue;

			if (( lookupItem( *list, i->ptr ) == NULL ) &&
			    ( lookupItem( sub3->result.list, i->ptr ) == NULL ) )
			{
				addItem( &last_i, i->ptr );
			}
		}
		freeListItem( list );
		sub0->result.list = last_i;
	}
	else if ( sub3->result.not || sub0->result.not )
	{
		ExpressionSub *sub_swap = NULL;
		if ( sub0->result.not ) {
			sub_swap = sub0; sub0 = sub3; sub3 = sub_swap;
		}
		for ( listItem *i = *list; i!=NULL; i=next_i )
		{
			next_i = i->next;
			if ( !( as_sub & 8 ) )
			{
				if ( !test_as_sub( i->ptr, read_mode, as_sub ) )
				{
					clipListItem( list, i, last_i, next_i );
				}
				else last_i = i;
			}
			else if ( lookupItem( sub3->result.list, i->ptr ) != NULL )
			{
				clipListItem( list, i, last_i, next_i );
			}
			else last_i = i;
		}
		if ( sub_swap != NULL ) {
			sub_swap = sub0; sub0 = sub3; sub3 = sub_swap;
		}
	}
	else
	{
		for ( listItem *i = *list; i!=NULL; i=next_i )
		{
			next_i = i->next;
			if ( !( as_sub & 8 ) )
			{
				if ( !test_as_sub( i->ptr, read_mode, as_sub ) )
				{
					clipListItem( list, i, last_i, next_i );
				}
				else last_i = i;
			}
			else if ( lookupItem( sub3->result.list, i->ptr ) == NULL )
			{
				clipListItem( list, i, last_i, next_i );
			}
			else last_i = i;
		}
	}
}

