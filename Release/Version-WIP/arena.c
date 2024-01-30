#include <stdio.h>
#include <stdlib.h>

#include "arena.h"

//===========================================================================
//	bm_arena_register
//===========================================================================
CNInstance *
bm_arena_register( Registry *arena, char *s, CNDB *db )
/*
   Note:
	in the future we might want to create and use a compressed version
	of the "_" sub-string passed by bm_instantiate() on either
		do "_"
		do :_:"_"
	to index our string arena - we might also inform it directly during
	story / narrative creation
*/ {
	CNInstance *e;
	if (( s )) {
		// String arena is a registry indexed by string of ref registries
		Registry *string_arena;
		Pair *entry = registryLookup( arena, "$" );
		if (( entry )) string_arena = entry->value;
		else {
			string_arena = newRegistry( IndexedByName );
			registryRegister( arena, "$", string_arena ); }

		if (( entry = registryLookup( string_arena, s ) )) {
			Registry *ref = entry->value;
			Pair *r = registryLookup( ref, db );
			if (( r )) e = r->value;
			else {
				e = cn_new( NULL, NULL );
				e->sub[ 1 ] = (CNInstance *) entry;
				registryRegister( ref, db, e ); } }
		else {
			entry = registryRegister( string_arena, s, NULL );
			Registry *ref = newRegistry( IndexedByAddress );
			entry->value = ref;
			e = cn_new( NULL, NULL );
			e->sub[ 1 ] = (CNInstance *) entry;
			registryRegister( ref, db, e ); } }
	else {
		// UBE arena is simply a list of ref registries
		Pair *entry = registryLookup( arena, "" );
		if ( !entry ) entry = registryRegister( arena, "", NULL );

		Registry *ref = newRegistry( IndexedByAddress );
		addIfNotThere((listItem **) &entry->value, ref );
		e = cn_new( NULL, NULL );
		e->sub[ 1 ] = (CNInstance *) newPair( NULL, ref );
		registryRegister( ref, db, e ); }

	db_op( DB_MANIFEST_OP, e, db );
	return e; }

//===========================================================================
//	bm_arena_deregister
//===========================================================================
void
bm_arena_deregister( Registry *arena, CNInstance *e, CNDB *db )
/*
	Assumption: we have e:( NULL, [ ., ref:{[db,e]} ] )
					^---- NULL if UBE
*/ {
	// remove [db,e] from ref registry
	Pair *entry = (Pair *) e->sub[ 1 ];
	Registry *ref = entry->value;
	registryDeregister( ref, db );
	if (( ref->entries ))
		goto RELEASE;

	// remove dangling entry from appropriate arena
	freeRegistry( ref, NULL );
	if ( !entry->name ) {
		// UBE arena is a list of ref registries
		freePair( entry );
		entry = registryLookup( arena, "" );
		removeIfThere((listItem **) &entry->value, ref );
		goto RELEASE; }

	arena = registryLookup( arena, "$" )->value;
	listItem *next_i, *last_i=NULL;
	for ( listItem *i=arena->entries; i!=NULL; i=next_i ) {
		Pair *r = i->ptr;
		next_i = i->next;
		if ( r!=entry ) last_i = i;
		else {
			freeItem( i );
			freePair( r );
			if (( last_i )) last_i->next = next_i;
			else arena->entries = next_i;
			break; } }
RELEASE:
	e->sub[1] = NULL;
	cn_release( e ); }

//===========================================================================
//	freeArena
//===========================================================================
static freeRegistryCB free_arena_CB, free_string_CB;

void
freeArena( Registry *arena ) {
	freeRegistry( arena, free_arena_CB ); }

static void
free_arena_CB( Registry *arena, Pair *entry ) {
	switch ( *(char *)entry->name ) {
	case '\0': ;
		Registry *ref;
		listItem **ube = (listItem **) &entry->value;
		while (( ref=popListItem( ube ) ))
			freeRegistry( ref, NULL );
		break;
	case '$':
		arena = entry->value;
		freeRegistry( arena, free_string_CB );
		break; } }

static void
free_string_CB( Registry *arena, Pair *entry ) {
	Registry *ref = entry->value;
	freeRegistry( ref, NULL ); }


