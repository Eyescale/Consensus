#include <stdio.h>
#include <stdlib.h>

#include "database.h"

// #define DEBUG
// #define DB_CACHE

static inline int dbc_op( DBOperation, CNInstance *, CNDB *);
static inline void dbc_update( CNDB * ); // cf design/specs/db-update.txt
static inline DBType dbc_type( CNInstance *, CNInstance *, CNInstance *, CNDB * );

//===========================================================================
//	db_op
//===========================================================================
static inline int db_manifest_op( DBOperation, CNInstance *, CNDB * );

int
db_op( DBOperation op, CNInstance *e, CNDB *db ) {
#ifdef DB_CACHE
	return dbc_op( op, e, db );
#endif
	CNInstance *nil = db->nil, *f, *g, *perso;
	switch ( op ) {
	case DB_MANIFEST_OP:
	case DB_MANIFEST_LOCALE_OP:
		return db_manifest_op( op, e, db );
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

static inline int
db_manifest_op( DBOperation op, CNInstance *e, CNDB *db ) {
	CNInstance *nil = db->nil;
	if ( op==DB_MANIFEST_OP )
		cn_new( cn_new( e, nil ), nil );
	else {
		CNInstance *perso = e->sub[ 0 ];
		CNInstance *f = cn_instance( perso, nil, 1 );
		CNInstance *g = cn_instance( nil, perso, 0 );
		switch ( dbc_type( perso, f, g, db ) ) {
		case DB_NEWBORN:
		case DB_MANIFESTED:
		case DB_MANIFESTED_TO_BE_MANIFESTED:
		case DB_TO_BE_MANIFESTED:
		case DB_DEFAULT:
			cn_new( cn_new( e, nil ), nil );
			break;
		default: // take whatever type perso has
		if (( f )) {
			CNInstance *y = cn_new( e, nil );
			if (( f->as_sub[0] )) cn_new( y, nil );
			if (( f->as_sub[1] )) cn_new( nil, y ); }
		if (( g )) {
			CNInstance *y = cn_new( nil, e );
			if (( g->as_sub[0] )) cn_new( y, nil );
			if (( g->as_sub[1] )) cn_new( nil, y ); } } }
	return 0; }

//===========================================================================
//	db_update
//===========================================================================
void
db_update( CNDB *db ) {
#ifdef DB_CACHE
	return dbc_update( db );
#endif
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
	listItem *released[2] = { NULL, NULL };
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) { // to-be-released
			cn_release( f->as_sub[1]->ptr ); }
		else {
			x = f->sub[0]; // release candidate
			addItem( &released[isBase(x)], x ); // reordered
			cn_release( f ); } }
#ifdef DEBUG
fprintf( stderr, "db_update: 5. remove released entities\n" );
#endif
	while (( x=popListItem( &released[0] ) )) cn_release( x );
	while (( x=popListItem( &released[1] ) )) {
		if ( cnIsShared(x) ) db_arena_deregister( x, db );
		else if ( cnIsProxy(x) ) free_proxy( x, db );
		else db_deregister( x, db ); }
#ifdef DEBUG
fprintf( stderr, "--\n" );
#endif
	}

//===========================================================================
//	db_type
//===========================================================================
DBType
db_type( CNInstance *e, CNDB *db ) {
	return dbc_type( NULL, e, NULL, db ); }

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

//===========================================================================
//	dbc_type
//===========================================================================
#ifdef DEBUG
#define CHALLENGE( condition ) \
	if ( condition ) mismatch = 1;
#else
#define CHALLENGE( condition )
#endif

