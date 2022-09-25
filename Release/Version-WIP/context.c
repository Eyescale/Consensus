#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "locate.h"

//===========================================================================
//	newContext / freeContext
//===========================================================================
BMContext *
newContext( CNDB *db )
{
	CNInstance *this = cn_new( NULL, NULL );
	this->sub[ 1 ] = (CNInstance *) db;
	Registry *registry = newRegistry( IndexedByCharacter );
	registryRegister( registry, "%", NULL ); // aka. %%
	registryRegister( registry, "?", NULL ); // aka. %?
	registryRegister( registry, "!", NULL ); // aka. %!
	return (BMContext *) newPair( this, registry );
}
void
freeContext( BMContext *ctx )
/*
	Assumption: ctx->registry is clear (see bm_context_clear)
*/
{
	CNInstance *this = ctx->this;
	// free input connections %( this, . )
	for ( listItem *i=this->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		CNInstance *instance = i->ptr;
		removeItem( &instance->sub[1]->as_sub[1], instance );
	}
	freeListItem( &this->as_sub[ 0 ] );
	// nullify this in subscribers' connections %( ., this )
	// note that subscriber is responsible for removing the instance
	for ( listItem *i=this->as_sub[ 1 ]; i!=NULL; i=i->next ) {
		CNInstance *instance = i->ptr;
		instance->sub[ 1 ] = NULL;
	}
	freeListItem( &this->as_sub[ 1 ] );
	// free list of active connections
	freeListItem((listItem **) &this->sub[ 0 ] );
	// free whole CNDB along with "this"
	CNDB *db = (CNDB *) this->sub[1];
	cn_prune( this );
	freeCNDB( db );
	// free context & variable registry
	freeRegistry( ctx->registry, NULL );
	freePair((Pair *) ctx );
}

//===========================================================================
//	bm_context_set / bm_context_clear
//===========================================================================
typedef struct {
	CNInstance *instance;
	Registry *registry;
} RegisterData;
static BMLocateCB register_CB;

void
bm_context_set( BMContext *ctx, char *proto, CNInstance *instance )
{
	if ( !instance ) return;
	Registry *registry = ctx->registry;
	Pair *entry = registryLookup( registry, "%" );
	entry->value = instance;
	listItem *xpn = NULL;
	RegisterData data = { instance, registry };
	bm_locate_param( proto, &xpn, register_CB, &data );
}
static void
register_CB( char *p, listItem *xpn, void *user_data )
{
	RegisterData *data = user_data;
	listItem *exponent = NULL;
	for ( listItem *i=xpn; i!=NULL; i=i->next )
		addItem( &exponent, i->ptr );
	int exp;
	CNInstance *instance = data->instance;
	while (( exp = pop_item( &exponent ) ))
		instance = instance->sub[ exp & 1 ];
	registryRegister( data->registry, p, instance );
}

void
bm_context_clear( BMContext *ctx )
{
	Registry *registry = ctx->registry;
	listItem **entries = &registry->entries;
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		Pair *entry = i->ptr;
		next_i = i->next;
		char *name = entry->name;
		switch ( *name ) {
		case '!':
			freeListItem((listItem **) &entry->value );
			last_i = i;
			break;
		case '?':
			// Version-2.0: each item will be a Pair *
			freeListItem((listItem **) &entry->value );
			last_i = i;
			break;
		default:
			if ( is_separator( *name ) ) {
				last_i = i;
				break;
			}
			clipListItem( entries, i, last_i, next_i );
			freePair( entry );
		}
	}
}

//===========================================================================
//	bm_context_mark
//===========================================================================
int
bm_context_mark( BMContext *ctx, char *expression, CNInstance *found, int *marked )
{
	if ( !found ) return 0;
	else {
		listItem *xpn = NULL;
		if (( bm_locate_mark( expression, &xpn ) )) {
			*marked = 1;
			reorderListItem( &xpn );
			int exp;
			while (( exp = pop_item( &xpn ) ))
				found = found->sub[ exp & 1 ];
			bm_push_mark( ctx, '?', found );
		}
	}
	return 1;
}

//===========================================================================
//	bm_push_mark / bm_pop_mark
//===========================================================================
listItem *
bm_push_mark( BMContext *ctx, int mark, void *e )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return ( entry ) ? addItem((listItem**)&entry->value, e ) : NULL;
}
void *
bm_pop_mark( BMContext *ctx, int mark )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return ( entry ) ? popListItem((listItem**)&entry->value) : NULL;
}

//===========================================================================
//	bm_context_lookup
//===========================================================================
void *
bm_context_lookup( BMContext *ctx, char *p )
{
	Pair *entry = registryLookup( ctx->registry, p );
	if (( entry ))
		switch ( *p ) {
		case '?':
		case '!':
			if ((entry->value)) {
				listItem *stack = entry->value;
				return stack->ptr;
			}
			break;
		default:
			return entry->value;
		}
	return NULL;
}

//===========================================================================
//	bm_context_register
//===========================================================================
int
bm_context_register( BMContext *ctx, char *p )
{
	CNInstance *this;
	CNDB *db = BMContextDB( ctx );
	Pair *entry = registryLookup( ctx->registry, "%" );
	if (( entry )) this = entry->value;
	else {
		// base narrative LOCALE declaration: authorize
		// creation of a base entity named "this"
		this = db_register( "this", db, 1 );
	}
	// parse .locale(s) string
	while ( *p=='.' ) {
		p++;
		entry = registryLookup( ctx->registry, p );
		if (( entry )) goto ERR; // already registered
		else {
			CNInstance *x = db_register( p, db, 1 );
			x = db_instantiate( this, x, db );
			registryRegister( ctx->registry, p, x );
		}
		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++;
	}
	return 1;
ERR: ;
	CNInstance *instance = entry->value;
	if ( instance->sub[ 0 ] == this ) {
		fprintf( stderr, ">>>>> B%%: Error: LOCALE "
			"multiple declarations: '.%s'\n", p );
	}
	else fprintf( stderr, ">>>>> B%%: Error: LOCALE name '.%s' "
		"conflict with sub-narrative .arg\n", p );
	return -1;
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	if ( *p == '%' ) {
		// looking up %%, %?, %! or just plain old %
		switch ( p[1] ) {
		case '%': return bm_context_lookup( ctx, "%" );
		case '?': return bm_context_lookup( ctx, "?" );
		case '!': return bm_context_lookup( ctx, "!" );
		}
		return db_lookup( privy, p, BMContextDB(ctx) );
	}
	else if ( *p == '\'' ) {
		// looking up single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) ) {
			return db_lookup( privy, q.s, BMContextDB(ctx) );
		}
		return NULL;
	}
	else if ( !is_separator(*p) ) {
		// looking up normal identifier instance
		CNInstance *instance = bm_context_lookup( ctx, p );
		if (( instance )) return instance;
	}
	return db_lookup( privy, p, BMContextDB(ctx) );
}


//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p )
{
	if ( *p == '\'' ) {
		// registering single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) ) {
			return db_register( q.s, BMContextDB(ctx), 1 );
		}
		return NULL;
	}
	else if ( !is_separator(*p) ) {
		// registering normal identifier instance
		Pair *entry = registryLookup( ctx->registry, p );
		if (( entry )) return entry->value;
	}
	return db_register( p, BMContextDB(ctx), 1 );
}
