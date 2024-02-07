#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "string_util.h"
#include "database.h"
#include "db_op.h"

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
	Registry *index = newRegistry( IndexedByNameRef );
#endif
	char *p = strmake( "*" );
	CNInstance *star = cn_new( NULL, NULL );
#ifdef UNIFIED
	star->sub[ 1 ] = (CNInstance *) p;
#endif
	registryRegister( index, p, star );

	CNInstance *nil = cn_new( NULL, star );
	return (CNDB *) newPair( nil, index );
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
//	db_star
//===========================================================================
CNInstance *
db_star( CNDB *db )
/*
	Assumption: CNDB is freed upon exit, so that no attempt to access
	db->nil will be made after db_exit() - which overwrites nil->sub[1]
*/
{
	CNInstance *nil = db->nil;
	return nil->sub[ 1 ];
}

//===========================================================================
//	db_register
//===========================================================================
CNInstance *
db_register( char *p, CNDB *db, int manifest )
/*
	register instance represented by p
	manifest only if requested AND if instance is new or rehabilitated
*/
{
	if ( p == NULL ) return NULL;
#ifdef NULL_TERMINATED
	p = strmake( p );
#endif
	CNInstance *e = NULL;
	Pair *entry = registryLookup( db->index, p );
	if (( entry )) {
		e = entry->value;
		if ( manifest ) db_op( DB_REHABILITATE_OP, e, db );
#ifdef NULL_TERMINATED
		free( p );
#endif
	}
	else {
#ifndef NULL_TERMINATED
		p = strmake( p );
#endif
		e = cn_new( NULL, NULL );
#ifdef UNIFIED
		e->sub[ 1 ] = (CNInstance *) p;
#endif
		registryRegister( db->index, p, e );
		if ( manifest ) db_op( DB_MANIFEST_OP, e, db );
	}
	return e;
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
		/* Ward off concurrent reassignment case
		*/
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( db_to_be_manifested( candidate, db ) ) {
				db_outputf( stderr, db,
					"B%%::Warning: ((*,%_),%_) -> %_ concurrent reassignment unauthorized\n",
					e->sub[1], candidate->sub[1], f );
				if ( candidate->sub[ 1 ] == f )
					return candidate;
				else return NULL;
			}
		}
		/* Assignment case - as we have e:( *, . )
		*/
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *candidate = i->ptr;
			if ( candidate->sub[ 1 ] == f ) {
				instance = candidate;
			}
			else if ( db_deprecatable( candidate, db ) )
				db_deprecate( candidate, db );
		}
		if (( instance )) {
			db_op( DB_REASSIGN_OP, instance, db );
			return instance;
		}
	}
	else {
		/* General case
		*/
		instance = cn_instance( e, f, 0 );
		if (( instance )) {
			db_op( DB_REHABILITATE_OP, instance, db );
			return instance;
		}
	}
	instance = cn_new( e, f );
	db_op( DB_MANIFEST_OP, instance, db );
	return instance;
}

//===========================================================================
//	db_coupled
//===========================================================================
int
db_coupled( int privy, CNInstance *x, CNDB *db )
/*
	Assumption: invoked by bm_assign_op(), with x:(*,.)
*/
{
	for ( listItem *i=x->as_sub[0]; i!=NULL; i=i->next ) {
		CNInstance *e = i->ptr;
		if ( !db_private( privy, e, db ) )
			return 1;
	}
	return 0;
}

//===========================================================================
//	db_uncouple
//===========================================================================
void
db_uncouple( CNInstance *x, CNDB *db )
/*
	Assumption: invoked by bm_assign_op(), with x:(*,.)
	deprecate (x,.) and force manifest - aka. reassign - x
*/
{
	for ( listItem *i=x->as_sub[0]; i!=NULL; i=i->next ) {
		CNInstance *e = i->ptr;
		if ( db_deprecatable(e,db) )
			db_deprecate( e, db );
	}
	db_op( DB_REASSIGN_OP, x, db );
}

