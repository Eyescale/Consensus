#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

// #define DEBUG

//===========================================================================
//	cn_type
//===========================================================================
CNType
cn_type( CNInstance *e, CNInstance *f, CNInstance *g, CNDB *db )
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
			return CN_PRIVATE;
		e = f;
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
	}
	int type;
	if ((f)) {
		if ((f->as_sub[0])) {
			if ((f->as_sub[1])) {
				CHALLENGE( (g) );
				type = CN_NEWBORN_TO_BE_RELEASED;
			}
			else if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = CN_MANIFESTED_REASSIGNED;
			}
			else type = CN_NEWBORN;
		}
		else if ((f->as_sub[1])) {
			if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = CN_MANIFESTED_TO_BE_RELEASED;
			}
			else type = CN_TO_BE_RELEASED;
		}
		else if ((g)) {
			CHALLENGE( (g->as_sub[0]) || !(g->as_sub[1]) )
			type = CN_RELEASED_TO_BE_MANIFESTED;
		}
		else type = CN_RELEASED;
	}
	else if ((g)) {
		CHALLENGE( (g->as_sub[0]) )
		if ((g->as_sub[1]))
			type = CN_TO_BE_MANIFESTED;
		else {
			type = CN_MANIFESTED;
		}
	}
	else type = CN_DEFAULT;
#ifdef DEBUG
	if ( mismatch ) {
		fprintf( stderr, "B%%:: db_debug: Error: cn_type mismatch: %d - e=", type );
		db_output( stderr, "", e, db );
		fprintf( stderr, "\n" );
		exit( -1 );
	}
#endif
	return type;
}

//===========================================================================
//	db_op_debug
//===========================================================================
int
db_op_debug( DBOperation op, CNInstance *e, CNDB *db )
/*
	Assumptions
	1. op==DB_MANIFEST_OP - e is newly created
	2. op==DB_DEPRECATE_OP - e is not released

*/
{
#if 0
	fprintf( stderr, "db_op: bgn\n" );
#endif
	CNInstance *f, *g;
	CNInstance *nil = db->nil;
	switch ( op ) {
	case DB_EXIT_OP:
		if ( db_out(db) ) return -1;
		cn_new( nil, nil );
		break;

	case DB_MANIFEST_OP: // assumption: the entity was just created
		cn_new( cn_new( e, nil ), nil );
		break;

	case DB_DEPRECATE_OP: // objective: either released or to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( cn_type(e, f, g, db ) ) {
		case CN_NEWBORN: // return NEWBORN_TO_BE_RELEASED (deadborn)
			cn_new( nil, f );
			return CN_NEWBORN_TO_BE_RELEASED;
		case CN_NEWBORN_TO_BE_RELEASED:	// deadborn: return as-is
			return CN_NEWBORN_TO_BE_RELEASED;
		case CN_RELEASED: // return as-is
			return CN_RELEASED;
		case CN_RELEASED_TO_BE_MANIFESTED: // rehabilitated: return RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			return CN_RELEASED;
		case CN_MANIFESTED: // return MANIFESTED_TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			return CN_MANIFESTED_TO_BE_RELEASED;
		case CN_MANIFESTED_REASSIGNED: // reassigned: return TO_BE_RELEASED
			db_remove( g, db );
			db_remove( f->as_sub[0]->ptr, db );
			cn_new( nil, f );
			return CN_TO_BE_RELEASED;
		case CN_MANIFESTED_TO_BE_RELEASED: // return as-is
			return CN_MANIFESTED_TO_BE_RELEASED;
		case CN_TO_BE_MANIFESTED: // return TO_BE_RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			cn_new( nil, cn_new( e, nil ));
			return CN_TO_BE_RELEASED;
		case CN_TO_BE_RELEASED:	 // return as-is
			return CN_TO_BE_RELEASED;
		case CN_DEFAULT: // return TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			return CN_TO_BE_RELEASED;
		case CN_PRIVATE:
			return CN_PRIVATE;
		}
		break;

	case DB_REHABILITATE_OP: // objective: neither released nor to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( cn_type( e, f, g, db ) ) {
		case CN_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			return CN_RELEASED_TO_BE_MANIFESTED;
		case CN_TO_BE_RELEASED:	// remove to-be-released condition
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			return CN_DEFAULT;
		case CN_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			return CN_MANIFESTED;
		case CN_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			db_remove( f->as_sub[1]->ptr, db );
			return CN_NEWBORN;
		case CN_NEWBORN: // return as-is
			return CN_NEWBORN;
		case CN_MANIFESTED:
			return CN_MANIFESTED;
		case CN_MANIFESTED_REASSIGNED:
			return CN_MANIFESTED_REASSIGNED;
		case CN_RELEASED_TO_BE_MANIFESTED:
			return CN_RELEASED_TO_BE_MANIFESTED;
		case CN_TO_BE_MANIFESTED:
			return CN_TO_BE_MANIFESTED;
		case CN_DEFAULT:
			return CN_DEFAULT;
		case CN_PRIVATE:
			return CN_PRIVATE;
		}
		break;

	case DB_REASSIGN_OP: // objective: reassigned, newborn or to-be-manifested
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( cn_type( e, f, g, db ) ) {
		case CN_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			return CN_RELEASED_TO_BE_MANIFESTED;
		case CN_TO_BE_RELEASED:	// return TO_BE_MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			cn_new( nil, cn_new( nil, e ));
			return CN_TO_BE_MANIFESTED;
		case CN_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED_REASSIGNED
			db_remove( f->as_sub[1]->ptr, db );
			cn_new( f, nil );
			return CN_MANIFESTED_REASSIGNED;
		case CN_MANIFESTED: // return MANIFESTED_REASSIGNED (reassigned)
			cn_new( cn_new( e, nil ), nil );
			return CN_MANIFESTED_REASSIGNED;
		case CN_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			db_remove( f->as_sub[1]->ptr, db );
			return CN_NEWBORN;
		case CN_NEWBORN: // return as-is
			return CN_NEWBORN;
		case CN_MANIFESTED_REASSIGNED:
			return CN_MANIFESTED_REASSIGNED;
		case CN_RELEASED_TO_BE_MANIFESTED:
			return CN_RELEASED_TO_BE_MANIFESTED;
		case CN_TO_BE_MANIFESTED:
			return CN_TO_BE_MANIFESTED;
		case CN_DEFAULT: // return TO_BE_MANIFESTED
			cn_new( nil, cn_new( nil, e ));
			return CN_TO_BE_MANIFESTED;
		case CN_PRIVATE:
			return CN_PRIVATE;
		}
		break;
	}
