#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "program.h"
#include "locate.h"

//===========================================================================
//	newContext / freeContext
//===========================================================================
typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;
	
BMContext *
newContext( CNDB *db, CNEntity *parent )
{
	CNEntity *this = cn_new( NULL, NULL );
	this->sub[ 0 ] = (CNEntity *) db;
	Pair *id = newPair( NULL, NULL );
	if (( parent ))
		id->name = db_proxy( this, parent, db );
	id->value = db_proxy( NULL, this, db );
	Pair *active = newPair( newPair(NULL,NULL), NULL );
	Registry *registry = newRegistry( IndexedByCharacter );
	registryRegister( registry, "%", id ); // aka. .. and %%
	registryRegister( registry, "@", active ); // active connections (proxies)
	registryRegister( registry, ".", NULL ); // sub-narrative instance
	registryRegister( registry, "?", NULL ); // aka. %?
	registryRegister( registry, "!", NULL ); // aka. %!
	registryRegister( registry, "|", NULL ); // aka. %|
	return (BMContext *) newPair( this, registry );
}

void
freeContext( BMContext *ctx )
/*
	Assumption: ctx->this->sub[ 1 ]==*BMContextCarry(ctx)==NULL
*/
{
	// flush active connections register
	ActiveRV *active = BMContextActive( ctx );
	freeListItem( &active->buffer->activated );
	freeListItem( &active->buffer->deactivated );
	freePair((Pair *) active->buffer );
	freeListItem( &active->value );
	freePair((Pair *) active );

	/* prune CNDB proxies & release input connections ( this, . )
	   this includes parent connection & proxy if these were set
	*/
	CNEntity *this = ctx->this;
	for ( listItem *i=this->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		cn_prune( connection->as_sub[0]->ptr ); // remove proxy
		cn_release( connection ); }

	// free context id register
	Pair *id = BMContextId( ctx );
	CNInstance *self = id->value; // self is a proxy
	if (( self )) {
		CNEntity *connection = self->sub[ 0 ];
		cn_prune( self ); cn_free( connection ); }
	freePair( id );

	// free CNDB now that it is free from any proxy attachment
	CNDB *db = (CNDB *) this->sub[ 0 ];
	this->sub[ 0 ] = NULL;
	freeCNDB( db );

	/* release this now that both its subs are NULL
	   cn_release will nullify this in subscribers' connections ( ., this )
	   subscriber is responsible for removing dangling connections
	*/
	cn_release( this );

	/* free context register & variable registry - now presumably
	   all flushed out
	*/
	freeRegistry( ctx->registry, NULL );
	freePair((Pair *) ctx );
}

//===========================================================================
//	bm_context_finish
//===========================================================================
void
bm_context_finish( BMContext *ctx, int subscribe )
/*
	Assumption: only invoked at inception - see bm_conceive()
	finish cell's context: activate or deprecate proxy to parent
*/
{
	Pair *id = BMContextId( ctx );
	if ( subscribe ) {
		ActiveRV *active = BMContextActive( ctx );
		addIfNotThere( &active->buffer->activated, id->name ); }
	else {
		db_deprecate( id->name, BMContextDB(ctx) );
		id->name = NULL; }
}

//===========================================================================
//	bm_activate / bm_deactivate
//===========================================================================
BMCBTake
bm_activate( CNInstance *proxy, BMContext *ctx, void *user_data )
{
	if ((proxy->sub[1]) || !proxy->sub[0] || !proxy->sub[0]->sub[0] )
		return BM_CONTINUE;
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->activated, proxy );
	return BM_CONTINUE;
}
BMCBTake
bm_deactivate( CNInstance *proxy, BMContext *ctx, void *user_data )
{
	if ((proxy->sub[1]) || !proxy->sub[0] || !proxy->sub[0]->sub[0] )
		return BM_CONTINUE;
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->deactivated, proxy );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_context_init / bm_context_update
//===========================================================================
static void update_active( ActiveRV * );

void
bm_context_init( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	db_update( db, NULL ); // integrate cell's init conditions
	update_active( BMContextActive(ctx) );
	db_init( db );
}
void
bm_context_update( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	db_update( db, BMContextParent(ctx) );

	// deactivate connections whose proxies were deprecated
	ActiveRV *active = BMContextActive( ctx );
	update_active( active );
	listItem **entries = &active->value;
	for ( listItem *i=*entries, *last_i=NULL, *next_i; i!=NULL; i=next_i ) {
		CNInstance *e = i->ptr; next_i = i->next;
		if ( db_deprecated( e, db ) )
			clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
}
static void
update_active( ActiveRV *active )
{
	listItem **change = &active->buffer->deactivated;
	for ( CNInstance *e;( e = popListItem(change) ); )
		removeIfThere( &active->value, e );
	change = &active->buffer->activated;
	for ( CNInstance *e;( e = popListItem(change) ); )
		addIfNotThere( &active->value, e );
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
		entry->value = db_register( "this", BMContextDB(ctx) );
		instance = entry->value;
	}
	return instance;
}

//===========================================================================
//	bm_inform / bm_context_inform
//===========================================================================
static CNInstance * lookup_proxy( BMContext *, CNInstance * );

