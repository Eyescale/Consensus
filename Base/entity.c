#include <stdio.h>
#include <stdlib.h>

#include "entity.h"
#include "pair.h"

// #define DEBUG

//===========================================================================
//	cn_new
//===========================================================================
static void add_as_sub( CNEntity *, listItem **, const int pivot );

CNEntity *
cn_new( CNEntity *source, CNEntity *target )
{
	Pair *sub = newPair( source, target );
	Pair *as_sub = newPair( NULL, NULL );
	CNEntity *e = (CNEntity *) newPair( sub, as_sub );
#ifndef OPT
	if (( source )) addItem( &source->as_sub[0], e );
	if (( target ))	addItem( &target->as_sub[1], e );
#else
	if (( source )) add_as_sub( e, &source->as_sub[0], 0 );
	if (( target ))	add_as_sub( e, &target->as_sub[1], 1 );
#endif
	return e;
}

static void
add_as_sub( CNEntity *e, listItem **as_sub, const int p )
{
	listItem *i, *last_i=NULL, *j=newItem( e );
	switch ( p ) {
	case 0:
		e = e->sub[ 1 ];
		for ( i=*as_sub; i!=NULL; i=i->next ) {
			CNEntity *g = i->ptr;
			intptr_t compare = (uintptr_t) g->sub[ 1 ] - (uintptr_t) e;
			if ( compare < 0 ) { last_i=i; continue; }
#ifdef DEBUG
			else if ( !compare ) goto ERR;
#endif
			break;
		}
		break;
	default:
		e = e->sub[ 0 ];
		for ( i=*as_sub; i!=NULL; i=i->next ) {
			CNEntity *g = i->ptr;
			intptr_t compare = (uintptr_t) g->sub[ 0 ] - (uintptr_t) e;
			if ( compare < 0 ) { last_i=i; continue; }
#ifdef DEBUG
			else if ( !compare ) goto ERR;
#endif
			break;
		}
	}
	if (( last_i )) { last_i->next = j; j->next = i; }
	else { j->next = *as_sub; *as_sub = j; }
	return;
ERR:
	fprintf( stderr, ">>>>> B%%: Error: add_as_sub(): already there\n" );
	freeItem( j );
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
#ifndef OPT
			if ( instance->sub[ 0 ] == e )
#else
			intptr_t comparison = (uintptr_t) instance->sub[ 0 ] - (uintptr_t) e;
			if ( comparison < 0 ) continue;
			else if ( comparison ) break;
#endif
				return instance;
		}
		break;
	default:
		for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
			CNEntity *instance = i->ptr;
#ifndef OPT
			if ( instance->sub[ 1 ] == f )
#else
			intptr_t comparison = (uintptr_t) instance->sub[ 1 ] - (uintptr_t) f;
			if ( comparison < 0 ) continue;
			else if ( comparison ) break;
#endif
				return instance;
		}
	}
	return NULL;
}

//===========================================================================
//	cn_rewire
//===========================================================================
static void remove_as_sub( CNEntity *, listItem **, const int position );

void
cn_rewire( CNEntity *e, int ndx, CNEntity *sub )
{
	CNEntity *previous = e->sub[ ndx ];
	remove_as_sub( e, &previous->as_sub[ ndx ], ndx );
	e->sub[ ndx ] = sub;
	add_as_sub( e, &sub->as_sub[ ndx ], ndx );
}

