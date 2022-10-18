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
	registryRegister( registry, ".", NULL ); // sub-narrative instance
	registryRegister( registry, "?", NULL ); // aka. %?
	registryRegister( registry, "!", NULL ); // aka. %!
	return (BMContext *) newPair( this, registry );
}
void
freeContext( BMContext *ctx )
/*
	Assumption: ctx->registry is clear (see bm_context_flush)
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
//	bm_context_set / bm_context_fetch
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
	Pair *entry = registryLookup( registry, "." );
	entry->value = instance;
	listItem *xpn = NULL;
	RegisterData data = { instance, registry };
	bm_locate_param( proto, &xpn, register_CB, &data );
}
static void
register_CB( char *p, listItem *exponent, void *user_data )
{
	RegisterData *data = user_data;
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=NULL; i=i->next )
		addItem( &xpn, i->ptr );
	int exp; // note that we do not reorder xpn
	CNInstance *instance = data->instance;
	while (( exp = pop_item( &xpn ) ))
		instance = instance->sub[ exp & 1 ];
	registryRegister( data->registry, p, instance );
}

CNInstance *
bm_context_fetch( BMContext *ctx, char *unused )
{
	Pair *entry = registryLookup( ctx->registry, "." );
	CNInstance *instance = entry->value;
	if ( !instance ) {
		// base narrative LOCALE declaration: authorize
		// creation of a base entity named "this"
		entry->value = db_register( "this", BMContextDB(ctx), 1 );
		instance = entry->value;
	}
	return instance;
}
//===========================================================================
//	bm_context_mark
//===========================================================================
static inline CNInstance * xsub( CNInstance *, listItem ** );

int
bm_context_mark( BMContext *ctx, char *expression, CNInstance *x, int *marked )
{
	if ( !x ) return 0;
	listItem *xpn = NULL;
	if (( bm_locate_mark( expression, &xpn ) )) {
		*marked = 1;
		bm_push_mark( ctx, '?', xsub(x,&xpn) ); }
	return 1;
}

static inline CNInstance *
xsub( CNInstance *x, listItem **xpn )
/*
	Assumption: x.xpn exists by construction
*/
{
	int exp;
	reorderListItem( xpn );
	while (( exp = pop_item( xpn ) ))
		x = x->sub[ exp & 1 ];
	return x;
}

//===========================================================================
//	bm_context_flush / bm_flush_pipe
//===========================================================================
void
bm_context_flush( BMContext *ctx )
{
	Registry *registry = ctx->registry;
	listItem **entries = &registry->entries;
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		Pair *entry = i->ptr;
		next_i = i->next;
		char *name = entry->name;
		switch ( *name ) {
		case '?':
		case '!':
			freeListItem((listItem **) &entry->value );
			last_i = i;
			break;
		default:
			if ( !is_separator( *name ) ) {
				clipListItem( entries, i, last_i, next_i );
				freePair( entry ); }
			else last_i = i;
		}
	}
}

void
bm_flush_pipe( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "!" );
	freeListItem((listItem **) &entry->value );
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
				return stack->ptr; }
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
/*
	register .locale(s)
*/
{
	CNInstance *this = bm_context_fetch( ctx, "." );
	Pair *entry;
	CNDB *db = BMContextDB( ctx );
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
	switch ( *p ) {
	case '.': return bm_context_lookup( ctx, "." );
	case '%': // looking up %., %?, %! or just plain old %
		switch ( p[1] ) {
		case '.': return bm_context_lookup( ctx, "." );
		case '?': return bm_context_lookup( ctx, "?" );
		case '!': return bm_context_lookup( ctx, "!" ); }
		return db_lookup( privy, p, BMContextDB(ctx) );
	case '\'': ; // looking up single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) )
			return db_lookup( privy, q.s, BMContextDB(ctx) );
		return NULL;
	default: if ( is_separator(*p) ) break;
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
