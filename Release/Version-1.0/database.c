#include <stdio.h>
#include <stdlib.h>

#include "database.h"
#include "db_private.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG

//===========================================================================
//	CNDB allocate / free
//===========================================================================
CNDB *
newCNDB( void )
{
	CNInstance *nil = cn_new( NULL, NULL );
	Registry *index = newRegistry( IndexedByName );
	return (CNDB *) newPair( nil, index );
}

static freeRegistryCB freename_CB;
void
freeCNDB( CNDB *db )
{
	/* delete all CNDB entities, proceeding top down
	*/
	listItem *stack = NULL;
	listItem *i = db->index->entries;
	while (( i )) {
		CNInstance *e;
		if (( stack ))
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
			cn_free( e );
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				e = i->ptr;
			}
			else i = NULL;
		}
	}
	/* then delete CNDB per se
	*/
	freeRegistry( db->index, freename_CB );
	cn_free( db->nil );
	freePair((Pair *) db );
}
static int
freename_CB( Registry *registry, Pair *entry )
{
	free( entry->name );
	return 1;
}

//===========================================================================
//	CNDB update
//===========================================================================
void
db_update( CNDB *db )
/*
   cf db_op()
	( nil, ( nil, x ))	x to be manifested
	( nil, ( x, nil ))	x to be released
	( nil, x )		x manifested (as of last frame)
	( x, nil )		x deprecated (as of last frame)
	(( x, nil ), nil )	x newborn
   combinations
	x can be both manifested AND to-be-released
	x can be both manifested AND newborn (reassigned)
	x can be both released AND to-be-manifested (reborn)
	x can be both newborn AND to-be-released

	x cannot be both manifested AND released
	x cannot be both released AND to-be-released
	x cannot be both newborn AND released
	x cannot be both newborn AND to-be-manifested
	x cannot be both to-be-released AND to-be-manifested
*/
{
	CNInstance *nil = db->nil;
	CNInstance *e, *f, *g, *x;
#ifdef DEBUG
fprintf( stderr, "db_update: 1. actualize past manifestations\n" );
#endif
	// for each g: ( nil, x ) where ( nil, g ) does not exist and
	//	x is neither ( ., nil ) nor ( nil, . )
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		g = i->ptr;
		if (( g->as_sub[ 1 ] )) continue; // to be manifested
		x = g->sub[ 1 ];
		if ( x->sub[ 0 ] == nil ) continue;
		if ( x->sub[ 1 ] == nil ) continue;
		f = cn_instance( x, nil );
		if (( f )) {
			if (( f->as_sub[ 0 ] )) { // reassigned
				// remove (( x, nil ), nil ) and ( x, nil )
				g = f->as_sub[ 0 ]->ptr;
				db_remove( g, db );
				db_remove( f, db );
			}
			else if (( f->as_sub[ 1 ] )) { // to be released
				// remove ( nil, x )
				db_remove( g, db );
				// remove ( nil, ( x, nil ))
				g = f->as_sub[ 1 ]->ptr;
				db_remove( g, db );
				// remove ( x, nil ) and x
				db_remove( f, db );
				if ( x->sub[ 0 ] == NULL ) {
					db_deregister( x, db );
				}
				db_remove( x, db );
			}
		}
		else {
			// remove ( nil, x )
			db_remove( g, db );
		}
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 2. actualize newborn entities\n" );
#endif
	// for each g: ( f, nil ) where f: ( x, nil )
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		g = i->ptr;
		f = g->sub[ 0 ];
		if ( f->sub[ 1 ] != nil ) continue;
		if (( next_i ) && ( next_i->ptr == f ))
			next_i = next_i->next;
		x = f->sub[ 0 ];
		if (( f->as_sub[ 1 ] )) { // to be released
			// remove (( x, nil ), nil )
			db_remove( g, db );
			// remove ( nil, ( x, nil ))
			g = f->as_sub[ 1 ]->ptr;
			db_remove( g, db );
			// remove ( x, nil ) and x
			db_remove( f, db );
			if ( x->sub[ 0 ] == NULL ) {
				db_deregister( x, db );
			}
			db_remove( x, db );
		}
		else {
			// remove (( x, nil ), nil ) and ( x, nil )
			db_remove( g, db );
			db_remove( f, db );
			// create ( nil, x )
			cn_new( nil, x );
		}
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 3. actualize to be manifested entities\n" );
#endif
	// for each f: ( nil, x ) where ( nil, f ) exists and possibly ( x, nil )
	for ( listItem *i=nil->as_sub[ 0 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
			// actualize to be manifested entity
			g = f->as_sub[ 1 ]->ptr;
			if (( next_i ) && ( next_i->ptr == g ))
				next_i = next_i->next;
			// remove ( nil, ( nil, x ) )
			db_remove( g, db );
			// remove ( x, nil ) if x was released
			g = cn_instance( f->sub[ 1 ], nil );
			if (( g )) db_remove( g, db );
		}
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 4. remove released entities\n" );
#endif
	// for each f: ( x, nil ) where ( nil, f ) does not exist
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) continue; // to be released
		x = f->sub[ 0 ];
		// remove ( x, nil ) and x
		db_remove( f, db );
		if ( x->sub[ 0 ] == NULL ) {
			db_deregister( x, db );
		}
		db_remove( x, db );
	}
