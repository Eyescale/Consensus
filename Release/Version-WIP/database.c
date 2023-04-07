#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "string_util.h"
#include "database.h"

// #define DEBUG
// #define NULL_TERMINATED

//===========================================================================
//	newCNDB
//===========================================================================
CNDB *
newCNDB( void )
{
	CNInstance *nil = cn_new( NULL, NULL );
#ifdef NULL_TERMINATED
	Registry *index = newRegistry( IndexedByName );
#else
	Registry *index = newRegistry( IndexedByCharacter );
#endif
	CNDB *db = (CNDB *) newPair( nil, index );
	db_init( db );
	return db;
}

//===========================================================================
//	freeCNDB
//===========================================================================
static freeRegistryCB free_CB;

void
freeCNDB( CNDB *db )
/*
	delete all CNDB entities, proceeding top down
*/
{
	if ( db == NULL ) return;
	CNInstance *nil = db->nil;
	nil->sub[ 0 ] = NULL;
	nil->sub[ 1 ] = NULL;
	cn_prune( nil );
	freeRegistry( db->index, free_CB );
	freePair((Pair *) db );
}
static void
free_CB( Registry *registry, Pair *entry )
{
	free( entry->name );
	cn_prune((CNInstance *) entry->value );
}

//===========================================================================
//	db_register
//===========================================================================
CNInstance *
db_register( char *p, CNDB *db )
/*
	register instance represented by p
	manifest only if instance is new or rehabilitated
*/
{
	if ( p == NULL ) return NULL;
#ifdef NULL_TERMINATED
	p = strmake( p ); // for comparison
#endif
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, p );
	if (( entry )) {
		e = entry->value;
		db_op( DB_REHABILITATE_OP, e, db );
#ifdef NULL_TERMINATED
		free( p );
#endif
	}
	else {
#ifndef NULL_TERMINATED
		p = strmake( p ); // for storage
#endif
		e = cn_new( NULL, NULL );
#ifdef UNIFIED
		e->sub[ 1 ] = (CNInstance *) p;
#endif
		registryRegister( db->index, p, e );
		db_op( DB_MANIFEST_OP, e, db );
	}
	return e;
}

//===========================================================================
//	db_match
//===========================================================================
int
db_match( CNInstance *x, CNDB *db_x, CNInstance *y, CNDB *db_y )
/*
	Assumption: x!=NULL
	Note that we use y to lead the traversal - but works either way
*/
{
	if ( !y ) return 0;
	if ( db_x==db_y ) return ( x==y );
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(y,ndx) )) {
			if ( !CNSUB(x,ndx) )
				goto FAIL;
			add_item( &stack, ndx );
			addItem( &stack, y );
			addItem( &stack, x );
			y = y->sub[ ndx ];
			x = x->sub[ ndx ];
			ndx = 0; continue; }

		if (( y->sub[ 0 ] )) {
			if ( !isProxy(x) || DBProxyThat(x)!=DBProxyThat(y) )
				goto FAIL; }
		else {
			if (( x->sub[ 0 ] ))
				goto FAIL;
			char *p_x = DBIdentifier( x, db_x );
			char *p_y = DBIdentifier( y, db_y );
			if ( strcomp( p_x, p_y, 1 ) )
				goto FAIL; }
		for ( ; ; ) {
			if ( !stack ) return 1;
			x = popListItem( &stack );
			y = popListItem( &stack );
			if ( !pop_item( &stack ) )
				{ ndx=1; break; } } }
FAIL:
	freeListItem( &stack );
	return 0;
}

//===========================================================================
//	db_proxy
//===========================================================================
CNInstance *
db_proxy( CNEntity *this, CNEntity *that, CNDB *db )
/*
	Assumption: proxy not already created
*/
{
	CNInstance *proxy = NULL;
	if (( that )) {
		proxy = cn_new( cn_new( this, that ), NULL );
		if ( !this ) db_op( DB_MANIFEST_OP, proxy, db ); }
	return proxy;
}