static inline DBType
dbc_type( CNInstance *e, CNInstance *f, CNInstance *g, CNDB *db )
/*
   Assumption:
   if e: NULL, then g: NULL and f is the target entity
   otherwise we have
	f: cn_instance( e, nil, 1 )
	g: cn_instance( nil, e, 0 )
*/ {
#ifdef DEBUG
	int mismatch = 0;
#endif
	if ( !e ) {
		CNInstance *nil = db->nil;
		if ( f->sub[0]==nil || f->sub[1]==nil )
			return DB_PRIVATE;
		e = f;
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 ); }
	DBType type;
	if ((f)) {
		if ((f->as_sub[0])) {
			if ((f->as_sub[1])) {
				CHALLENGE( (g) );
				type = DB_NEWBORN_TO_BE_RELEASED; }
			else if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = DB_MANIFESTED_TO_BE_MANIFESTED; }
			else type = DB_NEWBORN; }
		else if ((f->as_sub[1])) {
			if ((g)) {
				CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
				type = DB_MANIFESTED_TO_BE_RELEASED; }
			else type = DB_TO_BE_RELEASED; }
		else if ((g)) {
			if ((g->as_sub[1])) {
				CHALLENGE( (g->as_sub[0]) )
				type = DB_RELEASED_TO_BE_MANIFESTED; }
			else {
				CHALLENGE( !g->as_sub[0] )
				type = DB_RELEASED_TO_BE_RELEASED; } }
		else type = DB_RELEASED; }
	else if ((g)) {
		CHALLENGE( (g->as_sub[0]) )
		if ((g->as_sub[1])) {
			type = DB_TO_BE_MANIFESTED; }
		else type = DB_MANIFESTED; }
	else type = DB_DEFAULT;
#ifdef DEBUG
	if ( mismatch ) {
		db_outputf( db, stderr, "B%%:: Error: dbc_type mismatch, type=%d, e=%_\n", type, e );
		exit( -1 ); }
#endif
	return type; }