#ifdef DEBUG
fprintf( stderr, "db_update: 5. actualize to be released entities\n" );
#endif
	// for each f: ( x, nil ) where ( nil, f ) exists
	for ( listItem *i=nil->as_sub[ 1 ], *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		f = i->ptr;
		if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
			// actualize to be released entity
			g = f->as_sub[ 1 ]->ptr;
			// remove ( nil, ( x, nil ) )
			db_remove( g, db );
		}
	}
}

//===========================================================================
//	CNDB operation
//===========================================================================
int
db_feel( char *expression, DBLogType type, CNDB *db )
{
	int privy;
	switch ( type ) {
	case DB_CONDITION:
		return db_traverse( expression, db, NULL, NULL );
	case DB_RELEASED:
		privy = 1;
		break;
	case DB_INSTANTIATED:
		privy = 0;
		break;
	}
	CNInstance *nil = db->nil;
#ifdef DEBUG
	if (( privy )) fprintf( stderr, "db_feel: (%s,) {\n", expression );
	else fprintf( stderr, "db_feel: (,%s) {\n", expression );
	for ( listItem *i=nil->as_sub[ privy ]; i!=NULL; i=i->next ) {
		CNInstance *g = i->ptr;
		if (( g->as_sub[ 0 ] ) || ( g->as_sub[ 1 ] )) continue;
		CNInstance *x = g->sub[ !privy ];
		if (( x->sub[ 0 ] == nil ) || ( x->sub[ 1 ] == nil ))
			continue;
		dbg_out( "db_feel:\t", g, NULL, db );
		if ( db_verify( privy, x, expression, db ) ) {
			fprintf( stderr, " - found\n" );
			fprintf( stderr, "db_feel: }\n" );
			return 1;
		}
		else fprintf( stderr, " - no match\n" );
	}
	fprintf( stderr, "db_feel: }\n" );
#else	// DEBUG
	for ( listItem *i=nil->as_sub[ privy ]; i!=NULL; i=i->next ) {
		CNInstance *g = i->ptr;
		if (( g->as_sub[ 0 ] ) || ( g->as_sub[ 1 ] )) continue;
		CNInstance *x = g->sub[ !privy ];
		if (( x->sub[ 0 ] == nil ) || ( x->sub[ 1 ] == nil ))
			continue;
		if ( db_verify( privy, x, expression, db ) )
			return 1;
	}
#endif	// DEBUG
	return 0;
}
CNInstance *
db_register( char *identifier, CNDB *db )
{
	if ( identifier == NULL ) return NULL;
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, identifier );
	if (( entry )) {
		e = entry->value;
		db_op( DB_REHABILITATE_OP, e, 0, db );
	}
	else {
		identifier = strdup( identifier );
		e = cn_new( NULL, NULL );
		registryRegister( db->index, identifier, e );
		db_op( DB_MANIFEST_OP, e, DB_NEW, db );
	}
	return e;
}
listItem *
db_couple( listItem *sub[2], CNDB *db )
/*
	Instantiates and returns all ( sub[0], sub[1] )
	Manifests only those newly created or rehabilitated
	Assumption: no entity in either sub[] is deprecated
*/
{
	listItem *results = NULL, *s = NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *t = NULL;
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
				addIfNotThere( &results, db_instantiate(e,f,db) );
		}
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, db_instantiate(e,j->ptr,db) );
		}
	}
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, db_instantiate(i->ptr,f,db) );
	}
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, db_instantiate(i->ptr,j->ptr,db) );
	}
	return results;
}
void
db_deprecate( CNInstance *x, CNDB *db )
/*
	deprecates x and all its ascendants, proceeding top-down
	assumption: x is not deprecated
*/
{
	if ( x == NULL ) return;

	union { int value; void *ptr; } icast;
	listItem *stack = NULL;

	int	position = 0;

	listItem *i = newItem( x ), *j;
	for ( ; ; ) {
		x = i->ptr;
		for ( j=x->as_sub[ position ]; j!=NULL; j=j->next )
			if ( !db_deprecated( j->ptr, db ) ) break;
		if (( j )) {
			if ( position == 0 ) {
				icast.value = position;
				addItem( &stack, i );
				addItem( &stack, icast.ptr );
			}
			i = j;
			position = 0;
			continue;
		}
		for ( ; ; ) {
			if (( x )) {
				db_op( DB_DEPRECATE_OP, x, 0, db );
			}
			if (( i->next )) {
				i = i->next;
				if ( !db_deprecated( i->ptr, db ) ) {
					break;
				}
				else x = NULL;
			}
			else if (( stack )) {
				int x_position = (int) stack->ptr;
				if ( x_position == 0 ) {
					icast.value = 1;
					stack->ptr = icast.ptr;
					listItem *k = stack->next->ptr;
					CNInstance *sup = k->ptr;
					for ( j = sup->as_sub[ 1 ]; j!=NULL; j=j->next )
						if ( !db_deprecated( j->ptr, db ) ) break;
					if (( j )) {
						i = j;
						position = 0;
						break;
					}
					else x = NULL;
				}
				else {
					position = (int) popListItem( &stack );
					i = popListItem( &stack );
					x = i->ptr;
				}
			}
			else {
				freeItem( i );
				return;
			}
		}
	}
}