//===========================================================================
//	db_deprecate
//===========================================================================
void
db_deprecate( CNInstance *x, CNDB *db )
/*
	deprecate (ie. set "to-be-released") x and all its ascendants,
	proceeding top-down
*/
{
	if ( x==NULL ) return;

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
			ndx = 0;
			i = j; continue;
		}
		for ( ; ; ) {
			if ( ndx == 0 ) {
				ndx = 1;
				break;
			}
			if ((x)) {
				db_op( DB_DEPRECATE_OP, x, db );
			}
			if (( i->next )) {
				i = i->next;
				if ( !db_deprecatable( i->ptr, db ) )
					x = NULL;
				else {
					ndx = 0;
					break;
				}
			}
			else if (( stack )) {
				ndx = pop_item( &stack );
				i = popListItem( &stack );
				if ( ndx ) x = i->ptr;
			}
			else goto RETURN;
		}
	}
RETURN:
	freeItem( i );
}

//===========================================================================
//	db_signal
//===========================================================================
void
db_signal( CNInstance *x, CNDB *db )
{
	db_op( DB_SIGNAL_OP, x, db );
}

//===========================================================================
//	db_lookup
//===========================================================================
CNInstance *
db_lookup( int privy, char *p, CNDB *db )
{
	if ( p == NULL )
		return NULL;
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
char *
db_identifier( CNInstance *e, CNDB *db )
/*
	Assumption: e->sub[0]==NULL already verified
*/
{
#ifdef UNIFIED
	return (char *) e->sub[ 1 ];
#else
	Pair *entry = registryLookup( db->index, NULL, e );
	return ( entry ) ? entry->name : NULL;
#endif
}

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
				return e;
			}
		}
	}
	listItem *i = popListItem( stack );
	for ( ; ; ) {
		if (( i->next )) {
			i = i->next;
			if (( *stack ))
				e = i->ptr;
			else {
				// e in db->index
				Pair *entry = i->ptr;
				e = entry->value;
			}
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e;
			}
		}
		else if (( *stack )) {
			i = popListItem( stack );
		}
		else return NULL;
	}
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
			e = pair->value;
		}
		listItem *j = e->as_sub[ 0 ];
		if (( j )) {
			addItem( &stack, i );
			i = j; continue;
		}
		while (( i )) {
			if ( user_CB( e, db, user_data ) ) {
				freeListItem( &n );
				freeListItem( &stack );
				return 1;
			}
			if (( i->next )) {
				i = i->next;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				if ((stack) || (n))
					e = i->ptr;
				else {
					Pair *pair = i->ptr;
					e = pair->value;
				}
			}
			else if (( n )) {
				freeListItem( &n );
				i = db->index->entries;
				break;
			}
			else i = NULL;
		}
	}
	return 0;
}

//===========================================================================
//	db_outputf
//===========================================================================
static int db_output( FILE *, CNDB *, int type, CNInstance * );

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
				db_output( stream, db, p[1], e );
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
db_output( FILE *stream, CNDB *db, int type, CNInstance *e )
/*
	type is either 's' or '_', the only difference being that,
	in the former case, we insert a backslash at the start of
	non-base entity expressions
*/
{
	if ( e == NULL ) return 0;
	if ( type=='s' ) {
		if (( e->sub[ 0 ] ))
			fprintf( stream, "\\" );
		else {
			char *p = db_identifier( e, db );
			if (( p )) fprintf( stream, "%s", p );
			return 0;
		}
	}
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( e->sub[ ndx ] )) {
			fprintf( stream, ndx==0 ? "(" : "," );
			add_item( &stack, ndx );
			addItem( &stack, e );
			e = e->sub[ ndx ];
			ndx = 0;
			continue;
		}
		char *p = db_identifier( e, db );
		if (( p )) {
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
				}
			}
		}
		for ( ; ; ) {
			if (( stack )) {
				e = popListItem( &stack );
				ndx = pop_item( &stack );
				if ( ndx==0 ) { ndx = 1; break; }
				else fprintf( stream, ")" );
			}
			else goto RETURN;
		}
	}
RETURN:
	return 0;
}