//===========================================================================
//	db_instantiate
//===========================================================================
CNInstance *
db_instantiate( CNInstance *e, CNInstance *f, CNDB *db )
/*
	Assumption: neither e nor f are released
	Special case: assignment - i.e. e is ( *, variable )
		in which case we must deprecate current assignment
	Note that until next frame, ((*,e), . ) will exist both as
		the current (either to-be-released or reassigned) and
		the new (either newborn, reassigned, or released-to-be-manifested)
	assignments.
*/
{
#ifdef DEBUG
	db_outputf( stderr, db, "db_instantiate: ( %_, %_ )\n", e, f );
#endif
	CNInstance *instance = NULL;
	if ( DBStarMatch( e->sub[0], db ) ) {
		/* Assignment case - as we have e:( *, . )
		*/
		// ward off concurrent reassignment case
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( db_to_be_manifested( candidate, db ) ) {
				if ( candidate->sub[ 1 ]==f )
					return candidate;
				db_outputf( stderr, db,
					"B%%::Warning: ((*,%_),%_) -> %_ concurrent reassignment unauthorized\n",
					e->sub[1], candidate->sub[1], f );
				return NULL; } }
		// perform assignment
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( candidate->sub[ 1 ] == f )
				instance = candidate;
			else if ( db_deprecatable( candidate, db ) )
				db_deprecate( candidate, db ); }
		if (( instance )) {
			db_op( DB_REASSIGN_OP, instance, db );
			return instance; } }
	else {
		/* General case
		*/
		instance = cn_instance( e, f, 0 );
		if (( instance )) {
			db_op( DB_REHABILITATE_OP, instance, db );
			return instance; } }

	instance = cn_new( e, f );
	db_op( DB_MANIFEST_OP, instance, db );
	return instance;
}

//===========================================================================
//	db_assign
//===========================================================================
CNInstance *
db_assign( CNInstance *variable, CNInstance *value, CNDB *db )
{
	CNInstance *star = db_register( "*", db );
	variable = db_instantiate( star, variable, db );
	return db_instantiate( variable, value, db );
}

//===========================================================================
//	db_unassign
//===========================================================================
CNInstance *
db_unassign( CNInstance *x, CNDB *db )
/*
	deprecate ((*,x),.) and force manifest (*,x) if it exists
	otherwise instantiate (*,x)
*/
{
	CNInstance *star = db_register( "*", db );
	CNInstance *e;
	for ( listItem *i=x->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( e->sub[0]!=star )
			continue;
		for ( listItem *j=e->as_sub[0]; j!=NULL; j=j->next ) {
			CNInstance *f = j->ptr;
			if ( db_deprecatable(f,db) )
				db_deprecate( f, db ); }
		db_op( DB_REASSIGN_OP, e, db );
		return e; }
	e = cn_new( star, x );
	db_op( DB_MANIFEST_OP, e, db );
	return e;
}

//===========================================================================
//	db_deprecate
//===========================================================================
void
db_deprecate( CNInstance *x, CNDB *db )
/*
	Assumption: x is deprecatable
	deprecate (ie. set "to-be-released") x and all its ascendants,
	proceeding top-down
*/
{
	if ( !x ) return;

	listItem * stack = NULL,
		 * i = newItem( x ),
		 * j;
	int ndx = 0;
	for ( ; ; ) {
		x = i->ptr;
		for ( j=x->as_sub[ ndx ]; j!=NULL; j=j->next )
			if ( db_deprecatable( j->ptr, db ) )
				break;
		if (( j )) {
			addItem( &stack, i );
			add_item( &stack, ndx );
			ndx=0; i=j; continue; }

		if ( ndx ) db_op( DB_DEPRECATE_OP, x, db );
		else { ndx=1; continue; }
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				if ( db_deprecatable( i->ptr, db ) )
					{ ndx=0; break; } }
			else if (( stack )) {
				ndx = pop_item( &stack );
				i = popListItem( &stack );
				if ( ndx ) db_op( DB_DEPRECATE_OP, i->ptr, db );
				else { ndx=1; break; } }
			else goto RETURN; } }
RETURN:
	freeItem( i );
}

//===========================================================================
//	db_lookup
//===========================================================================
CNInstance *
db_lookup( int privy, char *p, CNDB *db )
{
	if ( !p ) return NULL;
#ifdef NULL_TERMINATED
	p = strmake( p );
	Pair *entry = registryLookup( db->index, p );
	free( p );
#else
	Pair *entry = registryLookup( db->index, p );
#endif
	return (( entry ) && !db_private( privy, entry->value, db )) ?
		entry->value : NULL;
}

//===========================================================================
//	DBIdentifier
//===========================================================================
#ifndef UNIFIED
char *
DBIdentifier( CNInstance *e, CNDB *db )
/*
	Assumption: e->sub[0]==e->sub[1]==NULL
*/
{
	Pair *entry = registryLookup( db->index, NULL, e );
	return ( entry ) ? entry->name : NULL;
}
int DBStarMatch( CNInstance *e, CNDB *db )
{
	if ( (e) && !e->sub[0] ) {
		char *identifier = DBIdentifier(e,db);
		if (( identifier )) return *identifier=='*'; }
	return 0;
}
#endif	// UNIFIED

