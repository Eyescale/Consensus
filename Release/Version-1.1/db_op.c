#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"
#include "db_debug.h"

// #define DEBUG
// #define DB_DEBUG

//===========================================================================
//	db_op
//===========================================================================
void db_remove( CNInstance *, CNDB * );

int
db_op( DBOperation op, CNInstance *e, CNDB *db )
{
#ifdef DB_DEBUG
	return db_op_debug( op, e, db );
#endif
	CNInstance *nil = db->nil, *f, *g;
	switch ( op ) {
	case DB_EXIT_OP:
		nil->sub[ 1 ] = nil;
		break;

	case DB_MANIFEST_OP: // assumption: e was just created
		cn_new( cn_new( e, nil ), nil );
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
		else if ( op == DB_REHABILITATE_OP )
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
	}
	return 0;
}
void
db_remove( CNInstance *e, CNDB *db )
{
#ifdef DEBUG
fprintf( stderr, "db_remove:\t" );
db_output( stderr, "", e, db );
fprintf( stderr, "\n" );
#endif
	if (( e->sub[0] )) {
		cn_release( e );
		return;
	}
	cn_release( e );

#ifdef DEBUG
fprintf( stderr, "db_deregister:\t" );
db_output( stderr, "", e, db );
fprintf( stderr, "\n" );
#endif
	Registry *index = db->index;
	listItem *next_i, *last_i=NULL;
	for ( listItem *i=index->entries; i!=NULL; i=next_i ) {
		Pair *entry = i->ptr;
		next_i = i->next;
		if ( entry->value == e ) {
			if (( last_i )) last_i->next = next_i;
			else index->entries = next_i;
			free( entry->name );
			freePair( entry );
			freeItem( i );
			break;
		}
		else last_i = i;
	}
#ifdef UNIFIED
	e->sub[1] = NULL;
#endif
}

//===========================================================================
//	db_update
//===========================================================================
#define TRASH(x) \
	addItem( ((x->sub[0]) ? &trash[0] : &trash[1]), x )
void
db_update( CNDB *db )
/*
   cf design/specs/db-update.txt
*/
{
#ifdef DB_DEBUG
	db_update_debug( db );
	return;
#endif
	CNInstance *nil = db->nil;
	if ( nil->sub[ 0 ] == nil ) // remove init condition
		nil->sub[ 0 ] = NULL;

	CNInstance *f, *g, *x;
	listItem *trash[ 2 ] = { NULL, NULL };
#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		g = i->ptr;
		x = g->sub[ 1 ]; // manifested candidate
		if ( x->sub[0]==nil || x->sub[1]==nil )
			continue;
		f = cn_instance( x, nil, 1 );
		if (( g->as_sub[ 1 ] )) { // to-be-manifested
			if (( f )) // released to-be-manifested
				db_remove( f, db );
		}
		else if (( f ) && ( f->as_sub[ 0 ] )) { // reassigned
			db_remove( f->as_sub[0]->ptr, db );
			db_remove( f, db );
		}
		else db_remove( g, db ); // possibly to-be-released
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
		if (( f->as_sub[1] )) { // newborn to-be-released
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
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) {
			g = f->as_sub[1]->ptr;
			if ((next_i) && next_i->ptr==g )
				next_i = next_i->next;
			db_remove( g, db );
		}
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if ( !f->as_sub[1] ) {
			x = f->sub[0]; // released candidate
			db_remove( f, db );
			TRASH( x );
		}
	}
	for ( int i=0; i<2; i++ )
		while (( x = popListItem( &trash[i] ) ))
			db_remove( x, db );
#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ]; i!=NULL; i=i->next ) {
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) {
			db_remove( f->as_sub[1]->ptr, db );
		}
	}
#ifdef DEBUG
fprintf( stderr, "--\n" );
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
	. if privy==0
		returns 1 if either newborn (not reassigned) or released
		returns 0 otherwise
	. if privy==1 - traversing db_log( released, ... )
		returns 1 if newborn (not reassigned)
		returns 0 otherwise
	. if privy==2 - traversing %!
		returns 1 if e:( ., nil ) or e:( nil, . )
		returns 0 otherwise
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[0]==nil || e->sub[1]==nil )
		return 1;
	if ( privy == 2 ) return 0;
	CNInstance *f = cn_instance( e, nil, 1 );
	if (( f )) {
		if ( f->as_sub[ 1 ] ) // to be released
			return !!f->as_sub[ 0 ]; // newborn
		if ( f->as_sub[ 0 ] ) // newborn or reassigned
			return !cn_instance( nil, e, 0 );
		return !privy; // released
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
	returns 0 if e is either released or to-be-released,
		which we assume applies to all its ascendants
	returns 1 otherwise.
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[0]==nil || e->sub[1]==nil )
		return 0;
	CNInstance *f = cn_instance( e, nil, 1 );
	return ( !f || ( f->as_sub[0] && !f->as_sub[1] ));
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
//	db_in
//===========================================================================
int
db_in( CNDB *db )
{
	CNInstance *nil = db->nil;
	return ( nil->sub[ 0 ] == nil );
}

//===========================================================================
//	db_out
//===========================================================================
int
db_out( CNDB *db )
{
	CNInstance *nil = db->nil;
	return ( nil->sub[ 1 ] == nil );
}

