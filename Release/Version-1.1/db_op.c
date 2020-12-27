#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

// #define DEBUG

//===========================================================================
//	db_op
//===========================================================================
static void db_remove( CNInstance *, CNDB * );

void
db_op( DBOperation op, CNInstance *e, CNDB *db )
/*
	Assumptions
	1. op==DB_MANIFEST_OP - e is newly created
	2. op==DB_DEPRECATE_OP - e is not deprecated

*/
{
	CNInstance *nil = db->nil, *f, *g;
	switch ( op ) {
	case DB_EXIT_OP:
		if ( db_out(db) ) break;
		cn_new( nil, nil );
		break;

	case DB_DEPRECATE_OP:
		f = cn_instance( e, nil, 1 );
		if (( f )) {
			// case to-be-released, possibly manifested (cannot be to-be-manifested)
			if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
				break; // already to-be-released, possibly manifested
			}
			// case newborn, possibly manifested (reassigned)
			else if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( g = cn_instance( nil, e, 0 ) )) {
					// remove (( e, nil ), nil ) only
					db_remove( f->as_sub[0]->ptr, db );
				}
				// create ( nil, ( e, nil )) (to be released)
				cn_new( nil, f );
			}
			// case released, possibly to-be-manifested (rehabilitated)
			else if (( g = cn_instance( nil, e, 0 ) )) {
				// remove ( nil, ( nil, e )) and ( nil, e )
				db_remove( g->as_sub[1]->ptr, db );
				// remove ( nil, e )
				db_remove( g, db );
			}
			// else already released
		}
		else {
			/* neither released, nor newborn, nor to-be-released
			   possibly manifested or to-be-manifested
			*/
			g = cn_instance( nil, e, 0 );
			if (( g ) && ( g->as_sub[ 1 ] )) {
				// remove ( nil, ( nil, e )) and ( nil, e )
				db_remove( g->as_sub[1]->ptr, db );
				db_remove( g, db );
			}
			cn_new( nil, cn_new( e, nil ) );
		}
		break;
	case DB_REASSIGN_OP:
	case DB_REHABILITATE_OP:
		f = cn_instance( e, nil, 1 );
		if (( f )) {
			// case to-be-released, possibly manifested (cannot be to-be-manifested)
			if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
				// remove ( nil, ( e, nil )) and ( e, nil )
				db_remove( f->as_sub[1]->ptr, db );
				db_remove( f, db );
			}
			// case newborn, possibly manifested (reassigned)
			else if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				; // leave as-is
			}
			// case released, possibly to-be-manifested (rehabilitated)
			else if (( g = cn_instance( nil, e, 0 ) ))
				; // already to-be-manifested
			else {
				// create ( nil, ( nil, e )) (to be manifested)
				cn_new( nil, cn_new( nil, e ) );
			}
		}
		else if ( op != DB_REASSIGN_OP )
			break;
		else {
			/* neither released, nor newborn, nor to-be-released
			   possibly manifested or to-be-manifested
			*/
			g = cn_instance( nil, e, 0 );
			if (( g )) {
				if (!( g->as_sub[ 1 ] )) {
					// manifested - create newborn (reassigned)
					cn_new( cn_new( e, nil ), nil );
				}
				else ; // already to-be-manifested
			}
			else {
				// create ( nil, ( nil, e )) (to be manifested)
				cn_new( nil, cn_new( nil, e ) );
			}
		}
		break;

	case DB_MANIFEST_OP: // assumption: the entity was just created
		cn_new( cn_new( e, nil ), nil );
		break;
	}
}

//===========================================================================
//	db_update
//===========================================================================
static void db_deregister( CNInstance *, CNDB * );

#define TRASH( x ) \
	if (( x->sub[0] )) addItem( &trash[0], x ); \
	else addItem( &trash[1], x );
#define EMPTY_TRASH \
	while (( x = popListItem( &trash[0] ) )) \
		db_remove( x, db ); \
	while (( x = popListItem( &trash[1] ) )) { \
		db_deregister( x, db ); \
		db_remove( x, db ); \
	}

