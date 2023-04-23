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

int
db_op( DBOperation op, CNInstance *e, CNDB *db )
{
	CNInstance *nil = db->nil, *f, *g;
	switch ( op ) {
	case DB_MANIFEST_OP: // assumption: e was just created
		cn_new( cn_new( e, nil ), nil );
		break;

	case DB_DEPRECATE_OP:
	case DB_SIGNAL_OP:
		f = cn_instance( e, nil, 1 );
		if (( f )) {
			// case newborn, possibly to-be-released, xor
			// manifested (reassigned)
			if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( cn_instance( nil, e, 0 ) )) {
					// remove (( e, nil ), nil ) only
					db_remove( f->as_sub[0]->ptr, db ); }
				// create ( nil, ( e, nil )) if not existing
				if ( !f->as_sub[ 1 ] ) cn_new( nil, f ); }
			// case to-be-released, possibly manifested
			// (cannot be to-be-manifested)
			else if (( f->as_sub[ 1 ] )) // can only be ( nil, f )
				break; // already to-be-released, possibly manifested
			// case released, possibly to-be-released (signal), xor
			// to-be-manifested (rehabilitated)
			else if (( g = cn_instance( nil, e, 0 ) )) {
				switch ( op ) {
				case DB_DEPRECATE_OP:
					if (( g->as_sub[ 1 ] )) { // rehabilitated
						// remove ( nil, ( nil, e )) and ( nil, e )
						db_remove( g->as_sub[1]->ptr, db );
						db_remove( g, db ); }
					break;
				default: // DB_SIGNAL_OP:
					if (( g->as_sub[ 1 ] )) { // rehabilitated
						// remove ( nil, ( nil, e ))
						db_remove( g->as_sub[1]->ptr, db ); }
					if ( !g->as_sub[ 0 ] ) {
						// create ( ( nil, e ), nil ) (signal)
						cn_new( g, nil ); } } }
			// just released
			else if ( op==DB_SIGNAL_OP ) {
				// create ( ( nil, e ), nil ) (signal)
				cn_new( cn_new( nil, e ), nil ); } }
		else {
			/* neither released, nor newborn, nor to-be-released
			   possibly manifested or to-be-manifested
			*/
			g = cn_instance( nil, e, 0 );
			if (( g ) && ( g->as_sub[ 1 ] )) {
				// remove ( nil, ( nil, e )) and ( nil, e )
				db_remove( g->as_sub[1]->ptr, db );
				db_remove( g, db ); }
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
					// remove ( nil, ( e, nil ))
					db_remove( f->as_sub[1]->ptr, db ); } }
			// case to-be-released, possibly manifested
			// (cannot be to-be-manifested)
			else if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
				// remove ( nil, ( e, nil ))
				db_remove( f->as_sub[1]->ptr, db );
				// remove ( e, nil ) if not newborn
				if ( !f->as_sub[ 0 ] ) db_remove( f, db ); }
			// case released, possibly to-be-released (signal), xor
			// to-be-manifested (rehabilitated)
			else if ( !cn_instance( nil, e, 0 ) ) { // just released
				// create ( nil, ( nil, e )) (to be manifested)
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
	return 0;
}
static void
db_remove( CNInstance *e, CNDB *db )
{
	if (( e->sub[0] )) {
		if (( e->sub[ 1 ] ))
			cn_release( e );
		else {
			/* e is proxy:( connection:(this,that), NULL )
			   where this==NULL denotes current cell's Self proxy
			   cn_release( connection ) will remove connection
			   from this->as_sub[ 0 ] and from that->as_sub[ 1 ]
			*/
			CNEntity *connection = e->sub[ 0 ];
			if (( connection->sub[ 0 ] )) {
				cn_release( e ); // remove proxy
				cn_release( connection ); } } }
	else {
#ifdef UNIFIED
		// Assumption: e->sub[1] is not NULL
		e->sub[1] = NULL;
#endif
		cn_release( e );
		// deregister e from db->index
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
				break; }
			else last_i = i; } }
}

//===========================================================================
//	db_update
//===========================================================================
#define TRASH( x, parent ) \
	if (( x->sub[0] )) { \
		if (( x->sub[1] )) addItem( &trash[0], x ); \
		else if ( x!=parent ) addItem( &trash[1], x ); } \
	else addItem( &trash[1], x );

void
db_update( CNDB *db, CNInstance *parent )
/*
   cf design/specs/db-update.txt
*/
{
	CNInstance *nil = db->nil;
	if (( nil->sub[0] )) // remove init condition
		nil->sub[ 0 ] = NULL;

	CNInstance *f, *g, *x;
	listItem *trash[ 2 ] = { NULL, NULL };
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
			db_remove( h, db ); // ((nil,.),nil)
			db_remove( g, db );
			cn_new( nil, f ); } // handled below
		else if (( g->as_sub[ 1 ] )) { // to-be-manifested
			if (( f )) // released to-be-manifested
				db_remove( f, db ); }
		else if (( f ) && ( f->as_sub[ 0 ] )) { // reassigned
			db_remove( f->as_sub[0]->ptr, db );
			db_remove( f, db ); }
		else db_remove( g, db ); } // possibly to-be-released
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
			// to-be-released handled below
			db_remove( g, db ); }
		else { // just newborn
			db_remove( g, db );
			db_remove( f, db );
			cn_new( nil, x ); } }
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
			db_remove( g, db ); } }
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if ( !f->as_sub[1] ) {
			x = f->sub[0]; // released candidate
			db_remove( f, db );
			TRASH( x, parent ) } }
	// empty trash
	for ( int i=0; i<2; i++ )
		while (( x = popListItem( &trash[i] ) ))
			db_remove( x, db );

#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	for ( listItem *i=nil->as_sub[ 1 ]; i!=NULL; i=i->next ) {
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) {
			db_remove( f->as_sub[1]->ptr, db ); } }
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
		return e; }

	return NULL;
}

