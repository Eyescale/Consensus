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
#ifdef NULL_TERMINATED
	Registry *index = newRegistry( IndexedByName );
#else
	Registry *index = newRegistry( IndexedByCharacter );
#endif
	CNInstance *nil = cn_new( NULL, NULL );
	CNDB *db = (CNDB *) newPair( nil, index );
	// optimize db_star( db )
	nil->sub[ 1 ] = db_register( "*", db );
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
	cn_prune( db->nil );
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
//	db_proxy
//===========================================================================
CNInstance *
db_proxy( CNEntity *this, CNEntity *that, CNDB *db )
{
	CNInstance *proxy = NULL;
	if (( that )) {
		proxy = cn_new( cn_new( this, that ), NULL );
		if (( this )) db_op( DB_MANIFEST_OP, proxy, db ); }
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
	if ( e->sub[ 0 ]==db_star(db) ) {
		/* Assignment case - as we have e:( *, . )
		*/
		// ward off concurrent reassignment case
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( db_to_be_manifested( candidate, db ) ) {
				db_outputf( stderr, db,
					"B%%::Warning: ((*,%_),%_) -> %_ concurrent reassignment unauthorized\n",
					e->sub[1], candidate->sub[1], f );
				if ( candidate->sub[ 1 ]==f )
					return candidate;
				else return NULL; } }
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
	variable = db_instantiate( db_star(db), variable, db );
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
	CNInstance *star = db_star( db );
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
	if ( x==NULL || x==db_star(db) )
		return;

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
		for ( ; ; ) {
			if ( ndx==0 ) { ndx=1; break; }
			if ((x)) db_op( DB_DEPRECATE_OP, x, db );
			if (( i->next )) {
				i = i->next;
				if ( !db_deprecatable( i->ptr, db ) )
					x = NULL;
				else { ndx=0; break; } }
			else if (( stack )) {
				ndx = pop_item( &stack );
				i = popListItem( &stack );
				if ( ndx ) x = i->ptr; }
			else goto RETURN;
		} }
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
//	db_identifier
//===========================================================================
#ifndef UNIFIED
char *
db_identifier( CNInstance *e, CNDB *db )
/*
	Assumption: e->sub[0]==e->sub[1]==NULL
*/
{
	Pair *entry = registryLookup( db->index, NULL, e );
	return ( entry ) ? entry->name : NULL;
}
#endif	// UNIFIED

//===========================================================================
//	db_first, db_next, db_traverse
//===========================================================================
CNInstance *
db_first( CNDB *db, listItem **stack )
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
db_next( CNDB *db, CNInstance *e, listItem **stack )
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
	type is either 's' or '_', the only difference being that,
	in the former case, we insert a backslash at the start of
	non-base entity expressions
*/
{
	if ( e == NULL ) return 0;
	if ( type=='s' ) {
		if (( e->sub[ 0 ] ) && ( e->sub[ 1 ] ))
			fprintf( stream, "\\" );
		else if ( !e->sub[0] ) {
			fprintf( stream, "%s", db_identifier(e,db) );
			return 0; }
		else {
			fprintf( stream, "@@@" ); // proxy
			return 0; } }

	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(e,ndx) )) {
			fprintf( stream, ndx==0 ? "(" : "," );
			add_item( &stack, ndx );
			addItem( &stack, e );
			e = e->sub[ ndx ];
			ndx=0; continue; }
		if (( e->sub[ 0 ] )) {
fprintf( stderr, "BINGO\n" );
			fprintf( stream, "@@@" ); } // proxy
		else {
			char *p = db_identifier( e, db );
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