static void
remove_as_sub( CNEntity *e, listItem **as_sub, const int p )
{
	CNEntity *h;
	listItem *i, *last_i;
	switch ( p ) {
	case 0:
#ifdef UNIFIED
		if ( !(h=e->sub[ 1 ]) || !e->sub[0] ) {
#else
		if ( !(h=e->sub[ 1 ]) ) {
#endif
			removeItem( as_sub, e );
			return;
		}
		for ( i=*as_sub, last_i=NULL; i!=NULL; i=i->next ) {
			CNEntity *g = i->ptr;
#ifdef UNIFIED
			if ( !g->sub[0] ) { last_i=i; continue; }
#endif
			intptr_t compare = (uintptr_t) g->sub[ 1 ] - (uintptr_t) h;
			if ( compare < 0 ) { last_i=i; continue; }
			else if ( !compare ) {
				if (( last_i )) last_i->next = i->next;
				else *as_sub = i->next;
				freeItem( i );
			}
			return;
		}
		break;
	default:
		if ( !(h=e->sub[ 0 ]) ) {
			removeItem( as_sub, e );
			return;
		}
		for ( i=*as_sub, last_i=NULL; i!=NULL; i=i->next ) {
			CNEntity *g=i->ptr, *k=g->sub[ 0 ];
#ifdef UNIFIED
			if ( !k ) { last_i=i; continue; }
#endif
			intptr_t compare = (uintptr_t) k - (uintptr_t) h;
			if ( compare < 0 ) { last_i=i; continue; }
			else if ( !compare ) {
				if (( last_i )) last_i->next = i->next;
				else *as_sub = i->next;
				freeItem( i );
			}
			return;
		}
	}
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
#ifndef UNIFIED
	/* 1. remove e from the as_sub lists of its subs
	*/
	CNEntity *sub;
#ifndef OPT
	if (( sub=e->sub[0] )) removeItem( &sub->as_sub[0], e );
	if (( sub=e->sub[1] )) removeItem( &sub->as_sub[1], e );
#else // OPT
	if (( sub=e->sub[0] )) remove_as_sub( e, &sub->as_sub[0], 0 );
	if (( sub=e->sub[1] )) remove_as_sub( e, &sub->as_sub[1], 1 );
#endif
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

#else // UNIFIED

	/* 1. remove e from the as_sub lists of its subs
	*/
	CNEntity *sub;
#ifndef OPT
	if (( sub=e->sub[0] )) {
		removeItem( &sub->as_sub[0], e );
		if (( sub=e->sub[1] )) removeItem( &sub->as_sub[1], e );
	}
	else if (( sub = e->sub[1] )) {
		Pair *pair = (Pair *) sub;
		if (( pair->name )) removeItem( (listItem **) pair->name, e );
		if (( pair->value )) removeItem( (listItem **) pair->value, e );
		freePair( pair );
	}
#else // OPT
	if (( sub=e->sub[0] )) {
		remove_as_sub( e, &sub->as_sub[0], 0 );
		if (( sub=e->sub[1] )) remove_as_sub( e, &sub->as_sub[1], 1 );
	}
	else if (( sub = e->sub[1] )) {
		Pair *pair = (Pair *) sub;
		if (( pair->name )) remove_as_sub( e, (listItem **) pair->name, 0 );
		if (( pair->value )) remove_as_sub( e, (listItem **) pair->value, 1 );
		freePair( pair );
	}
#endif
	/* 2. nullify e as sub of its as_sub's
	*/
	for ( listItem *j=e->as_sub[ 0 ]; j!=NULL; j=j->next ) {
		CNEntity *instance = j->ptr;
		Pair *pair;
		if (( instance->sub[0] )) {
			sub = instance->sub[ 1 ];
			pair = newPair( NULL, &sub->as_sub[1] );
			instance->sub[0] = NULL;
			instance->sub[1] = (CNEntity *) pair;
		}
		else if (( instance->sub[1] )) {
			pair = (Pair *) instance->sub[ 1 ];
			if ( pair->value == NULL ) {
				freePair( pair );
				instance->sub[1] = NULL;
			}
		}
	}
	for ( listItem *j=e->as_sub[ 1 ]; j!=NULL; j=j->next ) {
		CNEntity *instance = j->ptr;
		Pair *pair;
		if (( instance->sub[0] )) {
			sub = instance->sub[ 0 ];
			pair = newPair( &sub->as_sub[0], NULL );
			instance->sub[0] = NULL;
			instance->sub[1] = (CNEntity *) pair;
		}
		else if (( instance->sub[1] )) {
			pair = (Pair *) instance->sub[ 1 ];
			if ( pair->name == NULL ) {
				freePair( pair );
				instance->sub[1] = NULL;
			}
		}
	}
	/* 3. free e
	*/
	cn_free( e );
#endif
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