void
db_update( CNDB *db )
/*
   cf design/specs/db-update.txt
*/
{
	CNInstance *nil = db->nil;
	CNInstance *f, *g, *x;
	listItem *trash[ 2 ] = { NULL, NULL };
#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		g = i->ptr;
		if (( g->as_sub[ 1 ] )) continue; // to be manifested
		x = g->sub[ 1 ]; // manifested candidate
		if ( x==nil || x->sub[0]==nil || x->sub[1]==nil )
			continue;
		f = cn_instance( x, nil, 1 );
		if (( f )) {
			if (( f->as_sub[ 1 ] )) // to be released
				continue;
			if (( f->as_sub[ 0 ] )) { // reassigned
				db_remove( f->as_sub[0]->ptr, db );
				db_remove( f, db );
			}
		}
		else db_remove( g, db );
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 2. actualize newborn entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if ( !f->as_sub[ 0 ] ) continue;
		x = f->sub[0]; // newborn candidate
		g = f->as_sub[0]->ptr;
		if ((next_i) && next_i->ptr==g )
			next_i = next_i->next;
		if (( f->as_sub[1] )) { // to-be-released
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( g, db );
			db_remove( f, db );
			TRASH( x );
		}
		else { // just newborn
			db_remove( g, db );
			db_remove( f, db );
			cn_new( nil, x );
		}
	}
	EMPTY_TRASH
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if ( !f->as_sub[ 1 ] ) continue;
		x = f->sub[1]; // to-be-manifested candidate
		g = f->as_sub[1]->ptr;
		if ((next_i) && next_i->ptr==g )
			next_i = next_i->next;
		if (( f = cn_instance( x, nil, 1 ) )) {
			db_remove( g, db );
			db_remove( f, db );
		}
		else db_remove( g, db );
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) continue; // to be released
		x = f->sub[ 0 ]; // released candidate
		if ( x==nil || x->sub[0]==nil || x->sub[1]==nil )
			continue;
		else {
			db_remove( f, db );
			TRASH( x );
		}
	}
	EMPTY_TRASH
#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if ( !f->as_sub[ 1 ] ) continue;
		x = f->sub[0]; // to-be-released candidate
		g = f->as_sub[1]->ptr;
		if (( f = cn_instance( nil, x, 0 ) )) {
			db_remove( g, db );
			db_remove( f, db );
		}
		else db_remove( g, db );
	}
#ifdef DEBUG
fprintf( stderr, "--\n" );
#endif
}

