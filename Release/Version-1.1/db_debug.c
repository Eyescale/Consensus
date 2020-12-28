#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

//===========================================================================
//	db_op
//===========================================================================
typedef enum {
	CN_NEWBORN = 0,
	CN_NEWBORN_TO_BE_RELEASED,
	CN_MANIFESTED,
	CN_MANIFESTED_TO_BE_RELEASED,
	CN_MANIFESTED_REASSIGNED,
	CN_RELEASED,
	CN_RELEASED_TO_BE_MANIFESTED,
	CN_TO_BE_MANIFESTED,
	CN_TO_BE_RELEASED,
	CN_DEFAULT // keep last
} CNType;
static CNType cn_type( CNInstance *e, CNInstance *f, CNInstance *g );

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
		g = cn_instance( nil, e, 0 );
		switch ( cn_type(e, f, g ) ) {
		case CN_NEWBORN: // return NEWBORN_TO_BE_RELEASED (deadborn)
			cn_new( nil, cn_new( e, nil ));
			break;
		case CN_NEWBORN_TO_BE_RELEASED:	// deadborn: return as-is
		case CN_RELEASED: // return as-is
			break;
		case CN_RELEASED_TO_BE_MANIFESTED: // rehabilitated: return RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			break;
		case CN_MANIFESTED: // return MANIFESTED_TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			break;
		case CN_MANIFESTED_REASSIGNED: // reassigned: return TO_BE_RELEASED
			db_remove( f->as_sub[0]->ptr, db );
			db_remove( g, db );
			cn_new( nil, cn_new( e, nil ));
			break;
		case CN_MANIFESTED_TO_BE_RELEASED: // return as-is
			break;
		case CN_TO_BE_MANIFESTED: // return TO_BE_RELEASED
			db_remove( g->as_sub[1]->ptr, db );
			db_remove( g, db );
			cn_new( nil, cn_new( e, nil ));
			break;
		case CN_TO_BE_RELEASED:	 // return as-is
			break;
		case CN_DEFAULT: // return TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			break;
		}
		break;
	case DB_REASSIGN_OP:
	case DB_REHABILITATE_OP:
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( cn_type( e, f, g ) ) {
		case CN_NEWBORN:		// return as-is
			break;
		case CN_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			break;
		case CN_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			break;
		case CN_MANIFESTED: // return as-is resp. MANIFESTED_REASSIGNED (reassigned)
			if ( op == DB_REASSIGN_OP ) {
				cn_new( cn_new( e, nil ), nil );
			}
			break;
		case CN_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED resp. MANIFESTED_REASSIGNED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			if ( op == DB_REASSIGN_OP ) {
				cn_new( cn_new( e, nil ), nil );
			}
			break;
		case CN_MANIFESTED_REASSIGNED:	// reassigned: return as-is
		case CN_RELEASED_TO_BE_MANIFESTED:	// rehabilitated: return as-is
		case CN_TO_BE_MANIFESTED:		// return as-is
			break;
		case CN_TO_BE_RELEASED:	// remove condition resp. return TO_BE_MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			if ( op == DB_REASSIGN_OP ) {
				cn_new( nil, cn_new( nil, e ));
			}
			break;
		case CN_DEFAULT: // return as-is resp. TO_BE_MANIFESTED
			if ( op == DB_REASSIGN_OP ) {
				cn_new( nil, cn_new( nil, e ));
			}
			break;
		}
		break;

	case DB_MANIFEST_OP: // assumption: the entity was just created
		cn_new( cn_new( e, nil ), nil );
		break;
	}
}

static CNType
cn_type( CNInstance *e, CNInstance *f, CNInstance *g )
/*
   Assumption:
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
		fprintf( stderr, "B%%:: Error: cn_type mismatch: %d\n", type );
		exit( -1 );
	}
#endif
	return type;
}

//===========================================================================
//	db_update
//===========================================================================
static void db_cache_log( CNDB *db, listItem **log );
static void db_log_update( listItem **log, int type, CNDB * );

void
db_update( CNDB *db )
/*
   cf design/specs/db-update.txt
*/
{
	listItem *log[ CN_DEFAULT ];
	memset( log, 0, sizeof(log) );
	db_cache_log( db, log );

	/* update manifested
	*/
	db_log_update( log, CN_MANIFESTED, db );
	db_log_update( log, CN_MANIFESTED_REASSIGNED, db );

	/* update newborn
	*/
	db_log_update( log, CN_NEWBORN_TO_BE_RELEASED, db );
	db_log_update( log, CN_NEWBORN, db );

	/* update to-be-manifested
	*/
	db_log_update( log, CN_RELEASED_TO_BE_MANIFESTED, db );
	db_log_update( log, CN_TO_BE_MANIFESTED, db );

	/* update released
	*/
	db_log_update( log, CN_RELEASED, db );

	/* update to-be-released
	*/
	db_log_update( log, CN_MANIFESTED_TO_BE_RELEASED, db );
	db_log_update( log, CN_TO_BE_RELEASED, db );
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
		addItem( &log[ cn_type(e,f,g) ], newPair( e, pair ));
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
		addItem( &log[ cn_type(e,f,g) ], newPair( e, pair ));
	}
}
static void
db_log_update( listItem **log, int type, CNDB *db )
{
	Pair *pair;
	CNInstance *e, *f, *g;
	CNInstance *nil = db->nil;
#define DBUpdateBegin( type ) \
	while (( pair = popListItem(&log[type]) )) { \
		e = pair->name; \
		f = ((Pair*) pair->value)->name; \
		g = ((Pair*) pair->value)->value; \
		freePair( pair->value ); \
		freePair( pair );
#define DBUpdateEnd \
	}
        DBUpdateBegin( type )
	switch ( type ) {
	case CN_MANIFESTED:
		db_remove( g, db );
		break;
	case CN_MANIFESTED_REASSIGNED:
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		break;
	case CN_NEWBORN:
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		cn_new( nil, e );
		break;
	case CN_NEWBORN_TO_BE_RELEASED:
		db_remove( f->as_sub[1]->ptr, db );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
		break;
        case CN_TO_BE_MANIFESTED:
		db_remove( g->as_sub[1]->ptr, db );
		break;
	case CN_RELEASED_TO_BE_MANIFESTED:
		db_remove( g->as_sub[1]->ptr, db );
		db_remove( f, db );
		break;
        case CN_RELEASED:
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
		break;
        case CN_TO_BE_RELEASED:
		db_remove( f->as_sub[1]->ptr, db );
		break;
        case CN_MANIFESTED_TO_BE_RELEASED:
		db_remove( g, db );
		db_remove( f->as_sub[1]->ptr, db );
		break;
	}
	DBUpdateEnd
}