//===========================================================================
//	CNDB query
//===========================================================================
CNInstance *
db_lookup( int privy, char *identifier, CNDB *db )
{
	if ( identifier == NULL ) return NULL;
	Pair *entry = registryLookup( db->index, identifier );
	return (( entry ) && !db_private( privy, entry->value, db )) ?
		entry->value : NULL;
}
int
db_private( int privy, CNInstance *e, CNDB *db )
/*
	tests e against ( nil, . ) and ( ., nil )
	returns 1 if passes, otherwise:
		tests new born = ((e,nil),nil)
			in which case tests manifested = ( nil, e )
		otherwise tests to be released = (nil,(e,nil))
			in which case returns 0
		otherwise tests deprecated = (e,nil)
			in which case returns !privy
	Note: privy is set only by db_feel, in release log traversal mode
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[ 0 ] == nil ) return 1;
	if ( e->sub[ 1 ] == nil ) return 1;
	CNInstance *f = cn_instance( e, nil );
	if (( f )) {
		if ( f->as_sub[ 0 ] ) // new born
			return !( cn_instance( nil, e ) );
		if ( f->as_sub[ 1 ] ) // to be released
			return 0;
		// deprecated
		return !privy;
	}
	return 0;
}

//===========================================================================
//	CNDB private
//===========================================================================
static CNInstance *
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
	cn_out( stderr, e, db );
	dbg_out( ", ", f, " )\n", db );
#endif
	CNInstance *instance = NULL;
	Pair *entry = registryLookup( db->index, "*" );
	if (( entry ) && ( e->sub[ 0 ] == entry->value )) {
		/* Assignment case
		*/
		CNInstance *reassignment = NULL;
		// for each instance: ( e:( *, . ), . )
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			instance = i->ptr;
			if ( instance->sub[ 1 ] == f ) {
				db_op( DB_REHABILITATE_OP, instance, DB_REASSIGN, db );
				reassignment = instance;
			}
			else if ( !db_deprecated( instance, db ) )
				db_deprecate( instance, db );
		}
		if (( reassignment )) return reassignment;
	}
	else {
		/* General case
		*/
		instance = cn_instance( e, f );
		if (( instance )) {
			db_op( DB_REHABILITATE_OP, instance, 0, db );
			return instance;
		}
	}
	instance = cn_new( e, f );
	db_op( DB_MANIFEST_OP, instance, DB_NEW, db );
	return instance;
}
static char *
db_identifier( CNInstance *e, CNDB *db )
{
	if ( e == NULL ) return NULL;
	Pair *entry = registryLookup( db->index, NULL, e );
	if (( entry )) return entry->name;
	return NULL;
}
static void
db_deregister( CNInstance *e, CNDB *db )
/*
	removes e from db index if it's there
	Note: db_remove MUST be called, but is called separately
*/
{
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
			return;
		}
	}
}
static void
db_remove( CNInstance *e, CNDB *db )
/*
	Removes e from db
	Assumption: db_deregister is called, when needed, separately
*/
{
#ifdef DEBUG
dbg_out( "db_remove:\t", e, "\n", db );
#endif
	/* nullify e as sub of its as_sub's
	   note: these will be free'd as well, as e was deprecated
	*/
	for ( int i=0; i<2; i++ ) {
		for ( listItem *j=e->as_sub[i]; j!=NULL; j=j->next ) {
			CNInstance *e = j->ptr;
			e->sub[i] = NULL;
		}
	}
	/* remove e from the as_sub lists of its subs
	*/
	for ( int i=0; i<2; i++ ) {
		CNInstance *sub = e->sub[ i ];
		if (( sub )) removeItem( &sub->as_sub[ i ], e );
	}
	cn_free( e );
}
static CNInstance *
db_op( int op, CNInstance *e, int flag, CNDB *db )
{
	CNInstance *nil = db->nil, *f;
	switch ( op ) {
	case DB_DEPRECATE_OP:
		f = cn_instance( e, nil );
		if (( f )) {
			// case newborn, possibly to-be-released or manifested (reassigned)
			if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
					CNInstance *g = f->as_sub[ 1 ]->ptr;
					return g; // already to-be-released
				}
				// case manifested
				if (( cn_instance( nil, e ) )) {
					// remove (( e, nil ), nil )
					CNInstance *g = f->as_sub[ 0 ]->ptr;
					db_remove( g, db );
				}
				// create ( nil, ( e, nil )) (to be released)
				return cn_new( nil, f );
			}
			// case to-be-released (=> cannot be deprecated or to-be-manifested)
			else if (( f->as_sub[ 1 ] )) {
				CNInstance *g = f->as_sub[ 1 ]->ptr;
				return g; // already to-be-released
			}
			// case released, possibly to-be-manifested (reborn)
			CNInstance *k = cn_instance( nil, e );
			if (( k )) {
				// cannot be released and manifested
				CNInstance *g = k->as_sub[ 1 ]->ptr;
				db_remove( g, db );
				db_remove( k, db );
			}
			return f;
		}
		/* neither released, nor newborn, nor to-be-released
		   possibly manifested or to-be-manifested
		*/
		f = cn_instance( nil, e );
		if (( f ) && ( f->as_sub[ 1 ] )) { // can only be ( nil, f )
			CNInstance *g = f->as_sub[ 1 ]->ptr;
			db_remove( g, db );
			db_remove( f, db );
		}
		return cn_new( nil, cn_new( e, nil ) );

	case DB_REHABILITATE_OP:
		f = cn_instance( e, nil );
		if (( f )) {
			// case newborn, possibly to-be-released or manifested (reassigned)
			if (( f->as_sub[ 0 ] )) { // can only be ( f, nil )
				if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
					CNInstance *g = f->as_sub[ 1 ]->ptr;
					db_remove( g, db );
				}
				CNInstance *g = f->as_sub[ 0 ]->ptr;
				return g;
			}
			// case to-be-released (=> cannot be deprecated or to-be-manifested)
			else if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
				CNInstance *g = f->as_sub[ 1 ]->ptr;
				db_remove( g, db );
				db_remove( f, db );
			}
			// case released, possibly to-be-manifested (aka. reborn)
			else flag = DB_REASSIGN; // cannot be released AND manifested
		}
		// case reassignment, possibly manifested or to-be-manifested
		if ( flag == DB_REASSIGN ) {
			f = cn_instance( nil, e );
			if (( f )) {
				if (( f->as_sub[ 1 ] )) { // can only be ( nil, f )
					CNInstance *g = f->as_sub[ 1 ]->ptr;
					return g; // already to-be-manifested
				}
				// already manifested - create newborn (reassigned)
				return cn_new( cn_new( e, nil ), nil );
			}
			// create ( nil, ( nil, e )) (to be manifested)
			return cn_new( nil, cn_new( nil, e ) );
		}
		// general case: manifest only if deprecated
		return e;

	case DB_MANIFEST_OP:
		/* Assumption: flag == DB_NEW
		   create (( e, nil ), nil ) = newborn
		*/
		return cn_new( cn_new( e, nil ), nil );
	}
	return NULL;
}
static int
db_deprecated( CNInstance *e, CNDB *db )
/*
	tests e against ( nil, . ) and ( ., nil )
	returns 1 if passes, otherwise:
		tests newborn or reassigned - ((e,nil),nil)
			in which case returns 0
		tests to be released = (nil,(e,nil))
			in which case returns 0
		otherwise tests deprecated = (e,nil)
			in which case returns 1
*/
{
	CNInstance *nil = db->nil;
	if ( e->sub[ 0 ] == nil ) return 1;
	if ( e->sub[ 1 ] == nil ) return 1;
	CNInstance *f = cn_instance( e, nil );
	if (( f )) {
		if ( f->as_sub[ 0 ] )	// newborn or reassigned
			return 0;
		if ( f->as_sub[ 1 ] )	// to be released
			return 0;
		return 1;		// deprecated
	}
	return 0;
}

