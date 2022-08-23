#include <stdio.h>
#include <stdlib.h>

#include "entity.h"
#include "pair.h"

// #define DEBUG

//===========================================================================
//	cn_new
//===========================================================================
CNEntity *
cn_new( CNEntity *source, CNEntity *target )
{
	Pair *sub = newPair( source, target );
	Pair *as_sub = newPair( NULL, NULL );
	CNEntity *e = (CNEntity *) newPair( sub, as_sub );
	if (( source )) addItem( &source->as_sub[0], e );
	if (( target ))	addItem( &target->as_sub[1], e );
	return e;
}

//===========================================================================
//	cn_instance
//===========================================================================
CNEntity *
cn_instance( CNEntity *e, CNEntity *f, const int pivot )
{
	switch ( pivot ) {
	case 0:
		for ( listItem *i=f->as_sub[ 1 ]; i!=NULL; i=i->next ) {
			CNEntity *instance = i->ptr;
			if ( instance->sub[ 0 ] == e )
				return instance;
		}
		break;
	default:
		for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
			CNEntity *instance = i->ptr;
			if ( instance->sub[ 1 ] == f )
				return instance;
		}
	}
	return NULL;
}

//===========================================================================
//	cn_rewire
//===========================================================================
void
cn_rewire( CNEntity *e, int ndx, CNEntity *f )
/*
	Assumption: e->sub[ndx] is proper CNEntity *
*/
{
	CNEntity *previous = e->sub[ndx];
	removeItem( &previous->as_sub[ndx], e );
	addItem( &f->as_sub[ ndx ], e );
	e->sub[ ndx ] = f;
}

//===========================================================================
//	cn_prune
//===========================================================================
void
cn_prune( CNEntity *e )
/*
	frees e and all %(( e, . ), ... ) relationship instances,
	proceeding top-down
*/
{
	listItem *stack = NULL;
	listItem *i = newItem( e );
	for ( ; ; ) {
		listItem *j = e->as_sub[ 0 ];
		if (( j )) {
			addItem( &stack, i );
			i = j; e = i->ptr;
			continue;
		}
		for ( ; ; ) {
			cn_free( e );
			if (( i->next )) {
				i = i->next;
				e = i->ptr;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				e = i->ptr;
			}
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
}

//===========================================================================
//	cn_release
//===========================================================================
void
cn_release( CNEntity *e )
/*
	1. removes e from from the as_sub lists of its subs
	2. nullifies e as sub of its as_sub's
	3. frees e
*/
{
	/* 1. remove e from the as_sub lists of its subs
	*/
	CNEntity *sub;
	if (( sub=e->sub[0] )) removeItem( &sub->as_sub[0], e );
	if (( sub=e->sub[1] )) removeItem( &sub->as_sub[1], e );

	/* 2. nullify e as sub of its as_sub's
	*/
	for ( listItem *j=e->as_sub[ 0 ]; j!=NULL; j=j->next ) {
		CNEntity *instance = j->ptr;
		instance->sub[ 0 ] = NULL;
	}
	for ( listItem *j=e->as_sub[ 1 ]; j!=NULL; j=j->next ) {
		CNEntity *instance = j->ptr;
		instance->sub[ 1 ] = NULL;
	}
	/* 3. free e
	*/
	cn_free( e );

}

//===========================================================================
//	cn_free
//===========================================================================
void
cn_free( CNEntity *e )
{
	if ( e == NULL ) return;
	freePair((Pair *) e->sub );
	freeListItem( &e->as_sub[ 0 ] );
	freeListItem( &e->as_sub[ 1 ] );
	freePair((Pair *) e->as_sub );
	freePair((Pair *) e );
}

