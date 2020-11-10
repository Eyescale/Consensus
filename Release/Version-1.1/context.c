#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "util.h"

//===========================================================================
//	bm_push
//===========================================================================
typedef struct {
	CNInstance *e;
	Registry *registry;
} RegisterData;
static BMLocateCB arg_register_CB;

BMContext *
bm_push( CNNarrative *n, CNInstance *e, CNDB *db )
{
	Registry *registry = newRegistry( IndexedByToken );
	registryRegister( registry, "?", NULL ); // aka. %?
	if (( e )) {
		registryRegister( registry, "this", e );
		if ((n) && (n->proto)) {
			listItem *exponent = NULL;
			RegisterData data = { e, registry };
			p_locate_arg( n->proto, &exponent, arg_register_CB, &data );
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

//===========================================================================
//	bm_pop
//===========================================================================
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
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	if ( !strncmp(p,"%?",2) ) {
		Pair *entry = registryLookup( ctx->registry, "?" );
		return (entry) ? entry->value : NULL;
	}
	Pair *entry = registryLookup( ctx->registry, p );
	return (entry) ? entry->value : db_lookup( privy, p, ctx->db );
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
	case '?': // registering %?
		entry = registryLookup( registry, "?" );
		if (( entry )) {
			entry->value = e;
			return e;
		}
		return NULL;
	case '.': break;
	default: // registering instance
		entry = registryLookup( registry, p );
		return (entry) ? entry->value : db_register( p, ctx->db );
	}
	// registering .local(s)
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
		CNInstance *x = db_lookup( 0, p, db );
		if ( x == NULL ) {
			x = db_register( p, db );
		}
		x = db_instantiate( this, x, db );
		if (( entry )) {
			entry->name = p;
			entry->name = x;
		}
		else registryRegister( registry, p, x );
	}
	return NULL;
}