//===========================================================================
//	CNDB core
//===========================================================================
static CNInstance *
cn_new( CNInstance *source, CNInstance *target )
{
	Pair *sub = newPair( source, target );
	Pair *as_sub = newPair( NULL, NULL );
	CNInstance *e = (CNInstance *) newPair( sub, as_sub );
	if (( source )) addItem( &source->as_sub[0], e );
	if (( target )) addItem( &target->as_sub[1], e );
	return e;
}
static void
cn_free( CNInstance *e )
{
	if ( e == NULL ) return;
	freePair((Pair *) e->sub );
	freeListItem( &e->as_sub[ 0 ] );
	freeListItem( &e->as_sub[ 1 ] );
	freePair((Pair *) e->as_sub );
	freePair((Pair *) e );
}
static CNInstance *
cn_instance( CNInstance *e, CNInstance *f )
{
	for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
		CNInstance *instance = i->ptr;
		if ( instance->sub[ 1 ] == f )
			return instance;
	}
	return NULL;
}
int
cn_out( FILE *stream, CNInstance *e, CNDB *db )
{
	if ( e == NULL ) return 0;
	union { int value; void *ptr; } icast;
	struct {
		listItem *ndx;
		listItem *e;
	} stack = { NULL, NULL };
	int ndx = 0;
	for ( ; ; ) {
		if (( e->sub[ ndx ] )) {
			if ( ndx ) fprintf( stream, "," );
			else {
				fprintf( stream, "(" );
				icast.value = ndx;
				addItem( &stack.ndx, icast.ptr );
				addItem( &stack.e, e );
			}
			e = e->sub[ ndx ];
			ndx = 0;
			continue;
		}
		char *name = db_identifier( e, db );
		if (( name )) fprintf( stream, "%s", name );
		for ( ; ; ) {
			if (( stack.ndx )) {
				int parent_ndx = (int) stack.ndx->ptr;
				if ( parent_ndx ) {
					ndx = (int) popListItem( &stack.ndx );
					e = popListItem( &stack.e );
					fprintf( stream, ")" );
				}
				else {
					icast.value = ndx = 1;
					stack.ndx->ptr = icast.ptr;
					e = stack.e->ptr;
					break;
				}
			}
			else return 0;
		}
	}
	return 0;
}
void
dbg_out( char *pre, CNInstance *e, char *post, CNDB *db )
{
	if (( pre )) fprintf( stderr, "%s", pre );
	if (( e )) cn_out( stderr, e, db );
	if (( post )) fprintf( stderr, "%s", post );
}