//===========================================================================
//	dbc_op
//===========================================================================
static inline int
dbc_op( DBOperation op, CNInstance *e, CNDB *db )
/*
	Assumptions
	1. op==DB_MANIFEST_OP - e is newly created
	2. op==DB_DEPRECATE_OP - e is not released

*/ {
	CNInstance *f, *g;
	CNInstance *nil = db->nil;
	switch ( op ) {
	case DB_MANIFEST_OP:
	case DB_MANIFEST_LOCALE_OP:
		return db_manifest_op( op, e, db );

	case DB_DEPRECATE_OP: // objective: either released or to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbc_type(e, f, g, db ) ) {
		case DB_NEWBORN:
			cn_new( nil, f );
			return DB_NEWBORN_TO_BE_RELEASED;
		case DB_NEWBORN_TO_BE_RELEASED:	// deadborn: return as-is
			return DB_NEWBORN_TO_BE_RELEASED;
		case DB_RELEASED:
			cn_new( cn_new( nil, e ), nil );
			return DB_RELEASED_TO_BE_RELEASED;
		case DB_RELEASED_TO_BE_RELEASED: // return as-is
			return DB_RELEASED_TO_BE_RELEASED;
		case DB_RELEASED_TO_BE_MANIFESTED: // rehabilitated
			cn_release( g->as_sub[1]->ptr );
			cn_new( g, nil );
			return DB_RELEASED_TO_BE_RELEASED;
		case DB_MANIFESTED:
			cn_new( nil, cn_new( e, nil ));
			return DB_MANIFESTED_TO_BE_RELEASED;
		case DB_MANIFESTED_TO_BE_MANIFESTED: // reassigned
			cn_release( f->as_sub[0]->ptr );
			cn_new( nil, f );
			return DB_MANIFESTED_TO_BE_RELEASED;
		case DB_MANIFESTED_TO_BE_RELEASED: // return as-is
			return DB_MANIFESTED_TO_BE_RELEASED;
		case DB_TO_BE_MANIFESTED: // return TO_BE_RELEASED
			cn_release( g->as_sub[1]->ptr );
			cn_release( g );
			cn_new( nil, cn_new( e, nil ));
			return DB_TO_BE_RELEASED;
		case DB_TO_BE_RELEASED:	 // return as-is
			return DB_TO_BE_RELEASED;
		case DB_DEFAULT: // return TO_BE_RELEASED
			cn_new( nil, cn_new( e, nil ));
			return DB_TO_BE_RELEASED;
		case DB_PRIVATE:
			return DB_PRIVATE; }
		break;

	case DB_REHABILITATE_OP: // objective: neither released nor to-be-released
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbc_type( e, f, g, db ) ) {
		case DB_RELEASED: // return RELEASED_TO_BE_MANIFESTED (rehabilitated)
			cn_new( nil, cn_new( nil, e ));
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_RELEASED:	// remove to-be-released condition
			cn_release( f->as_sub[1]->ptr );
			cn_release( f );
			return DB_DEFAULT;
		case DB_RELEASED_TO_BE_RELEASED:
			cn_release( g->as_sub[0]->ptr );
			cn_new( nil, g );
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED
			cn_release( f->as_sub[1]->ptr );
			cn_release( f );
			return DB_MANIFESTED;
		case DB_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			cn_release( f->as_sub[1]->ptr );
			return DB_NEWBORN;
		case DB_NEWBORN: // return as-is
			return DB_NEWBORN;
		case DB_MANIFESTED:
			return DB_MANIFESTED;
		case DB_MANIFESTED_TO_BE_MANIFESTED:
			return DB_MANIFESTED_TO_BE_MANIFESTED;
		case DB_RELEASED_TO_BE_MANIFESTED:
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_MANIFESTED:
			return DB_TO_BE_MANIFESTED;
		case DB_DEFAULT:
			return DB_DEFAULT;
		case DB_PRIVATE:
			return DB_PRIVATE; }
		break;

	case DB_REASSIGN_OP: // objective: reassigned, newborn or to-be-manifested
		f = cn_instance( e, nil, 1 );
		g = cn_instance( nil, e, 0 );
		switch ( dbc_type( e, f, g, db ) ) {
		case DB_RELEASED: // return RELEASED_TO_BE_MANIFESTED
			cn_new( nil, cn_new( nil, e ));
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_RELEASED:	// return TO_BE_MANIFESTED
			cn_release( f->as_sub[1]->ptr );
			cn_release( f );
			cn_new( nil, cn_new( nil, e ));
			return DB_TO_BE_MANIFESTED;
		case DB_RELEASED_TO_BE_RELEASED:
			cn_release( g->as_sub[0]->ptr );
			cn_new( nil, g );
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_MANIFESTED_TO_BE_RELEASED:
			cn_release( f->as_sub[1]->ptr );
			cn_new( f, nil );
			return DB_MANIFESTED_TO_BE_MANIFESTED;
		case DB_MANIFESTED:
			cn_new( cn_new( e, nil ), nil );
			return DB_MANIFESTED_TO_BE_MANIFESTED;
		case DB_NEWBORN_TO_BE_RELEASED:	// return NEWBORN
			cn_release( f->as_sub[1]->ptr );
			return DB_NEWBORN;
		case DB_NEWBORN: // return as-is
			return DB_NEWBORN;
		case DB_MANIFESTED_TO_BE_MANIFESTED:
			return DB_MANIFESTED_TO_BE_MANIFESTED;
		case DB_RELEASED_TO_BE_MANIFESTED:
			return DB_RELEASED_TO_BE_MANIFESTED;
		case DB_TO_BE_MANIFESTED:
			return DB_TO_BE_MANIFESTED;
		case DB_DEFAULT: // return TO_BE_MANIFESTED
			cn_new( nil, cn_new( nil, e ));
			return DB_TO_BE_MANIFESTED;
		case DB_PRIVATE:
			return DB_PRIVATE; } }
	return 0; }