#if 0
	fprintf( stderr, "db_op: end\n" );
#endif
	return 0;
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

void
db_update_debug( CNDB *db )
/*
   cf design/specs/db-update.txt
*/
{
	Pair *pair;
	CNInstance *e, *f, *g;
	CNInstance *nil = db->nil;
#ifdef DEBUG
	fprintf( stderr, "db_update: bgn\n" );
#endif
	/* cache log, as: {{ [ e, [ f:(e,nil), g:(nil,e) ] ] }}
	*/
	listItem *log[ CN_DEFAULT ];
	memset( log, 0, sizeof(log) );
	db_cache_log( db, log );
#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	/* transform manifested into default or manifested
	*/
	DBUpdateBegin( CN_MANIFESTED );
		db_remove( g, db );
	DBUpdateEnd
	DBUpdateBegin( CN_MANIFESTED_REASSIGNED );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 2. actualize newborn entities\n" );
#endif
	/* transform newborn into manifested or remove them
	*/
	DBUpdateBegin( CN_NEWBORN );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		cn_new( nil, e );
	DBUpdateEnd
	DBUpdateBegin( CN_NEWBORN_TO_BE_RELEASED );
		db_remove( f->as_sub[1]->ptr, db );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	/* transform to-be-manifested into manifested
	*/
	DBUpdateBegin( CN_TO_BE_MANIFESTED );
		db_remove( g->as_sub[1]->ptr, db );
	DBUpdateEnd
	DBUpdateBegin( CN_RELEASED_TO_BE_MANIFESTED );
		db_remove( f, db );
		db_remove( g->as_sub[1]->ptr, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	/* remove released
	*/
	DBUpdateBegin( CN_RELEASED );
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	/* transform to-be-released into released
	*/
	DBUpdateBegin( CN_TO_BE_RELEASED );
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
        DBUpdateBegin( CN_MANIFESTED_TO_BE_RELEASED )
		db_remove( g, db );
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
#ifdef DEBUG
	fprintf( stderr, "db_update: end\n" );
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
		if ( e==nil || e->sub[0]==nil || e->sub[1]==nil )
			continue;
		g = cn_instance( nil, e, 0 );
		pair = newPair( f, g );
		addItem( &log[ cn_type(e,f,g,db) ], newPair( e, pair ));
	}
	f = NULL;
	for ( listItem *i=nil->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		g = i->ptr;
		e = g->sub[ 1 ];
		if ( e==nil || e->sub[0]==nil || e->sub[1]==nil )
			continue;
		if (( cn_instance( e, nil, 1 ) )) // already done
			continue;
		pair = newPair( f, g );
		addItem( &log[ cn_type(e,f,g,db) ], newPair( e, pair ));
	}
}


//===========================================================================
//	test_instantiate
//===========================================================================
CNType
test_instantiate( char *src, CNInstance *e, CNDB *db )
{
	int type = cn_type( NULL, e, NULL, db );
	switch( type ) {
	case CN_PRIVATE:
	case CN_RELEASED:
	case CN_NEWBORN_TO_BE_RELEASED:
		fprintf( stderr, ">>>>>> B%%: Error: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case CN_TO_BE_RELEASED: // user conflict
	case CN_MANIFESTED_TO_BE_RELEASED: // user conflict
		fprintf( stderr, ">>>>>> B%%: Warning: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case CN_NEWBORN:
	case CN_MANIFESTED:
	case CN_MANIFESTED_REASSIGNED:
	case CN_TO_BE_MANIFESTED:
	case CN_RELEASED_TO_BE_MANIFESTED:
	case CN_DEFAULT:
		break;
	}
	return type;
}

//===========================================================================
//	test_deprecatable
//===========================================================================
CNType
test_deprecate( char *src, CNInstance *e, CNDB *db )
{
	int type = cn_type( NULL, e, NULL, db );
	switch( type ) {
	case CN_PRIVATE:
	case CN_RELEASED:
	case CN_NEWBORN:
	case CN_NEWBORN_TO_BE_RELEASED:
		fprintf( stderr, ">>>>>> B%%: Error: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d", type );
		break;
	case CN_TO_BE_RELEASED: // redundant
	case CN_MANIFESTED_TO_BE_RELEASED: // redundant
	case CN_RELEASED_TO_BE_MANIFESTED: // user conflict
		fprintf( stderr, ">>>>>> B%%: Warning: %s: ", src );
		db_output( stderr, "", e, db );
		fprintf( stderr, " - type=%d\n", type );
		break;
	case CN_MANIFESTED:
	case CN_MANIFESTED_REASSIGNED:
	case CN_TO_BE_MANIFESTED:
	case CN_DEFAULT:
		break;
	}
	return type;
}
