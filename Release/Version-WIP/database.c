#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

// #define DEBUG
// #define NULL_TERMINATED

//===========================================================================
//	newCNDB
//===========================================================================
CNDB *
newCNDB( void )
{
#ifdef NULL_TERMINATED
	Registry *index = newRegistry( IndexedByName );
#else
	Registry *index = newRegistry( IndexedByCharacter );
#endif
	CNInstance *nil = cn_new( NULL, NULL );
	return (CNDB *) newPair( nil, index );
}

//===========================================================================
//	freeCNDB
//===========================================================================
static freeRegistryCB free_CB;

void
freeCNDB( CNDB *db )
/*
	delete all CNDB entities, proceeding top down
*/
{
	if ( db == NULL ) return;
	cn_prune( db->nil );
	freeRegistry( db->index, free_CB );
	freePair((Pair *) db );
}
static void
free_CB( Registry *registry, Pair *pair )
{
	free( pair->name );
	cn_prune((CNInstance *) pair->value );
}

//===========================================================================
//	db_register
//===========================================================================
CNInstance *
db_register( char *p, CNDB *db, int manifest )
/*
	register instance represented by p
	manifest only if requested AND if instance is new or rehabilitated
*/
{
	if ( p == NULL ) return NULL;
#ifdef NULL_TERMINATED
	p = strmake( p );
#endif
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, p );
	if (( entry )) {
		e = entry->value;
		if ( manifest ) db_op( DB_REHABILITATE_OP, e, db );
#ifdef NULL_TERMINATED
		free( p );
#endif
	}
	else {
#ifndef NULL_TERMINATED
		p = strmake( p );
#endif
		e = cn_new( NULL, NULL );
#ifdef UNIFIED
		e->sub[ 1 ] = (CNInstance *) p;
#endif
		registryRegister( db->index, p, e );
		if ( manifest ) db_op( DB_MANIFEST_OP, e, db );
	}
	return e;
}

//===========================================================================
//	db_couple
//===========================================================================
CNInstance *
db_couple( CNInstance *e, CNInstance *f, CNDB *db )
/*
	Assumption: called upon CNDB creation - cf. cnLoad()/bm_substantiate()
	Same as db_instantiate except without change registration
*/
{
	CNInstance *instance;
	if (( instance = cn_instance( e, f, 0 ) ))
		return instance;
	else if (( e->as_sub[ 0 ] )) {
		Pair *star = registryLookup( db->index, "*" );
		if (( star ) && ( e->sub[ 0 ] == star->value )) {
			instance = e->as_sub[ 0 ]->ptr;
			fprintf( stderr, "B%%::db_couple: Warning: reassigning " );
			db_output( stderr, "", instance, db );
			fprintf( stderr, " to " );
			cn_rewire( instance, 1, f );
			db_output( stderr, "", instance, db );
			fprintf( stderr, "\n" );
			return instance;
		}
	}
	return cn_new( e, f );
}

//===========================================================================
//	db_instantiate
//===========================================================================
CNInstance *
db_instantiate( CNInstance *e, CNInstance *f, CNDB *db )
/*
	Assumption: neither e nor f are released
	Special case: assignment - i.e. e is ( *, variable )
		in which case we must deprecate current assignment
	Note that until next frame, ((*,e), . ) will exist both as
		the current (either to-be-released or reassigned) and
		the new (either newborn, reassigned, or released-to-be-manifested)
	assignments.
*/
{
#ifdef DEBUG
	fprintf( stderr, "db_instantiate: ( " );
	db_output( stderr, "", e, db );
	fprintf( stderr, ", " );
	db_output( stderr, "", f, db );
	fprintf( stderr, " )\n" );
#endif
	CNInstance *instance = NULL;
	Pair *star = registryLookup( db->index, "*" );
	if (( star ) && ( e->sub[ 0 ] == star->value )) {
		/* Assignment case - as we have e:( *, . )
		*/
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( candidate->sub[ 1 ] == f ) {
				instance = candidate;
			}
			else if ( db_deprecatable( candidate, db ) )
				db_deprecate( candidate, db );
		}
		if (( instance )) {
			db_op( DB_REASSIGN_OP, instance, db );
			return instance;
		}
	}
	else {
		/* General case
		*/
		instance = cn_instance( e, f, 0 );
		if (( instance )) {
			db_op( DB_REHABILITATE_OP, instance, db );
			return instance;
		}
	}
	instance = cn_new( e, f );
	db_op( DB_MANIFEST_OP, instance, db );
	return instance;
}

