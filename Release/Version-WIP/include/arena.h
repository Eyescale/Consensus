#ifndef ARENA_H
#define ARENA_H

#include "database.h"
#include "registry.h"

void		freeArena( Registry * );
CNInstance *	bm_arena_register( Registry *, char *, CNDB * );
void		bm_arena_deregister( Registry *, CNInstance *, CNDB * );

static inline Registry * newArena( void ) {
	return newRegistry(IndexedByCharacter); }

static inline CNInstance * bm_arena_translate( CNInstance *x, CNDB *db ) {
	Registry *ref = ((Pair *) x->sub[ 1 ])->value; 
	Pair *entry = registryLookup( ref, db );
	return (( entry )? entry->value : NULL ); }

static inline CNInstance * bm_arena_transpose( CNInstance *x, CNDB *db ) {
	Registry *ref = ((Pair *) x->sub[ 1 ])->value;
	Pair *entry = registryLookup( ref, db );
	if (( entry )) return entry->value;
	else {
		CNInstance *instance = cn_new( NULL, NULL );
		if (( DBIdentifier(x) ))
			instance->sub[ 1 ] = x->sub[ 1 ];
		else	instance->sub[ 1 ] = (CNInstance *) newPair( NULL, ref );
		registryRegister( ref, db, instance );
		return instance; } }

static inline int bm_arena_compare( CNInstance *x, CNInstance *y ) {
	void *x_ref = ((Pair *) x->sub[ 1 ])->value;
	void *y_ref = ((Pair *) y->sub[ 1 ])->value;
	return ( x_ref!=y_ref ); }

static CNInstance * bm_arena_lookup( Registry *arena, char *p, CNDB *db ) {
	if (( p )) {
		Pair *entry = registryLookup( arena, "$" );
		if (( entry )) {
			entry = registryLookup((Registry *) entry->value, p );
			if (( entry )) {
				entry = registryLookup((Registry *) entry->value, db );
				if (( entry )) return entry->value; } } }
	return NULL; }


#endif // ARENA_H
