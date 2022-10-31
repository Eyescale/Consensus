#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "locate.h"

#define TEMPORARILY

//===========================================================================
//	newContext / freeContext
//===========================================================================
BMContext *
newContext( CNEntity *cell, CNEntity *parent )
{
	Registry *ctx = newRegistry( IndexedByCharacter );
	CNDB *db = newCNDB();
	Pair *id = newPair(
		db_proxy( NULL, cell, db ),
		db_proxy( cell, parent, db ) );
	Pair *active = newPair(
		newPair( NULL, NULL ),
		NULL );
	registryRegister( ctx, "", id ); // aka. %% and .. (proxies)
	registryRegister( ctx, "%", db );
	registryRegister( ctx, "@", active ); // active connections (proxies)
	registryRegister( ctx, ".", NULL ); // sub-narrative instance
	registryRegister( ctx, "?", NULL ); // aka. %?
	registryRegister( ctx, "!", NULL ); // aka. %!
	registryRegister( ctx, "|", NULL ); // aka. %|
	return ctx;
}

void
freeContext( BMContext *ctx )
{
	// free active connections register
	ActiveRV *active = BMContextActive( ctx );
	freeListItem( &active->buffer->activated );
	freeListItem( &active->buffer->deactivated );
	freePair((Pair *) active->buffer );
	freeListItem( &active->value );
	freePair((Pair *) active );

	// free context self & id register value
	Pair *id = BMContextId( ctx );
	CNInstance *self = id->name; // self is a proxy
	CNEntity *connection = self->sub[ 0 ];
	cn_prune( self ); // remove proxy
	cn_release( connection );
	freePair( id );

	// free CNDB now that it is free from any proxy attachment
	CNDB *db = BMContextDB( ctx );
	freeCNDB( db );

	/* free context register & variable registry - now
	   all flushed out
	*/
	freeRegistry( ctx, NULL );
}

//===========================================================================
//	bm_context_finish / bm_activate
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
		addIfNotThere( &active->buffer->activated, id->value ); }
	else {
		db_deprecate( id->value, BMContextDB(ctx) );
		id->value = NULL; }
}

void
bm_context_activate( BMContext *ctx, CNInstance *proxy )
{
	ActiveRV *active = BMContextActive( ctx );
	addIfNotThere( &active->buffer->activated, proxy );
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
static void
update_active( ActiveRV *active )
{
	listItem **changes = &active->buffer->deactivated;
	for ( CNInstance *e;( e = popListItem(changes) ); )
		removeIfThere( &active->value, e );
	changes = &active->buffer->activated;
	for ( CNInstance *e;( e = popListItem(changes) ); )
		addIfNotThere( &active->value, e );
}

void
bm_context_update( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	db_update( db, BMContextParent(ctx) );
	ActiveRV *active = BMContextActive( ctx );
	update_active( active );

	// deactivate connections whose proxies were deprecated
	listItem **entries = &active->value;
	for ( listItem *i=*entries, *last_i=NULL, *next_i; i!=NULL; i=next_i ) {
		CNInstance *e = i->ptr;
		next_i = i->next;
		if ( db_deprecated( e, db ) )
			clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
}

//===========================================================================
//	bm_context_set / bm_context_declare / bm_context_check
//===========================================================================
typedef struct {
	CNInstance *instance;
	Registry *ctx;
} RegisterData;
static BMLocateCB register_CB;

void
bm_context_set( BMContext *ctx, char *proto, CNInstance *instance )
{
	if ( !instance ) return;
	Pair *entry = registryLookup( ctx, "." );
	entry->value = instance;
	listItem *xpn = NULL;
	RegisterData data = { instance, ctx };
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
	registryRegister( data->ctx, p, instance );
}

int
bm_context_declare( BMContext *ctx, char *p )
/*
	register .locale(s)
*/
{
	CNInstance *perso = BMContextPerso( ctx );
	Pair *entry;
	CNDB *db = BMContextDB( ctx );
	while ( *p=='.' ) {
		p++;
		entry = registryLookup( ctx, p );
		if (( entry )) goto ERR; // already registered
		else {
			CNInstance *x = db_register( p, db );
			if (( perso )) x = db_instantiate( perso, x, db );
			registryRegister( ctx, p, x ); }
		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++; }
	return 1;
ERR: ;
	CNInstance *instance = entry->value;
	if ( instance->sub[ 0 ]==perso ) {
		fprintf( stderr, ">>>>> B%%: Error: LOCALE "
			"multiple declarations: '.%s'\n", p ); }
	else fprintf( stderr, ">>>>> B%%: Error: LOCALE name '.%s' "
		"conflict with sub-narrative .arg\n", p );
	return -1;
}

void
bm_context_check( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	ActiveRV *active = BMContextActive( ctx );
	listItem **entries = &active->value;
	for ( listItem *i=*entries, *last_i=NULL, *next_i; i!=NULL; i=next_i ) {
		next_i = i->next;
		CNInstance *proxy = i->ptr;
		CNEntity *connection = proxy->sub[ 0 ];
		if ( !connection->sub[ 1 ] ) {
			clipListItem( entries, i, last_i, next_i );
			db_deprecate( proxy, db ); }
		else last_i = i; }
}

//===========================================================================
//	bm_context_flush / bm_context_pipe_flush
//===========================================================================
void
bm_context_flush( BMContext *ctx )
{
	listItem **entries = &ctx->entries;
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
			else last_i = i; } }
}
void
bm_context_pipe_flush( BMContext *ctx )
{
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value );
}

