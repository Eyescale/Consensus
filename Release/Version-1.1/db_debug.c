#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"
#include "db_debug.h"

// #define DEBUG

extern void db_remove( CNInstance *, CNDB * );

//===========================================================================
//	db_op_debug
//===========================================================================
static DBGType dbgType( CNInstance *e, CNInstance *f, CNInstance *g, CNDB *db );

int
db_op_debug( DBOperation op, CNInstance *e, CNDB *db )
/*
	Assumptions
	1. op==DB_MANIFEST_OP - e is newly created
	2. op==DB_DEPRECATE_OP - e is not released

*/
{
	CNInstance *f, *g;
	CNInstance *nil = db->nil;
	switch ( op ) {
	case DB_EXIT_OP:
		nil->sub[ 1 ] = nil;
		break;

	case DB_MANIFEST_OP: // assumption: the entity was just created
		cn_new( cn_new( e, nil ), nil );
		break;

	case DB_DEPRECATE_OP: // objective: either released or to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbgType(e, f, g, db ) ) {
		case DB_NEWBORN: // return NEWBORN_TO_BE_RELEASED (deadborn)
			cn_new( nil, f );
			return DB_NEWBORN_TO_BE_RELEASED;
		case DB_NEWBORN_TO_BE_RELEASED:	// deadborn: return as-is
			return DB_NEWBORN_TO_BE_RELEASED;
		case DB_RELEASED: // return as-is
			return DB_RELEASED;
		case DB_RELEASED_TO_BE_MANIFESTED: // rehabilitated: return RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			return DB_RELEASED;
		case DB_MANIFESTED: // return MANIFESTED_TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			return DB_MANIFESTED_TO_BE_RELEASED;
		case DB_MANIFESTED_REASSIGNED: // reassigned: return TO_BE_RELEASED
			db_remove( g, db );
			db_remove( f->as_sub[0]->ptr, db );
			cn_new( nil, f );
			return DB_TO_BE_RELEASED;
		case DB_MANIFESTED_TO_BE_RELEASED: // return as-is
			return DB_MANIFESTED_TO_BE_RELEASED;
		case DB_TO_BE_MANIFESTED: // return TO_BE_RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			cn_new( nil, cn_new( e, nil ));
			return DB_TO_BE_RELEASED;
		case DB_TO_BE_RELEASED:	 // return as-is
			return DB_TO_BE_RELEASED;
		case DB_DEFAULT: // return TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			return DB_TO_BE_RELEASED;
		case DB_PRIVATE:
			return DB_PRIVATE;
		}
		break;

	case DB_REHABILITATE_OP: // objective: neither released nor to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbgType( e, f, g, db ) ) {
		case DB_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_RELEASED:	// remove to-be-released condition
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			return DB_DEFAULT;
		case DB_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			return DB_MANIFESTED;
		case DB_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			db_remove( f->as_sub[1]->ptr, db );
			return DB_NEWBORN;
		case DB_NEWBORN: // return as-is
			return DB_NEWBORN;
		case DB_MANIFESTED:
			return DB_MANIFESTED;
		case DB_MANIFESTED_REASSIGNED:
			return DB_MANIFESTED_REASSIGNED;
		case DB_RELEASED_TO_BE_MANIFESTED:
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_MANIFESTED:
			return DB_TO_BE_MANIFESTED;
		case DB_DEFAULT:
			return DB_DEFAULT;
		case DB_PRIVATE:
			return DB_PRIVATE;
		}
		break;

	case DB_REASSIGN_OP: // objective: reassigned, newborn or to-be-manifested
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbgType( e, f, g, db ) ) {
		case DB_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_RELEASED:	// return TO_BE_MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			cn_new( nil, cn_new( nil, e ));
			return DB_TO_BE_MANIFESTED;
		case DB_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED_REASSIGNED
			db_remove( f->as_sub[1]->ptr, db );
			cn_new( f, nil );
			return DB_MANIFESTED_REASSIGNED;
		case DB_MANIFESTED: // return MANIFESTED_REASSIGNED (reassigned)
			cn_new( cn_new( e, nil ), nil );
			return DB_MANIFESTED_REASSIGNED;
		case DB_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			db_remove( f->as_sub[1]->ptr, db );
			return DB_NEWBORN;
		case DB_NEWBORN: // return as-is
			return DB_NEWBORN;
		case DB_MANIFESTED_REASSIGNED:
			return DB_MANIFESTED_REASSIGNED;
		case DB_RELEASED_TO_BE_MANIFESTED:
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_MANIFESTED:
			return DB_TO_BE_MANIFESTED;
		case DB_DEFAULT: // return TO_BE_MANIFESTED
			cn_new( nil, cn_new( nil, e ));
			return DB_TO_BE_MANIFESTED;
		case DB_PRIVATE:
			return DB_PRIVATE;
		}
		break;
	}
	return 0;
}