//===========================================================================
//	db_traverse, DBFirst, DBNext
//===========================================================================
int
db_traverse( int privy, CNDB *db, DBTraverseCB user_CB, void *user_data )
/*
	traverses whole CNDB applying user_CB to each entity
*/
{
	listItem *stack = NULL;
	listItem *n = ( privy ? newItem( db->nil ) : NULL );
	listItem *i = ( privy ? n : db->index->entries );
	while (( i )) {
		CNInstance *e;
		if (( stack ) || (n))
			e = i->ptr;
		else {
			Pair *pair = i->ptr;
			e = pair->value; }
		listItem *j = e->as_sub[ 0 ];
		if (( j )) {
			addItem( &stack, i );
			i = j; continue; }
		while (( i )) {
			if ( user_CB( e, db, user_data ) ) {
				freeListItem( &n );
				freeListItem( &stack );
				return 1; }
			if (( i->next )) {
				i = i->next;
				break; }
			else if (( stack )) {
				i = popListItem( &stack );
				if ((stack) || (n))
					e = i->ptr;
				else {
					Pair *pair = i->ptr;
					e = pair->value; } }
			else if (( n )) {
				freeListItem( &n );
				i = db->index->entries;
				break; }
			else i = NULL; } }
	return 0;
}
CNInstance *
DBFirst( CNDB *db, listItem **stack )
{
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *e = entry->value;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e;
		}
	}
	return NULL;
}
CNInstance *
DBNext( CNDB *db, CNInstance *e, listItem **stack )
{
	if ( e == NULL ) return NULL;
	if (( e->as_sub[ 0 ] )) {
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			e = i->ptr;
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e; } } }
	listItem *i = popListItem( stack );
	for ( ; ; ) {
		if (( i->next )) {
			i = i->next;
			if (( *stack ))
				e = i->ptr;
			else {
				// e in db->index
				Pair *entry = i->ptr;
				e = entry->value; }
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e; } }
		else if (( *stack ))
			i = popListItem( stack );
		else return NULL; }
}

//===========================================================================
//	db_outputf
//===========================================================================
static int outputf( FILE *, CNDB *, int type, CNInstance * );

int
db_outputf( FILE *stream, CNDB *db, char *fmt, ... )
{
	CNInstance *e;
	va_list ap;
	va_start( ap, fmt );
	for ( char *p=fmt; *p; p++ )
		switch (*p) {
		case '%':
			switch ( p[1] ) {
			case '%':
				fprintf( stream, "%%" );
				break;
			case '_':
			case 's':
				e = va_arg( ap, CNInstance * );
				outputf( stream, db, p[1], e );
				break;
			default:
				; // unsupported
			}
			p++; break;
		default:
			fprintf( stream, "%c", *p );
		}
	va_end( ap );
	return 0;
}

static int
outputf( FILE *stream, CNDB *db, int type, CNInstance *e )
/*
	type is either 's' or '_', the main difference being that,
	in the former case, we insert a backslash at the start of
	non-base entity expressions
*/
{
	if ( e == NULL ) return 0;
	CNInstance *nil = db->nil;
	if ( type=='s' ) {
		if (( CNSUB(e,0) ))
			fprintf( stream, "\\" );
		else {
			if ( e==nil )
				fprintf( stream, "\\(nil)" );
			else if (( e->sub[0] ))
				fprintf( stream, "@@@" ); // proxy
			else
				fprintf( stream, "%s", DBIdentifier(e,db) );
			return 0; } }

	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if ( e==nil )
			fprintf( stream, "(nil)" );
		else if (( CNSUB(e,ndx) )) {
			fprintf( stream, ndx==0 ? "(" : "," );
			add_item( &stack, ndx );
			addItem( &stack, e );
			e = e->sub[ ndx ];
			ndx=0; continue; }
		else if (( e->sub[ 0 ] )) {
			fprintf( stream, "@@@" ); } // proxy
		else {
			char *p = DBIdentifier( e, db );
			if (( *p=='*' ) || ( *p=='%' ) || !is_separator(*p))
				fprintf( stream, "%s", p );
			else {
				switch (*p) {
				case '\0': fprintf( stream, "'\\0'" ); break;
				case '\t': fprintf( stream, "'\\t'" ); break;
				case '\n': fprintf( stream, "'\\n'" ); break;
				case '\'': fprintf( stream, "'\\''" ); break;
				case '\\': fprintf( stream, "'\\\\'" ); break;
				default:
					if ( is_printable(*p) ) fprintf( stream, "'%c'", *p );
					else fprintf( stream, "'\\x%.2X'", *(unsigned char *)p );
				} } }
		for ( ; ; ) {
			if (( stack )) {
				e = popListItem( &stack );
				if (( ndx = pop_item( &stack ) )) {
					fprintf( stream, ")" ); }
				else { ndx=1; break; } }
			else return 0; } }
}