listItem *
bm_inform( BMContext *ctx, listItem **instances, BMContext *dst )
{
	listItem *results = NULL;
	for ( CNInstance *e;( e = popListItem(instances) ); ) {
		e = bm_context_inform( ctx, e, dst );
		if (( e )) addItem( &results, e ); }
	return results;
}
CNInstance *
bm_context_inform( BMContext *ctx, CNInstance *e, BMContext *dst )
{
	if ( !e ) return NULL;
	CNDB *db_src = BMContextDB( ctx );
	CNDB *db_dst = BMContextDB( dst );
	struct { listItem *src, *dst; } stack = { NULL, NULL };
	CNInstance *instance;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(e,ndx) )) {
			add_item( &stack.src, ndx );
			addItem( &stack.src, e );
			e = e->sub[ ndx ];
			ndx = 0; continue; }

		if (( e->sub[ 0 ] )) { // proxy e:(( this, that ), NULL )
			CNInstance *that = e->sub[ 0 ]->sub[ 1 ];
			if (!( instance = lookup_proxy( dst, that ) ))
				instance = db_proxy( dst->this, that, db_dst ); }
		else {
			char *p = db_identifier( e, db_src );
			instance = db_register( strmake(p), db_dst ); }
		for ( ; ; ) {
			if (( stack.src )) {
				e = popListItem( &stack.src );
				if (( ndx = pop_item( &stack.src ) )) {
					e = popListItem( &stack.dst );
					instance = db_instantiate( e, instance, db_dst ); }
				else {
					addItem( &stack.dst, instance );
					ndx=1; break; } }
			else return instance; } }
}
static CNInstance *
lookup_proxy( BMContext *ctx, CNInstance *that )
/*
	Assumption: there is at most one %( ?:( ctx->this, that ), . )
	and - if there is one - that is to ( %?, NULL )==that proxy
	otherwise we would have to test each (%?,.) to find (%?,NULL)
*/
{
	CNInstance *this = ctx->this;
	if ( that==this ) return BMContextSelf( ctx );
	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNInstance *connection = i->ptr;
		if ( connection->sub[ 1 ]==that )
			return connection->as_sub[ 0 ]->ptr; }
	return NULL;
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
		case '|':
			freeListItem((listItem **) &entry->value );
			last_i = i;
			break;
		default:
			if ( !is_separator( *name ) ) {
				clipListItem( entries, i, last_i, next_i );
				freePair( entry ); }
			else last_i = i;
		} }
}
void
bm_flush_pipe( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx->registry, "|" );
	freeListItem((listItem **) &entry->value );
}

//===========================================================================
//	bm_context_mark / bm_context_unmark
//===========================================================================
static inline CNInstance * xsub( CNInstance *, listItem ** );

int
bm_context_mark( BMContext *ctx, char *expression, CNInstance *x, int *marked )
{
	if ( !x ) return 0;
	listItem *xpn = NULL;
	if ( *expression==':' ) {
		char *value = p_prune( PRUNE_FILTER, expression+1 ) + 1;
		if ( !strncmp( value, "~.", 2 ) ) {
			// we know x: ( *, e )
			if (( bm_locate_mark( expression, &xpn ) )) {
				*marked = QMARK|EMARK;
				bm_push_mark( ctx, '?', xsub(x->sub[1],&xpn) );
				bm_push_mark( ctx, '!', NULL ); } }
		else {
			// we know x: (( *, e ), f )
			if (( bm_locate_mark( value, &xpn ) )) {
				*marked = EMARK|QMARK;
				bm_push_mark( ctx, '!', x->sub[0]->sub[1] );
				bm_push_mark( ctx, '?', xsub(x->sub[1],&xpn) ); }
			else if (( bm_locate_mark( expression, &xpn ) )) {
				*marked = QMARK|EMARK;
				bm_push_mark( ctx, '?', xsub(x->sub[0]->sub[1],&xpn) );
				bm_push_mark( ctx, '!', x->sub[1] ); } } }
	else {
		if (( bm_locate_mark( expression, &xpn ) )) {
			*marked = QMARK;
			bm_push_mark( ctx, '?', xsub(x,&xpn) ); } }
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

void
bm_context_unmark( BMContext *ctx, int marked )
{
	if ( marked&EMARK ) bm_pop_mark( ctx, '!' );
	if ( marked&QMARK ) bm_pop_mark( ctx, '?' );
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
		case '|':
			if ((entry->value))
				return ((listItem*)entry->value)->ptr;
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
			CNInstance *x = db_register( p, db );
			x = db_instantiate( this, x, db );
			registryRegister( ctx->registry, p, x ); }
		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++; }
	return 1;
ERR: ;
	CNInstance *instance = entry->value;
	if ( instance->sub[ 0 ]==this ) {
		fprintf( stderr, ">>>>> B%%: Error: LOCALE "
			"multiple declarations: '.%s'\n", p ); }
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
	case '.':
		switch ( p[1] ) {
		case '.': return BMContextParent( ctx );
		default: return bm_context_lookup( ctx, "." ); }
	case '%': // looking up %?, %!, %|, %% or just plain old %
		switch ( p[1] ) {
		case '?': return bm_context_lookup( ctx, "?" );
		case '!': return bm_context_lookup( ctx, "!" );
		case '|': return bm_context_lookup( ctx, "|" );
		case '%': return BMContextSelf( ctx ); }
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
			return db_register( q.s, BMContextDB(ctx) ); }
		return NULL; }
	else if ( !is_separator(*p) ) {
		// registering normal identifier instance
		Pair *entry = registryLookup( ctx->registry, p );
		if (( entry )) return entry->value; }

	return db_register( p, BMContextDB(ctx) );
}