static DBGType
dbgType( CNInstance *e, CNInstance *f, CNInstance *g, CNDB *db )
/*
   Assumption:
   if e == NULL, then g == NULL and f is the target entity
   otherwise
	f == cn_instance( e, nil, 1 );
	g == cn_instance( nil, e, 0 );
*/
{
#ifdef DEBUG
	int mismatch = 0;
#define CHALLENGE( condition ) \
	if ( condition ) mismatch = 1;
#else
#define CHALLENGE( condition )
#endif
	if ( e == NULL ) {
		CNInstance *nil = db->nil;
		if ( f->sub[0]==nil || f->sub[1]==nil )
			return DB_PRIVATE;
		e = f;
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
	}
	DBGType type;
	if ((f)) {
		if ((f->as_sub[0])) {
			if ((f->as_sub[1])) {
				CHALLENGE( (g) );
				type = DB_NEWBORN_TO_BE_RELEASED;
			}
			else if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = DB_MANIFESTED_REASSIGNED;
			}
			else type = DB_NEWBORN;
		}
		else if ((f->as_sub[1])) {
			if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = DB_MANIFESTED_TO_BE_RELEASED;
			}
			else type = DB_TO_BE_RELEASED;
		}
		else if ((g)) {
			CHALLENGE( (g->as_sub[0]) || !(g->as_sub[1]) )
			type = DB_RELEASED_TO_BE_MANIFESTED;
		}
		else type = DB_RELEASED;
	}
	else if ((g)) {
		CHALLENGE( (g->as_sub[0]) )
		if ((g->as_sub[1])) {
			type = DB_TO_BE_MANIFESTED;
		}
		else type = DB_MANIFESTED;
	}
	else type = DB_DEFAULT;
#ifdef DEBUG
	if ( mismatch ) {
		fprintf( stderr, "B%%:: db_debug: Error: dbgType mismatch: %d - e=", type );
		db_output( stderr, "", e, db );
		fprintf( stderr, "\n" );
		exit( -1 );
	}
#endif
	return type;
}

//===========================================================================
//	db_update_debug
//===========================================================================
static void db_cache_log( CNDB *db, listItem **log );

#define DBUpdateBegin( type ) \
	while (( pair = popListItem(&log[type]) )) { \
		e = pair->name; \
		f = ((Pair*) pair->value)->name; \
		g = ((Pair*) pair->value)->value; \
		freePair( pair->value ); \
		freePair( pair );
#define DBUpdateEnd \
	}

#define TRASH( x ) \
	if (( x->sub[0] )) addItem( &trash[0], x ); \
	else addItem( &trash[1], x );
#define EMPTY_TRASH \
	for ( int i=0; i<2; i++ ) \
		while (( e = popListItem( &trash[i] ) )) \
			db_remove( e, db );

