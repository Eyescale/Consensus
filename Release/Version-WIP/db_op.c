#include <stdio.h>
#include <stdlib.h>

#include "database.h"

// #define DEBUG

//===========================================================================
//	db_op
//===========================================================================
int
db_op( DBOperation op, CNInstance *e, CNDB *db ) {
	CNInstance *nil = db->nil, *f, *g;
	switch ( op ) {
	case DB_MANIFEST_OP: // assumption: e was just created
		cn_new( cn_new( e, nil ), nil );
		break;
	case DB_DEPRECATE_OP:
		f = cn_instance( e, nil, 1 );
		if (( f )) {
			// case newborn, possibly to-be-released, xor
			// manifested (reassigned)
			if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( cn_instance( nil, e, 0 ) )) {
					// release (( e, nil ), nil ) only
					cn_release( f->as_sub[0]->ptr ); }
				// create ( nil, ( e, nil )) if not existing
				if ( !f->as_sub[ 1 ] ) cn_new( nil, f ); }
			// case to-be-released, possibly manifested
			// (cannot be to-be-manifested)
			else if (( f->as_sub[ 1 ] )) // can only be ( nil, f )
				break; // already to-be-released, possibly manifested
			// case released, possibly to-be-released (signal), xor
			// to-be-manifested (rehabilitated)
			else if (( g = cn_instance( nil, e, 0 ) )) {
				if (( g->as_sub[ 1 ] )) { // rehabilitated
					// release ( nil, ( nil, e ))
					cn_release( g->as_sub[1]->ptr ); }
				if ( !g->as_sub[ 0 ] ) {
					// create ( ( nil, e ), nil ) (signal)
					cn_new( g, nil ); } }
			// just released
			else {	// create ( ( nil, e ), nil ) (signal)
				cn_new( cn_new( nil, e ), nil ); } }
		else {
			/* neither released, nor newborn, nor to-be-released
			   possibly manifested or to-be-manifested
			*/
			g = cn_instance( nil, e, 0 );
			if (( g ) && ( g->as_sub[ 1 ] )) {
				// release ( nil, ( nil, e )) and ( nil, e )
				cn_release( g->as_sub[1]->ptr );
				cn_release( g ); }
			cn_new( nil, cn_new( e, nil ) ); }
		break;
	case DB_REASSIGN_OP:
	case DB_REHABILITATE_OP:
		f = cn_instance( e, nil, 1 );
		if (( f )) {
			// case newborn, possibly to-be-released, xor
			// manifested (reassigned)
			if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
					// release ( nil, ( e, nil ))
					cn_release( f->as_sub[1]->ptr ); } }
			// case to-be-released, possibly manifested
			// (cannot be to-be-manifested)
			else if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
				// release ( nil, ( e, nil ))
				cn_release( f->as_sub[1]->ptr );
				// release ( e, nil ) if not newborn
				if ( !f->as_sub[ 0 ] ) cn_release( f ); }
			// case released, possibly to-be-released (signal), xor
			// to-be-manifested (rehabilitated)
			else if (( g = cn_instance( nil, e, 0 ) )) {
				if (( g->as_sub[ 0 ] )) { // signal
					cn_release( g->as_sub[0]->ptr );
					// create ( nil, ( nil, e ) ) (rehabilitated)
					cn_new( nil, g ); } }
			// just released
			else {	// create ( nil, ( nil, e )) (to be manifested)
				cn_new( nil, cn_new( nil, e ) ); } }
		else if ( op==DB_REHABILITATE_OP )
			break;
		else {
			/* neither released, nor newborn, nor to-be-released
			   possibly manifested or to-be-manifested
			*/
			g = cn_instance( nil, e, 0 );
			if (( g )) {
				if (!( g->as_sub[ 1 ] )) {
					// manifested - create newborn (reassigned)
					cn_new( cn_new( e, nil ), nil ); }
				/* else already to-be-manifested */ }
			else {
				// create ( nil, ( nil, e )) (to be manifested)
				cn_new( nil, cn_new( nil, e ) ); } }
		break; }
	return 0; }

//---------------------------------------------------------------------------
//	db_update
//---------------------------------------------------------------------------
// cf design/specs/db-update.txt

void
db_update( CNDB *db, DBRemoveCB user_CB, void *user_data ) {
	CNInstance *nil = db->nil;
	CNInstance *f, *g, *x;
#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		g = i->ptr;	// g:( nil, . )
		x = g->sub[ 1 ]; // manifested candidate
		if ( x->sub[0]==nil || x->sub[1]==nil )
			continue;
		f = cn_instance( x, nil, 1 );
		if (( g->as_sub[0] )) { // released to-be-released
			CNInstance *h = g->as_sub[ 0 ]->ptr;
			if ((next_i) && next_i->ptr==h )
				next_i = next_i->next;
			cn_release( h ); // h:(g:(nil,.),nil)
			cn_release( g );
			cn_new( nil, f ); } // handled below
		else if (( g->as_sub[ 1 ] )) { // to-be-manifested
			if (( f )) // released to-be-manifested
				cn_release( f ); }
		else if (( f ) && ( f->as_sub[ 0 ] )) { // reassigned
			cn_release( f->as_sub[0]->ptr );
			cn_release( f ); }
		else cn_release( g ); } // possibly to-be-released
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
			// turn into to-be-released, handled below
			cn_release( f->as_sub[1]->ptr );
			cn_release( g );
			cn_release( f );
			// reordering x in nil->as_sub[1]
			cn_new( nil, cn_new( x, nil ) ); }
		else { // just newborn
			cn_release( g );
			cn_release( f );
			cn_new( nil, x ); } }
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to-be-manifested entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) {
			g = f->as_sub[1]->ptr;
			if ((next_i) && next_i->ptr==g )
				next_i = next_i->next;
			cn_release( g ); } }
#ifdef DEBUG
fprintf( stderr, "db_update: 4. actualize to-be-released entities\n" );
#endif
	listItem *released = NULL;
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) { // to-be-released
			cn_release( f->as_sub[1]->ptr ); }
		else {
			x = f->sub[0]; // released candidate
			addItem( &released, x ); // reordered
			cn_release( f ); } }
#ifdef DEBUG
fprintf( stderr, "db_update: 5. remove released entities\n" );
#endif
	while (( x=popListItem( &released ) )) {
		if (( user_CB ) && user_CB( db, x, user_data ) )
			continue; // takes care of shared
		else if ( !isBase(x) ) cn_release( x );
		else if ( cnIsProxy(x) ) free_proxy( x );
		else db_deregister( x, db ); }
#ifdef DEBUG
fprintf( stderr, "--\n" );
#endif
	}

//===========================================================================
//	DBLog
//===========================================================================
CNInstance *
DBLog( int first, int released, CNDB *db, listItem **stack )
/*
	returns first / next e verifying
		if released: ( e, nil )
		if !released: ( nil, e )
		where e != (nil,.) (.,nil) nil
*/ {
	if ( !stack && !first ) return NULL;
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
		if (( stack )) addItem( stack, i );
		return e; }

	return NULL; }