//===========================================================================
//	db_remove	- private
//===========================================================================
static void
db_remove( CNInstance *e, CNDB *db )
/*
	Removes e from db
	Assumption: db_deregister has been called, when needed, separately
*/
{
#ifdef DEBUG
fprintf( stderr, "db_remove:\t" );
db_output( stderr, "", e, db );
fprintf( stderr, "\n" );
#endif
#ifndef UNIFIED
	/* 1. remove e from the as_sub lists of its subs
	*/
	CNInstance *sub;
	if (( sub = e->sub[0] )) removeItem( &sub->as_sub[0], e );
	if (( sub = e->sub[1] )) removeItem( &sub->as_sub[1], e );

	/* 2. nullify e as sub of its as_sub's
	   note: these will be free'd as well, as e was deprecated
	*/
	for ( listItem *j=e->as_sub[ 0 ]; j!=NULL; j=j->next ) {
		CNInstance *instance = j->ptr;
		instance->sub[ 0 ] = NULL;
	}
	for ( listItem *j=e->as_sub[ 1 ]; j!=NULL; j=j->next ) {
		CNInstance *instance = j->ptr;
		instance->sub[ 1 ] = NULL;
	}
	/* 3. free e
	*/
	cn_free( e );
#else
	/* 1. remove e from the as_sub lists of its subs
	*/
	CNInstance *sub;
	if (( sub = e->sub[0] )) {
		removeItem( &sub->as_sub[0], e );
		removeItem( &e->sub[1]->as_sub[1], e );
	}
	else if (( sub = e->sub[1] )) {
		Pair *pair = (Pair *) sub;
		if (( pair->name )) removeItem((listItem **) pair->name, e );
		if (( pair->value )) removeItem((listItem **) pair->value, e );
		freePair( pair );
	}

	/* 2. nullify e as sub of its as_sub's
	   note: these will be free'd as well, as e was deprecated
	*/
	for ( listItem *j=e->as_sub[ 0 ]; j!=NULL; j=j->next ) {
		CNInstance *instance = j->ptr;
		Pair *pair;
		if (( instance->sub[0] )) {
			sub = instance->sub[ 1 ];
			pair = newPair( NULL, &sub->as_sub[1] );
			instance->sub[0] = NULL;
			instance->sub[1] = (CNInstance *) pair;
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
		CNInstance *instance = j->ptr;
		Pair *pair;
		if (( instance->sub[0] )) {
			sub = instance->sub[ 0 ];
			pair = newPair( &sub->as_sub[0], NULL );
			instance->sub[0] = NULL;
			instance->sub[1] = (CNInstance *) pair;
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
//	db_deregister	- private
//===========================================================================
static void
db_deregister( CNInstance *e, CNDB *db )
/*
	removes e from db index if it's there
	Assumptions:
	1. e->sub[0] == NULL already verified
	2. db_remove will be called separately
*/
{
#ifdef DEBUG
fprintf( stderr, "db_deregister:\t" );
db_output( stderr, "", e, db );
fprintf( stderr, "\n" );
#endif
	Registry *index = db->index;
	listItem *next_i, *last_i=NULL;
	for ( listItem *i=index->entries; i!=NULL; last_i=i, i=next_i ) {
		Pair *entry = i->ptr;
		next_i = i->next;
		if ( entry->value == e ) {
			if (( last_i )) {
				last_i->next = next_i;
			}
			else index->entries = next_i;
			free( entry->name );
			freePair( entry );
			freeItem( i );
			break;
		}
	}
#ifdef UNIFIED
	e->sub[1] = NULL;
#endif
}

//===========================================================================
//	db_log
//===========================================================================
CNInstance *
db_log( int first, int released, CNDB *db, listItem **stack )
/*
	returns first / next e verifying
		if released: ( e, nil )
		if !released: ( nil, e )
		where e != (nil,.) (.,nil) nil
*/
{
	CNInstance *nil = db->nil;
	listItem *i = first ?
		nil->as_sub[ released ? 1 : 0 ] :
		((listItem *) popListItem( stack ))->next;

	for ( ; i!=NULL; i=i->next ) {
		CNInstance *g = i->ptr;
		if ((g->as_sub[0]) || (g->as_sub[1]))
			continue;
		CNInstance *e = g->sub[ released ? 0 : 1 ];
		if ( e==nil || e->sub[0]==nil || e->sub[1]==nil )
			continue;
		addItem( stack, i );
		return e;
	}
	return NULL;
}

//===========================================================================
//	db_private
//===========================================================================
int
db_private( int privy, CNInstance *e, CNDB *db )
/*
	called by xp_traverse() and xp_verify()
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[0]==nil || e->sub[1]==nil )
		return 1;
	CNInstance *f = cn_instance( e, nil, 1 );
	if (( f )) {
		if ( f->as_sub[ 0 ] ) // newborn or reassigned
			return !( cn_instance( nil, e, 0 ) );
		if ( f->as_sub[ 1 ] ) // to be released
			return 0;
		return !privy; // deprecated
	}
	return 0;
}

//===========================================================================
//	db_deprecatable
//===========================================================================
int
db_deprecatable( CNInstance *e, CNDB *db )
/*
	called by db_deprecate()
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[0]==nil || e->sub[1]==nil )
		return 0;
	CNInstance *f = cn_instance( e, nil, 1 );
	if (( f )) {
		if ( f->as_sub[ 0 ] ) // newborn or reassigned
			return 1;
		if ( f->as_sub[ 1 ] ) // to be released
			return 1;
		return 0; // deprecated
	}
	return 1;
}

//===========================================================================
//	db_still
//===========================================================================
int
db_still( CNDB *db )
{
	CNInstance *nil = db->nil;
	return !( nil->as_sub[0] || nil->as_sub[1] );
}

//===========================================================================
//	db_out
//===========================================================================
int
db_out( CNDB *db )
{
	CNInstance *nil = db->nil;
	return !!( cn_instance( nil, nil, 1 ) );
}

