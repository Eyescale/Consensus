#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "expression.h"
#include "expression_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	freeExpression
---------------------------------------------------------------------------*/
static void
nullify( Expression *expression, listItem **residue, listItem **list )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;

	// residues and lists are earlier versions of an expression which has superceded
	// itself, possibly more than once. Each must be freed only once.
	for ( int i=0; i<4; i++ )
	{
		if ( sub[ i ].result.identifier.type == ListIdentifier ) {
			for ( listItem *j = (listItem *) sub[ i ].result.identifier.value; j!=NULL; j=j->next )
			{
				nullify( (Expression *) j->ptr, residue, list );
			}
			addIfNotThere( list, sub[ i ].result.identifier.value );
			sub[ i ].result.identifier.value = NULL;
		}
		if ( sub[ i ].e == NULL )
			continue;
		nullify( sub[ i ].e, residue, list );
		if ( sub[ i ].result.identifier.value != NULL )
			continue;
		addIfNotThere( residue, sub[ i ].e );
		sub[ i ].e = NULL;
	}
}

static void
erase( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		if (( sub[ i ].result.identifier.type != VariableIdentifier ) ||
		    (( sub[ i ].result.identifier.value != variator_symbol ) && ( sub[ i ].result.identifier.value != this_symbol )))
		{
			free( sub[ i ].result.identifier.value );
		}
		erase( sub[ i ].e );
		free( sub[ i ].e );
	}
	freeListItem( &expression->result.list );
}

void
freeExpression( Expression *expression )
{
	listItem *residue = NULL, *list = NULL;
	nullify( expression, &residue, &list );
	for ( listItem *i = residue; i!=NULL; i=i->next ) {
		erase( (Expression *) i->ptr );
	}
	for ( listItem *i = list; i!=NULL; i=i->next ) {
		for ( listItem *j = (listItem *) i->ptr; j!=NULL; j=j->next ) {
			erase( (Expression *) j->ptr );
		}
		freeListItem( (listItem **) &i->ptr );
	}
	erase( expression );
	freeListItem( &residue );
	freeListItem( &list );
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
		if ( !sub[ i ].result.resolve ) continue;
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
		fprintf( stderr, "debug> collapse: collapsing %d\n", count );
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
		fprintf( stderr, "debug> collapse: collapsing %d\n", count );
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
	ExpressionSub *sub = expression->sub;
	int flags[ 3 ];

#ifdef DEBUG
	fprintf( stderr, " debug> expression_collapse: entering %0x\n", (int) expression );
#endif
	if (( sub[ 3 ].e != NULL ) &&
	     !sub[ 3 ].result.active && !sub[ 3 ].result.inactive && !sub[ 3 ].result.not &&
	    ( sub[ 0 ].result.any && !sub[ 0 ].result.active && !sub[ 0 ].result.inactive && !sub[ 0 ].result.not ) &&
	    ( sub[ 1 ].result.any && !sub[ 1 ].result.active && !sub[ 1 ].result.inactive && !sub[ 1 ].result.not ) &&
	    ( sub[ 2 ].result.any && !sub[ 2 ].result.active && !sub[ 2 ].result.inactive && !sub[ 2 ].result.not ))
	{
		flags[ 0 ] = 0;
		flags[ 1 ] = 0;
#ifdef DEBUG
		fprintf( stderr, "debug> expression_collapse: collapsing whole...\n" );
#endif
		// in this case we replace the whole current expression with its sub[ 3 ]
		collapse( expression, flags, 3, 4, -1 );
	}

	// collapse unnecessary levels
	for ( int i=0; i<4; i++ )	// look at each sub and see if it can be collapsed
	{
		// we want to replace sub[ i ] with its own sub-expression
		if ( sub[ i ].e == NULL )
			continue;

		Expression *e = sub[ i ].e;
		ExpressionSub *e_sub = e->sub;
		if ( e_sub[ 1 ].result.none && e_sub[ 3 ].result.any && !e->result.as_sub && !e->result.mark )
		{
#ifdef DEBUG
			fprintf( stderr, "debug> expression_collapse: collapsing sub[ %d ]\n", i );
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
			fprintf( stderr, "debug> expression_collapse: completing according to mark\n" );
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
				fprintf( stderr, "debug> expression_collapse: collapsing whole...\n" );
#endif
				// in this case we replace the whole current expression with its sub[ 0 ]
				collapse( expression, flags, 0, 4, -1 );
				expression->result.as_sub = as_sub;
			}
			else if ( sub[ 0 ].e->sub[ 3 ].result.any )
			{
#ifdef DEBUG
				fprintf( stderr, "debug> expression_collapse: collapsing body...\n" );
#endif
				// in this case we replace only the body of the current expression with sub[ 0 ]
				collapse( expression, flags, 0, 3, -1 );
				expression->result.as_sub = as_sub;
			}
		}
	}
	// we want to replace the current expression with its sub[ 3 ]
	else if ( sub[ 0 ].result.any && ( sub[ 3 ].e != NULL ) )
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
			fprintf( stderr, "debug> collapse: swapping sub[ 0 ] and sub[ 3 ]\n", count );
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
			fprintf( stderr, "debug> expression_collapse: collapsing whole...\n" );