//===========================================================================
//	db_deprecate
//===========================================================================
void
db_deprecate( CNInstance *x, CNDB *db )
/*
	deprecate (ie. set "to-be-released") x and all its ascendants,
	proceeding top-down
*/
{
	if ( x == NULL ) return;
	listItem * stack = NULL,
		 * i = newItem( x ),
		 * j;
	int ndx = 0;
	for ( ; ; ) {
		x = i->ptr;
		for ( j=x->as_sub[ ndx ]; j!=NULL; j=j->next )
			if ( db_deprecatable( j->ptr, db ) )
				break;
		if (( j )) {
			addItem( &stack, i );
			add_item( &stack, ndx );
			ndx = 0;
			i = j; continue;
		}
		for ( ; ; ) {
			if ( ndx == 0 ) {
				ndx = 1;
				break;
			}
			if ((x)) {
				db_op( DB_DEPRECATE_OP, x, db );
			}
			if (( i->next )) {
				i = i->next;
				if ( !db_deprecatable( i->ptr, db ) )
					x = NULL;
				else {
					ndx = 0;
					break;
				}
			}
			else if (( stack )) {
				ndx = pop_item( &stack );
				i = popListItem( &stack );
				if ( ndx ) x = i->ptr;
			}
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
}

//===========================================================================
//	db_lookup
//===========================================================================
CNInstance *
db_lookup( int privy, char *p, CNDB *db )
{
	if ( p == NULL )
		return NULL;
#ifdef NULL_TERMINATED
	p = strmake( p );
	Pair *entry = registryLookup( db->index, p );
	free( p );
#else
	Pair *entry = registryLookup( db->index, p );
#endif
	return (( entry ) && !db_private( privy, entry->value, db )) ?
		entry->value : NULL;
}

//===========================================================================
//	db_identifier
//===========================================================================
char *
db_identifier( CNInstance *e, CNDB *db )
/*
	Assumption: e->sub[0]==NULL already verified
*/
{
#ifdef UNIFIED
	return (char *) e->sub[ 1 ];
#else
	Pair *entry = registryLookup( db->index, NULL, e );
	return ( entry ) ? entry->name : NULL;
#endif
}

//===========================================================================
//	db_is_empty
//===========================================================================
int
db_is_empty( int privy, CNDB *db )
{
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		if ( !db_private( privy, entry->value, db ) )
			return 0;
	}
	return 1;
}

//===========================================================================
//	db_first, db_next, db_traverse
//===========================================================================
CNInstance *
db_first( CNDB *db, listItem **stack )
{
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *e = entry->value;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e;
		}
	}
	return NULL;
}
CNInstance *
db_next( CNDB *db, CNInstance *e, listItem **stack )
{
	if ( e == NULL ) return NULL;
	if (( e->as_sub[ 0 ] )) {
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			e = i->ptr;
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e;
			}
		}
	}
	listItem *i = popListItem( stack );
	for ( ; ; ) {
		if (( i->next )) {
			i = i->next;
			if (( *stack ))
				e = i->ptr;
			else {
				// e in db->index
				Pair *entry = i->ptr;
				e = entry->value;
			}
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e;
			}
		}
		else if (( *stack )) {
			i = popListItem( stack );
		}
		else return NULL;
	}
}
int
db_traverse( int privy, CNDB *db, DBTraverseCB user_CB, void *user_data )
/*
	traverses whole CNDB applying user_CB to each entity
*/
{
	listItem *stack = NULL;
	listItem *n = ( privy ? newItem( db->nil ) : NULL );
	listItem *i = ( privy ? n : db->index->entries );
	while (( i )) {
		CNInstance *e;
		if (( stack ) || (n))
			e = i->ptr;
		else {
			Pair *pair = i->ptr;
			e = pair->value;
		}
		listItem *j = e->as_sub[ 0 ];
		if (( j )) {
			addItem( &stack, i );
			i = j; continue;
		}
		while (( i )) {
			if ( user_CB( e, db, user_data ) ) {
				freeListItem( &n );
				freeListItem( &stack );
				return 1;
			}
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				if ((stack) || (n))
					e = i->ptr;
				else {
					Pair *pair = i->ptr;
					e = pair->value;
				}
			}
			else if (( n )) {
				freeListItem( &n );
				i = db->index->entries;
				break;
			}
			else i = NULL;
		}
	}
	return 0;
}

