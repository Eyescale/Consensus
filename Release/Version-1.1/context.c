#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "util.h"

//===========================================================================
//	bm_push / bm_pop
//===========================================================================
typedef struct {
	CNInstance *e;
	Registry *registry;
} RegisterData;
static BMLocateCB arg_register_CB;

BMContext *
bm_push( CNNarrative *n, CNInstance *e, CNDB *db )
{
	Registry *registry = newRegistry( IndexedByCharacter );
	registryRegister( registry, "?", NULL ); // aka. %?
	if (( e )) {
		registryRegister( registry, "this", e );
		if ((n) && (n->proto)) {
			listItem *exponent = NULL;
			RegisterData data = { e, registry };
			p_locate_param( n->proto, &exponent, arg_register_CB, &data );
		}
	}
	return (BMContext *) newPair( db, registry );
}
static void
arg_register_CB( char *p, listItem *exponent, void *user_data )
{
	RegisterData *data = user_data;
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=NULL; i=i->next )
		addItem( &xpn, i->ptr );
	int exp;
	CNInstance *x = data->e;
	while (( exp = (int) popListItem( &xpn ) ))
		x = x->sub[ exp & 1 ];
	registryRegister( data->registry, p, x );
}

void
bm_pop( BMContext *ctx )
{
	Registry *registry = ctx->registry;
	if (( registry )) {
		freeRegistry( registry, NULL );
	}
	freePair((Pair *) ctx );
}

//===========================================================================
//	push_mark_register / pop_mark_register
//===========================================================================
listItem *
push_mark_register( BMContext *ctx, CNInstance *e )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return ( entry ) ? addItem((listItem**)&entry->value, e ) : NULL;
}
CNInstance *
pop_mark_register( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return ( entry ) ? popListItem((listItem**)&entry->value) : NULL;
}
static CNInstance *
lookup_mark_register( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "?" );
	return (entry) && (entry->value) ? ((listItem*)entry->value)->ptr : NULL;
}

//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNInstance *e )
{
	Registry *registry = ctx->registry;
	Pair *entry;
	switch ( *p ) {
	case '.': // registering .local(s)
		entry = registryLookup( registry, "this" );
		if ( entry == NULL ) return NULL; // base narrative

		CNDB *db = ctx->db;
		CNInstance *this = entry->value;
		for ( ; ; p=p_prune( PRUNE_IDENTIFIER, p )) {
			for ( ; *p!='.' || is_separator(p[1]); p++ )
				if ( *p == '\0' ) return NULL;
			p++;
			entry = registryLookup( registry, p );
			if (( entry )) {
				CNInstance *e = entry->value;
				if ( e->sub[ 0 ] == this )
					continue; // already registered
			}
			/* assuming previous entry was registered
			   either as .arg or not at all
			*/
			CNInstance *x = db_register( p, db );
			x = db_instantiate( this, x, db );
			if (( entry )) {
				entry->name = p;
				entry->name = x;
			}
			else registryRegister( registry, p, x );
		}
	case '\'':; // registering single character identifier instance
		char q[ MAXCHARSIZE + 1 ];
		if ( charscan(p+1,q) )
			return db_register( q, ctx->db );
		break;
	default: // registering normal identifier instance
		if ( !is_separator(*p) ) {
			entry = registryLookup( registry, p );
			if (( entry )) return entry->value;
		}
		return db_register( p, ctx->db );
	}
	return NULL;
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	Pair *entry;
	char q[ MAXCHARSIZE + 1 ];
	switch ( *p ) {
	case '%': // looking up %? or %
		return ( p[1] == '?' ) ? lookup_mark_register( ctx ) :
			db_lookup( privy, p, ctx->db );
	case '\'': // looking up single character identifier instance
		return ( charscan( p+1, q ) ) ?
			db_lookup( privy, q, ctx->db ) : NULL;
	default: // looking up normal identifier instance
		if ( !is_separator(*p) ) {
			entry = registryLookup( ctx->registry, p );
			if (( entry )) return entry->value;
		}
		return db_lookup( privy, p, ctx->db );
	}
}