#endif
			// in this case we replace the whole current expression with its sub[ 3 ]
			collapse( expression, flags, 3, 4, -1 );
		}
	}
}

/*---------------------------------------------------------------------------
	test_as_sub
---------------------------------------------------------------------------*/
int
test_as_sub( Entity *e, int as_sub )
{
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
	if ( CN.context->expression.mode == ExpandedMode )
	{
		for ( listItem *i = results; i!=NULL; i=i->next )
		{
			ExpressionSub *e = i->ptr;
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

			if ( !test_as_sub( e, as_sub ) )
				continue;
			if (( active && !cn_is_active(e) ) ||
			    ( inactive && cn_is_active(e) ) )
				continue;
			if ( lookupItem( source, e ) == NULL )
			{
				addItem( &dest, e );
			}
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
	fprintf( stderr, "debug> take_all: init done, as_sub=%d, count=%d, check_instance=%d\n", as_sub, count, check_instance );
#endif

	if ( CN.context->expression.mode == ExpandedMode )
	{
		for ( listItem *i = results; i!=NULL; i=i->next )
		{
			ExpressionSub *s = i->ptr;
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
			if ( !test_as_sub( e, as_sub ) )
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
	take_sub_results
---------------------------------------------------------------------------*/
void
take_sub_results( ExpressionSub *sub, int as_sub )
{
	listItem *dest = NULL, *last_i = NULL, *next_i;
	int active = sub->result.active;
	int inactive = sub->result.inactive;
	listItem **list = &sub->result.list;
	if ( CN.context->expression.mode == ExpandedMode )
	{
		for ( listItem *i = *list; i!=NULL; i=next_i )
		{
			ExpressionSub *e = i->ptr;
			next_i = i->next;
			if (( active && !e->result.active ) ||
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
			if ( ( !test_as_sub( e, as_sub ) ) ||
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
	if ( sub0->result.not && sub3->result.not )
	{
		listItem *all = ( CN.context->expression.mode == ExpandedMode ) ?
			results : ( results == NULL ) ? CN.DB : results;
		for ( listItem *i = all; i!=NULL; i=i->next )
		{
			if (( CN.context->expression.mode != ExpandedMode ) && !( as_sub & 8 ))
			{
				Entity *e = (Entity *) i->ptr;
				if ( !test_as_sub( e, as_sub ) )
					continue;
			}

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
			if (( CN.context->expression.mode != ExpandedMode ) && !( as_sub & 8 ))
			{
				Entity *e = (Entity *) i->ptr;
				if ( !test_as_sub( e, as_sub ) )
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
			if (( CN.context->expression.mode != ExpandedMode ) && !( as_sub & 8 ))
			{
				Entity *e = (Entity *) i->ptr;
				if ( !test_as_sub( e, as_sub ) )
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

