#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

// #define DEBUG
// #define FORCE_NULL

//===========================================================================
//	newCNDB
//===========================================================================
CNDB *
newCNDB( void )
{
#ifdef FORCE_NULL
	Registry *index = newRegistry( IndexedByName );
#else
	Registry *index = newRegistry( IndexedByCharacter );
#endif
	CNInstance *nil = cn_new( NULL, NULL );
	CNDB *db = (CNDB *) newPair( nil, index );
	char *p = strdup( "init" );
#ifdef UNIFIED
	CNInstance *init = cn_new( NULL, (CNInstance*) p );
#else
	CNInstance *init = cn_new( NULL, NULL );
#endif
	registryRegister( db->index, p, init );
	cn_new( nil, init );
	return db;
}

//===========================================================================
//	freeCNDB
//===========================================================================
static freeRegistryCB free_CB;

void
freeCNDB( CNDB *db )
{
	if ( db == NULL ) return;

	/* delete all CNDB entities, proceeding top down
	*/
	listItem *stack = NULL;
	listItem *n = newItem( db->nil );
	listItem *i = n;
#ifdef DEBUG
	int debug_pass=1;
	do {
#endif
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
#ifdef DEBUG
			if ( debug_pass ) {
				fprintf( stderr, "freeCNDB: removing " );
				db_output( stderr, "", e, db );
				fprintf( stderr, "\n" );
			}
			else cn_free( e );
#else
			cn_free( e );
#endif
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
#ifdef DEBUG
	if ( debug_pass ) i = n = newItem( db->nil );
	} while ( debug_pass-- );
#endif
	/* then delete CNDB per se
	*/
	freeRegistry( db->index, free_CB );
	freePair((Pair *) db );
}
static void
free_CB( Registry *registry, Pair *pair )
{
	free( pair->name );
}

//===========================================================================
//	cn_new
//===========================================================================
CNInstance *
cn_new( CNInstance *source, CNInstance *target )
{
	Pair *sub = newPair( source, target );
	Pair *as_sub = newPair( NULL, NULL );
	CNInstance *instance = (CNInstance *) newPair( sub, as_sub );
#ifdef UNIFIED
	if (( source )) {
		addItem( &source->as_sub[0], instance );
		if (( target )) addItem( &target->as_sub[1], instance );
	}
#else
	if (( source )) addItem( &source->as_sub[0], instance );
	if (( target )) addItem( &target->as_sub[1], instance );
#endif
	return instance;
}

//===========================================================================
//	cn_free
//===========================================================================
void
cn_free( CNInstance *e )
{
	if ( e == NULL ) return;
	freePair((Pair *) e->sub );
	freeListItem( &e->as_sub[ 0 ] );
	freeListItem( &e->as_sub[ 1 ] );
	freePair((Pair *) e->as_sub );
	freePair((Pair *) e );
}

//===========================================================================
//	cn_instance
//===========================================================================
CNInstance *
cn_instance( CNInstance *e, CNInstance *f, int pivot )
{
	switch ( pivot ) {
	case 0:
		for ( listItem *i=f->as_sub[ 1 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			if ( instance->sub[0] == e )
				return instance;
		}
		break;
	default:
		for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
			CNInstance *instance = i->ptr;
			if ( instance->sub[1] == f )
				return instance;
		}
		break;
	}
	return NULL;
}

//===========================================================================
//	db_register
//===========================================================================
CNInstance *
db_register( char *p, CNDB *db, int notify )
{
	if ( p == NULL ) return NULL;
#ifdef FORCE_NULL
	p = strmake( p );
#endif
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, p );
	if (( entry )) {
		e = entry->value;
		if ( notify ) db_op( DB_REHABILITATE_OP, e, db );
#ifdef FORCE_NULL
		free( p );
#endif
	}
	else {
#ifndef FORCE_NULL
		p = strmake( p );
#endif
#ifdef UNIFIED
		e = cn_new( NULL, (CNInstance*) p );
#else
		e = cn_new( NULL, NULL );
#endif
		registryRegister( db->index, p, e );
		if ( notify ) db_op( DB_MANIFEST_OP, e, db );
	}
	return e;
}

//===========================================================================
//	db_deprecate
//===========================================================================
void
db_deprecate( CNInstance *x, CNDB *db )
/*
	deprecates x and all its ascendants, proceeding top-down
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
				ndx = (int) popListItem( &stack );
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

			CNInstance *sub = instance->sub[ 1 ];
			removeItem( &sub->as_sub[1], instance );
			instance->sub[ 1 ] = f;
			addItem( &f->as_sub[1], instance );

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
	Assumption: neither e nor f are deprecated
	Special case: assignment - i.e. e is ( *, variable )
		in this case must release previous assignment
		Note that until next frame, variable will hold
		both the old and the new values (feature)
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
		CNInstance *reassignment = NULL;
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			instance = i->ptr;
			if ( instance->sub[ 1 ] == f ) {
				db_op( DB_REASSIGN_OP, instance, db );
				reassignment = instance;
			}
			else if ( db_deprecatable( instance, db ) )
				db_deprecate( instance, db );
		}
		if (( reassignment )) return reassignment;
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
//	db_lookup
//===========================================================================
CNInstance *
db_lookup( int privy, char *p, CNDB *db )
{
	if ( p == NULL )
		return NULL;
#ifdef FORCE_NULL
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

//===========================================================================
//	db_exit
//===========================================================================
void
db_exit( CNDB *db )
{
	db_op( DB_EXIT_OP, NULL, db );
}
