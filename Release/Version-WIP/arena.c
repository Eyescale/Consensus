#include <stdio.h>
#include <stdlib.h>

#include "arena.h"

//===========================================================================
//	bm_arena_register
//===========================================================================
CNInstance *
bm_arena_register( CNArena *arena, char *s, CNDB *db )
/*
   returns
		e:( NULL, [ s, ref:{[db,e]} ] )
   Note:
	in the future we might want to create and use a compressed version
	of the "_" string passed by bm_instantiate() on either
		do "_"
		do :_:"_"
	to index string arena - we might also inform it directly during
	story / narrative creation
*/ {
	CNInstance *e;
	Pair *entry, *r;
	Registry *ref;
	if ( !s ) {
		listItem **ube_arena = (listItem **) &arena->value;
		ref = newRegistry( IndexedByAddress );
		entry = newPair( NULL, ref );
		addIfNotThere( ube_arena, entry ); }
	else {
		Registry *string_arena = arena->name;
		if (( entry=registryLookup( string_arena, s ) )) {
			ref = entry->value;
			if (( r=registryLookup(ref,db) ))
				return (CNInstance *) r->value; }
		else {
			ref = newRegistry( IndexedByAddress );
			entry = registryRegister( string_arena, s, ref ); } }

	e = cn_new( NULL, NULL );
	e->sub[ 1 ] = (CNInstance *) entry;
	registryRegister( ref, db, e );
	db_op( DB_MANIFEST_OP, e, db );
	return e; }

//===========================================================================
//	bm_arena_deregister
//===========================================================================
static freeRegistryCB deregister_CB;

void
bm_arena_deregister( CNArena *arena, CNInstance *e, CNDB *db )
/*
	Assumption: we have e:( NULL, [ ., ref:{[db,e]} ] )
					^---- NULL if UBE
*/ {
	// remove [db,e] from ref registry and release e
	Pair *entry = (Pair *) e->sub[ 1 ];
	Registry *ref = entry->value;
	registryCBDeregister( ref, deregister_CB, db );

	// remove dangling entry from appropriate arena
	if (( ref->entries )) return;
	freeRegistry( ref, NULL );
	if ( !entry->name ) {
		listItem **ube_arena = (listItem **) &arena->value;
		removeIfThere( ube_arena, entry ); }
	else {
		Registry *string_arena = arena->name;
		listItem *next_i, *last_i=NULL;
		for ( listItem *i=string_arena->entries; i!=NULL; i=next_i ) {
			Pair *r = i->ptr;
			next_i = i->next;
			if ( r!=entry ) last_i = i;
			else {
				freeItem( i );
				if (( last_i )) last_i->next = next_i;
				else string_arena->entries = next_i;
				break; } } }
	freePair( entry ); }

static void deregister_CB( Registry *registry, Pair *entry ) {
	CNInstance *e = entry->value;
	e->sub[ 1 ] = NULL;
	cn_release( e ); }

//===========================================================================
//	bm_arena_flush
//===========================================================================
static inline void flush( listItem **, listItem *, CNDB *, listItem ** );

void
bm_arena_flush( CNArena *arena, Pair *shared, CNDB *db, listItem **trace ) {
	// flush string arena - only those are traced
	listItem *list = shared->name;
	if (( list )) {
		Registry *string_arena = arena->name;
		listItem **base = &string_arena->entries;
		flush( base, list, db, trace ); }
	reorderListItem( trace );
	// flush ube arena
	list = shared->value;
	if (( list )) {
		listItem *ube_arena = arena->value;
		listItem **base = (listItem **) &arena->value;
		flush( base, list, db, NULL ); } }

static inline void
flush( listItem **base, listItem *list, CNDB *db, listItem **trace ) {
	listItem *next_i, *last_i=NULL;
	for ( listItem *i=*base;(( i )&&( list )); i=next_i ) {
		Pair *r = i->ptr;
		next_i = i->next;
		if ( r!=list->ptr ) last_i = i;
		else {
			Registry *ref = r->value;
			registryCBDeregister( ref, deregister_CB, db );
			if ( !ref->entries ) {
				if (( trace )) addItem( trace, list->ptr );
				freeRegistry( ref, NULL );
				freeItem( i );
				freePair( r );
				if (( last_i )) last_i->next = next_i;
				else *base = next_i; }
			list = list->next; } } }

//===========================================================================
//	bm_arena_translate, bm_arena_transpose, bm_arena_lookup
//===========================================================================
CNInstance *
bm_arena_translate( CNInstance *x, CNDB *db ) {
	Registry *ref = ((Pair *) x->sub[ 1 ])->value; 
	Pair *entry = registryLookup( ref, db );
	return (( entry )? entry->value : NULL ); }

CNInstance *
bm_arena_transpose( CNInstance *x, CNDB *db ) {
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

CNInstance *
bm_arena_lookup( CNArena *arena, char *p, CNDB *db ) {
	if (( p )) {
		Registry *string_arena = arena->name;
		Pair *entry = registryLookup( string_arena, p );
		if (( entry )) {
			entry = registryLookup((Registry *) entry->value, db );
			if (( entry )) return entry->value; } }
	return NULL; }

//===========================================================================
//	newArena / freeArena
//===========================================================================
static freeRegistryCB free_string_CB;

CNArena *
newArena( void ) {
	Pair *arena = newPair( NULL, NULL );
	arena->name = newRegistry( IndexedByName );
	return (CNArena *) arena; }

void
freeArena( CNArena *arena ) {
	Registry *string_arena = arena->name;
	freeRegistry( string_arena, free_string_CB );
	listItem **ube_arena = (listItem **) &arena->value;
	for ( Pair *ube;( ube=popListItem(ube_arena) ); ) {
		freeRegistry( ube->value, deregister_CB );
		freePair( ube ); } }

static void free_string_CB( Registry *arena, Pair *entry ) {
	Registry *ref = entry->value;
	freeRegistry( ref, deregister_CB ); }