void
db_update_debug( CNDB *db )
/*
   cf design/specs/db-update.txt
*/
{
	Pair *pair;
	CNInstance *e, *f, *g;
	listItem *trash[ 2 ] = { NULL, NULL };	
#ifdef DEBUG
	fprintf( stderr, "db_update_debug: bgn\n" );
#endif
	CNInstance *nil = db->nil;
	if ( nil->sub[ 0 ] == nil ) // remove init condition
		nil->sub[ 0 ] = NULL;

	/* cache log, as: {{ [ e, [ f:(e,nil), g:(nil,e) ] ] }}
	*/
	listItem *log[ DB_DEFAULT ];
	memset( log, 0, sizeof(log) );
	db_cache_log( db, log );

#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	/* transform manifested into default or manifested
	*/
	DBUpdateBegin( DB_MANIFESTED );
		db_remove( g, db );
	DBUpdateEnd
	DBUpdateBegin( DB_MANIFESTED_REASSIGNED );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 2. actualize newborn entities\n" );
#endif
	/* transform newborn into manifested or remove them
	*/
	DBUpdateBegin( DB_NEWBORN );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		cn_new( nil, e );
	DBUpdateEnd
	DBUpdateBegin( DB_NEWBORN_TO_BE_RELEASED );
		db_remove( f->as_sub[1]->ptr, db );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		TRASH( e );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	/* transform to-be-manifested into manifested
	*/
	DBUpdateBegin( DB_TO_BE_MANIFESTED );
		db_remove( g->as_sub[1]->ptr, db );
	DBUpdateEnd
	DBUpdateBegin( DB_RELEASED_TO_BE_MANIFESTED );
		db_remove( f, db );
		db_remove( g->as_sub[1]->ptr, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	/* remove released
	*/
	DBUpdateBegin( DB_RELEASED );
		db_remove( f, db );
		TRASH( e );
	DBUpdateEnd
	EMPTY_TRASH
#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	/* transform to-be-released into released
	*/
	DBUpdateBegin( DB_TO_BE_RELEASED );
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
        DBUpdateBegin( DB_MANIFESTED_TO_BE_RELEASED )
		db_remove( g, db );
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
#ifdef DEBUG
	fprintf( stderr, "db_update_debug: end\n" );
#endif
}
static void
db_cache_log( CNDB *db, listItem **log )
/*
	cache log, as: {{ [ e, [ f:(e,nil), g:(nil,e) ] ] }}
*/
{
	Pair *pair;
	CNInstance *e, *f, *g;
	CNInstance *nil = db->nil;
	for ( listItem *i=nil->as_sub[ 1 ]; i!=NULL; i=i->next ) {
		f = i->ptr;
		e = f->sub[ 0 ];
		if ( e->sub[0]==nil || e->sub[1]==nil )
			continue;
		g = cn_instance( nil, e, 0 );
		pair = newPair( f, g );
		addItem( &log[ dbgType(e,f,g,db) ], newPair( e, pair ));
	}
	f = NULL;
	for ( listItem *i=nil->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		g = i->ptr;
		e = g->sub[ 1 ];
		if ( e->sub[0]==nil || e->sub[1]==nil )
			continue;
		if (( cn_instance( e, nil, 1 ) )) // already done
			continue;
		pair = newPair( f, g );
		addItem( &log[ dbgType(e,f,g,db) ], newPair( e, pair ));
	}
}

//===========================================================================
//	test_instantiate
//===========================================================================
DBGType
test_instantiate( char *src, CNInstance *e, CNDB *db )
{
	DBGType type = dbgType( NULL, e, NULL, db );
	switch( type ) {
	case DB_PRIVATE:
	case DB_RELEASED:
	case DB_NEWBORN_TO_BE_RELEASED:
		fprintf( stderr, ">>>>>> B%%: Error: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case DB_TO_BE_RELEASED: // user conflict
	case DB_MANIFESTED_TO_BE_RELEASED: // user conflict
		fprintf( stderr, ">>>>>> B%%: Warning: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case DB_NEWBORN:
	case DB_MANIFESTED:
	case DB_MANIFESTED_REASSIGNED:
	case DB_TO_BE_MANIFESTED:
	case DB_RELEASED_TO_BE_MANIFESTED:
	case DB_DEFAULT:
		break;
	}
	return type;
}

//===========================================================================
//	test_deprecate
//===========================================================================
DBGType
test_deprecate( char *src, CNInstance *e, CNDB *db )
{
	DBGType type = dbgType( NULL, e, NULL, db );
	switch( type ) {
	case DB_PRIVATE:
	case DB_RELEASED:
	case DB_NEWBORN:
	case DB_NEWBORN_TO_BE_RELEASED:
		fprintf( stderr, ">>>>>> B%%: Error: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case DB_TO_BE_RELEASED: // redundant
	case DB_MANIFESTED_TO_BE_RELEASED: // redundant
	case DB_RELEASED_TO_BE_MANIFESTED: // user conflict
		fprintf( stderr, ">>>>>> B%%: Warning: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d\n", type );
		break;
	case DB_MANIFESTED:
	case DB_MANIFESTED_REASSIGNED:
	case DB_TO_BE_MANIFESTED:
	case DB_DEFAULT:
		break;
	}
	return type;
}
