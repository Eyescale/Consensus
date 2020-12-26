#ifdef DB_DEBUG_C

#ifdef DEBUG
#define DB_DEBUG(mark) db_debug(db,mark)
#else
#define DB_DEBUG(mark)
#endif

//===========================================================================
//	db_debug
//===========================================================================
static int debug_CB( CNInstance *, CNDB *, void * );

static int
db_debug( CNDB *db, char *mark )
{
	if ( !db_traverse( 0, db, debug_CB, NULL ) )
		return 0;
FOUND:
	fprintf( stderr, "******** DB_DEBUG: CONDITION MATCHED: %s ******\n", mark );
	exit( -1 );
}
static int
debug_CB( CNInstance *e, CNDB *db, void *user_data )
{
	if ( e == NULL ) {
		fprintf( stderr, "BOGUS\n" );
		exit( -1 );
	}
	return ( e->sub[0]==NULL && e->sub[1]!=NULL );
}

//===========================================================================
//	db_op
//===========================================================================
static void db_remove( CNInstance *e, CNDB *db );
typedef enum {
	CN_NEWBORN,
	CN_NEWBORN_TO_BE_RELEASED,
	CN_MANIFESTED,
	CN_MANIFESTED_TO_BE_RELEASED,
	CN_MANIFESTED_TO_BE_MANIFESTED,
	CN_RELEASED,
	CN_RELEASED_TO_BE_MANIFESTED,
	CN_TO_BE_MANIFESTED,
	CN_TO_BE_RELEASED,
	CN_DEFAULT
} CNType;
static CNType cn_type( CNInstance *e, CNInstance *f, CNInstance *g );

void
db_op( DBOperation op, CNInstance *e, CNDB *db )
/*
	Assumptions
	. DB_MANIFEST_OP is requested only when e is newly created
	. e is not internal - i.e. we have neither e:(nil,.) nor e:(.,nil)
*/
{
DB_DEBUG( "db_op: before" );
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
		case CN_MANIFESTED_TO_BE_MANIFESTED: // reassigned: return TO_BE_RELEASED
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
		case CN_MANIFESTED: // return as-is resp. MANIFESTED_TO_BE_MANIFESTED (reassigned)
			if ( op == DB_REASSIGN_OP ) {
				cn_new( cn_new( e, nil ), nil );
			}
			break;
		case CN_MANIFESTED_TO_BE_RELEASED: // return MANIFESTED resp. MANIFESTED_TO_BE_MANIFESTED
			db_remove( f->as_sub[1]->ptr, db );
			db_remove( f, db );
			if ( op == DB_REASSIGN_OP ) {
				cn_new( cn_new( e, nil ), nil );
			}
			break;
		case CN_MANIFESTED_TO_BE_MANIFESTED:	// reassigned: return as-is
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
DB_DEBUG( "db_op: after" );
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
				type = CN_MANIFESTED_TO_BE_MANIFESTED;
			}
			else type = CN_NEWBORN;
		}
		else if ((f->as_sub[1])) {
			CHALLENGE( (g->as_sub[0]) || (g->as_sub[1]) )
			if ((g)) type = CN_MANIFESTED_TO_BE_RELEASED;
			else type = CN_TO_BE_RELEASED;
		}
		else if ((g)) {
			CHALLENGE( (g->as_sub[0]) || !(g->as_sub[1]) )
			type = CN_RELEASED_TO_BE_MANIFESTED;
		}
		else type = CN_RELEASED;
	}
	else if ((g)) {
		if ((g->as_sub[1])) {
			CHALLENGE( (g->as_sub[0]) )
			type = CN_TO_BE_MANIFESTED;
		}
		else {
			CHALLENGE( (g->as_sub[0]) )
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
static void db_deregister( CNInstance *e, CNDB *db );

void
db_update( CNDB *db )
{
DB_DEBUG( "db_update: pre" );
	CNInstance *nil = db->nil;
	listItem *cache[ CN_DEFAULT ];
	memset( cache, 0, sizeof(cache) );
	Pair *pair;
	CNInstance *e, *f, *g;
	for ( listItem *i=nil->as_sub[ 1 ]; i!=NULL; i=i->next ) {
		CNInstance *f = i->ptr;
		CNInstance *e = f->sub[ 0 ];
		if ( e==nil || e->sub[0]==nil || e->sub[1]==nil )
			continue;
		CNInstance *g = cn_instance( nil, e, 0 );
		pair = newPair( f, g );
		addItem( &cache[ cn_type(e,f,g) ], newPair( e, pair ));
	}
	f = NULL;
	for ( listItem *i=nil->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		CNInstance *g = i->ptr;
		CNInstance *e = g->sub[ 1 ];
		if ( e==nil || e->sub[0]==nil || e->sub[1]==nil )
			continue;
		if (( cn_instance( e, nil, 1 ) )) // already done
			continue;
		pair = newPair( f, g );
		addItem( &cache[ cn_type(e,f,g) ], newPair( e, pair ));
	}
#define DBUpdateBegin( type ) \
	while (( pair = popListItem(&cache[type]) )) { \
		e = pair->name; \
		f = ((Pair*) pair->value)->name; \
		g = ((Pair*) pair->value)->value; \
		freePair( pair->value ); \
		freePair( pair );
#define DBUpdateEnd \
	}
DB_DEBUG( "db_update: 1. actualize last frame's manifestations" );
        DBUpdateBegin( CN_MANIFESTED )
		db_remove( g, db );
	DBUpdateEnd
        DBUpdateBegin( CN_MANIFESTED_TO_BE_MANIFESTED )
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
	DBUpdateEnd
DB_DEBUG( "db_update: 2. actualize newborn entities" );
	DBUpdateBegin( CN_NEWBORN )
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		cn_new( nil, e );
	DBUpdateEnd
	DBUpdateBegin( CN_NEWBORN_TO_BE_RELEASED )
		db_remove( f->as_sub[1]->ptr, db );
		db_remove( f->as_sub[0]->ptr, db );
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
	DBUpdateEnd
DB_DEBUG( "db_update: 3. actualize to be manifested entities" );
        DBUpdateBegin( CN_TO_BE_MANIFESTED )
		db_remove( g->as_sub[1]->ptr, db );
	DBUpdateEnd
        DBUpdateBegin( CN_RELEASED_TO_BE_MANIFESTED )
		db_remove( g->as_sub[1]->ptr, db );
		db_remove( f, db );
	DBUpdateEnd
DB_DEBUG( "db_update: 4. remove released entities" );
        DBUpdateBegin( CN_RELEASED )
		db_remove( f, db );
		if ( e->sub[0]==NULL )
			db_deregister( e, db );
		db_remove( e, db );
	DBUpdateEnd
#ifdef DEBUG
DB_DEBUG( "db_update: 5. actualize to be released entities" );
#endif
        DBUpdateBegin( CN_TO_BE_RELEASED )
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
        DBUpdateBegin( CN_MANIFESTED_TO_BE_RELEASED )
		db_remove( g, db );
		db_remove( f->as_sub[1]->ptr, db );
	DBUpdateEnd
#ifdef DEBUG
DB_DEBUG( "db_update: post" );
#endif
}

#endif // DB_DEBUG_C