//===========================================================================
//	dbc_update
//===========================================================================
static inline void cache_inform( CNDB *db, listItem **log );
#define DBUpdateBegin( type ) \
	while (( pair = popListItem(&log[type]) )) { \
		e = pair->name; \
		f = ((Pair*) pair->value)->name; \
		g = ((Pair*) pair->value)->value; \
		freePair( pair->value ); \
		freePair( pair );
#define DBUpdateEnd }

static inline void
dbc_update( CNDB *db )
/*
	cf design/specs/db-update.txt
*/ {
#ifdef DEBUG
	fprintf( stderr, "db_update_debug: bgn\n" );
#endif
	Pair *pair;
	CNInstance *e, *f, *g;
	CNInstance *nil = db->nil;
	listItem *released[2] = { NULL, NULL };
	listItem *log[ DB_DEFAULT ];
	memset( log, 0, sizeof(log) );

	/* cache log, as: {{ [ e, [ f:(e,nil), g:(nil,e) ] ] }}
	*/
	cache_inform( db, log );

#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize manifested entities\n" );
#endif
	/* transform manifested into default or manifested
	*/
	DBUpdateBegin( DB_MANIFESTED );
		cn_release( g );
	DBUpdateEnd
	DBUpdateBegin( DB_MANIFESTED_TO_BE_MANIFESTED );
		cn_release( f->as_sub[0]->ptr );
		cn_release( f );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 2. actualize newborn entities\n" );
#endif
	/* transform newborn into manifested or released
	*/
	DBUpdateBegin( DB_NEWBORN );
		cn_release( f->as_sub[0]->ptr );
		cn_release( f );
		cn_new( nil, e );
	DBUpdateEnd
	DBUpdateBegin( DB_NEWBORN_TO_BE_RELEASED );
		cn_release( f->as_sub[1]->ptr );
		cn_release( f->as_sub[0]->ptr );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	/* transform to-be-manifested into manifested
	*/
	DBUpdateBegin( DB_TO_BE_MANIFESTED );
		cn_release( g->as_sub[1]->ptr );
	DBUpdateEnd
	DBUpdateBegin( DB_RELEASED_TO_BE_MANIFESTED );
		cn_release( f );
		cn_release( g->as_sub[1]->ptr );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 4. actualize to be released entities\n" );
#endif
	/* transform to-be-released into released
	*/
	DBUpdateBegin( DB_TO_BE_RELEASED );
		cn_release( f->as_sub[1]->ptr );
	DBUpdateEnd
        DBUpdateBegin( DB_RELEASED_TO_BE_RELEASED )
		cn_release( g->as_sub[0]->ptr );
		cn_release( g );
	DBUpdateEnd
        DBUpdateBegin( DB_MANIFESTED_TO_BE_RELEASED )
		cn_release( f->as_sub[1]->ptr );
		cn_release( g );
	DBUpdateEnd
#ifdef DEBUG
fprintf( stderr, "db_update: 5. remove released entities\n" );
#endif
	/* remove released
	*/
	DBUpdateBegin( DB_RELEASED );
		cn_release( f );
		addItem( &released[isBase(e)], e );
	DBUpdateEnd
	while (( e=popListItem( &released[0] ) )) cn_release( e );
	while (( e=popListItem( &released[1] ) )) {
		if ( cnIsShared(e) ) db_arena_deregister( e, db );
		else if ( cnIsProxy(e) ) free_proxy( e, db );
		else db_deregister( e, db ); }
#ifdef DEBUG
	fprintf( stderr, "db_update_debug: end\n" );
#endif
}

//---------------------------------------------------------------------------
//	cache_inform
//---------------------------------------------------------------------------
static inline void
cache_inform( CNDB *db, listItem **log )
/*
	cache log, as: {{ [ e, [ f:(e,nil), g:(nil,e) ] ] }}
*/ {
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
		addItem( &log[ dbc_type(e,f,g,db) ], newPair( e, pair )); }
	f = NULL;
	for ( listItem *i=nil->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		g = i->ptr;
		e = g->sub[ 1 ];
		if ( e->sub[0]==nil || e->sub[1]==nil )
			continue;
		if (( cn_instance( e, nil, 1 ) )) // already done
			continue;
		pair = newPair( f, g );
		addItem( &log[ dbc_type(e,f,g,db) ], newPair( e, pair )); } }

