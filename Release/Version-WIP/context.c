#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "locate.h"
#include "story.h"

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
	registryRegister( registry, "<", NULL ); // aka. %<
	registryRegister( registry, "!", NULL ); // aka. %!
	return (BMContext *) newPair( this, registry );
}
void
freeContext( BMContext *ctx )
/*
	Assumption: registry variables values are all popped
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
	// free registry & ctx
	freeRegistry( ctx->registry, NULL );
	freePair((Pair *) ctx );
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
//	bm_push_mark / bm_pop_mark / lookup_mark_register
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
void *
lookup_mark_register( BMContext *ctx, int mark )
{
	Pair *entry = registryLookup( ctx->registry, ((char_s) mark).s );
	return (entry) && (entry->value) ? ((listItem*)entry->value)->ptr : NULL;
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
	// registering normal identifier instance
	return db_register( p, BMContextDB(ctx), 1 );
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	if ( *p == '%' ) {
		// looking up %% or %? or %< or %! or %
		if ( strmatch( "%?<!", p[1] ) )
			return lookup_mark_register( ctx, p[1] );
		else
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
	// looking up normal identifier instance
	return db_lookup( privy, p, BMContextDB(ctx) );
}

