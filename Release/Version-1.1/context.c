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
#define MAXCHARSIZE 1
static int charscan( char *p, char *q );

CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	char q[ MAXCHARSIZE ];
	Pair *entry;
	switch ( *p ) {
	case '%': // looking up %?
		if ( p[1] == '?' ) {
			entry = registryLookup( ctx->registry, "?" );
			return (entry) ? entry->value : NULL;
		}
		break;
	case '\'': // looking up single character identifier instance
		if ( charscan( p+1, q ) )
			return db_lookup( privy, q, ctx->db );
		break;
	default: // looking up normal identifier instance
		entry = registryLookup( ctx->registry, p );
		return (entry) ? entry->value : db_lookup( privy, p, ctx->db );
	}
	return NULL;
}
#define HVAL(c) (c-((c<58)?48:55))
static int
charscan( char *p, char *q )
{
	switch ( *p ) {
	case '\\':
		switch ( p[1] ) {
		case 'x':  q[ 0 ] = HVAL(p[2])*16 + HVAL(p[3]); break;
		case '0':  q[ 0 ] = '\0'; break;
		case 't':  q[ 0 ] = '\t'; break;
		case 'n':  q[ 0 ] = '\n'; break;
		case '\\': q[ 0 ] = '\\'; break;
		case '\"': q[ 0 ] = '\"'; break;
		default: return 0;
		}
		break;
	default:
		q[ 0 ] = *p;
	}
	return 1;
}

//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNInstance *e )
{
	Registry *registry = ctx->registry;
	char q[ MAXCHARSIZE ];
	Pair *entry;
	switch ( *p ) {
	case '?': // registering %?
		entry = registryLookup( registry, "?" );
		return ( entry ) ? (entry->value=e) : NULL;
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
		break;
	case '\'': // registering single character identifier instance
		if ( charscan(p+1,q) )
			return db_register( q, ctx->db );
		break;
	default: // registering normal identifier instance
		entry = registryLookup( registry, p );
		return (entry) ? entry->value : db_register( p, ctx->db );
	}
	return NULL;
}