//===========================================================================
//	bm_context_mark / bm_context_mark_x / bm_context_unmark
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

int
bm_context_mark_x( BMContext *ctx, char *expression, CNInstance *x, int *marked )
{
	if ( !x ) return 0;
	return 1;
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
	Pair *entry = registryLookup( ctx, ((char_s) mark).s );
	return ( entry ) ? addItem((listItem**)&entry->value, e ) : NULL;
}
void *
bm_pop_mark( BMContext *ctx, int mark )
{
	Pair *entry = registryLookup( ctx, ((char_s) mark).s );
	return ( entry ) ? popListItem((listItem**)&entry->value) : NULL;
}

//===========================================================================
//	bm_context_lookup
//===========================================================================
void *
bm_context_lookup( BMContext *ctx, char *p )
{
	Pair *entry = registryLookup( ctx, p );
	if (( entry ))
		switch ( *p ) {
		case '?':
		case '!':
		case '|': ;
			if ((entry->value))
				return ((listItem*)entry->value)->ptr;
			break;
		default:
			return entry->value;
		}
	return NULL;
}

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx, CNDB *db )
{
	switch ( *p ) {
	case '.':
		return ( p[1]=='.' ?
			BMContextParent(ctx) : BMContextPerso(ctx) );
	case '%': // looking up %?, %!, %|, %% or just plain old %
		switch ( p[1] ) {
		case '?': return bm_context_lookup( ctx, "?" );
		case '!': return bm_context_lookup( ctx, "!" );
		case '|': return bm_context_lookup( ctx, "|" );
		case '%': return BMContextSelf( ctx ); }
		break;
	case '\'': ; // looking up single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) )
			return db_lookup( privy, q.s, db );
		return NULL;
	default: if ( is_separator(*p) ) break;
		// looking up normal identifier instance
		CNInstance *instance = bm_context_lookup( ctx, p );
		if (( instance )) return instance; }

	return db_lookup( privy, p, db );
}

//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNDB *db )
{
	if ( *p == '\'' ) {
		// registering single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) ) {
			return db_register( q.s, db ); }
		return NULL; }
	else if ( !is_separator(*p) ) {
		// registering normal identifier instance
		Pair *entry = registryLookup( ctx, p );
		if (( entry )) return entry->value; }

	return db_register( p, db );
}

//===========================================================================
//	bm_inform / bm_context_inform
//===========================================================================
static CNInstance * lookup_proxy( CNEntity *, CNInstance * );

listItem *
bm_inform( BMContext *src, CNDB *db, listItem **instances, BMContext *dst )
{
	listItem *results = NULL;
	for ( CNInstance *e;( e = popListItem(instances) ); ) {
		e = bm_context_inform( src, db, e, dst );
		if (( e )) addItem( &results, e ); }
	return results;
}
CNInstance *
bm_context_inform( BMContext *src, CNDB *db_src, CNInstance *e, BMContext *dst )
{
	if ( !e ) return NULL;
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
			CNEntity *that = BMProxyThat( e );
			CNEntity *this = BMContextThis( dst );
			if (!( instance = lookup_proxy( this, that ) ))
				instance = db_proxy( this, that, db_dst ); }
		else {
			char *p = db_identifier( e, db_src );
			instance = db_register( p, db_dst ); }
		for ( ; ; ) {
			if ( !stack.src ) return instance;
			e = popListItem( &stack.src );
			if (( ndx = pop_item( &stack.src ) )) {
				e = popListItem( &stack.dst );
				instance = db_instantiate( e, instance, db_dst ); }
			else {
				addItem( &stack.dst, instance );
				ndx=1; break; }
		} }
}
static CNInstance *
lookup_proxy( CNEntity *this, CNEntity *that )
{
	if ( this==that ) {
		for ( listItem *i=this->as_sub[1]; i!=NULL; i=i->next ) {
			CNEntity *connection = i->ptr;
			// Assumption: there is only one ((NULL,this), . )
			if ( !connection->sub[ 0 ] )
				return connection->as_sub[ 0 ]->ptr; } }
	else {
		for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
			CNEntity *connection = i->ptr;
			// Assumption: there is at most one ((this,~NULL), . )
			if ( connection->sub[ 1 ]==that )
				return connection->as_sub[ 0 ]->ptr; } }
	return NULL;
}

