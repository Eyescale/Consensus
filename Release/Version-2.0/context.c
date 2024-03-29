#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "scour.h"
#include "locate_param.h"
#include "proxy.h"
#include "eenov.h"

//===========================================================================
//	newContext / freeContext
//===========================================================================
BMContext *
newContext( CNEntity *cell, CNEntity *parent )
{
	Registry *ctx = newRegistry( IndexedByNameRef );
	CNDB *db = newCNDB();
	Pair *id = newPair(
		db_proxy( NULL, cell, db ),
		db_proxy( cell, parent, db ) );
	Pair *active = newPair(
		newPair( NULL, NULL ),
		NULL );
	registryRegister( ctx, "", id ); // aka. %% and ..
	registryRegister( ctx, "%", db );
	registryRegister( ctx, "@", active ); // active connections (proxies)
	registryRegister( ctx, ".", NULL ); // aka. perso
	registryRegister( ctx, "?", NULL ); // aka. %?
	registryRegister( ctx, "!", NULL ); // aka. %!
	registryRegister( ctx, "|", NULL ); // aka. %|
	registryRegister( ctx, "<", NULL ); // aka. %<?> %<!> %<
	return ctx;
}

void
freeContext( BMContext *ctx )
/*
	Assumptions - cf releaseCell()
		. CNDB is free from any proxy attachment
		. id Self and Parent proxies have been released
		. ctx registry all flushed out
*/
{
	// free active connections register
	ActiveRV *active = BMContextActive( ctx );
	freeListItem( &active->buffer->activated );
	freeListItem( &active->buffer->deactivated );
	freePair((Pair *) active->buffer );
	freeListItem( &active->value );
	freePair((Pair *) active );

	// free context id register value
	Pair *id = BMContextId( ctx );
	freePair( id );

	// free CNDB & context registry
	CNDB *db = BMContextDB( ctx );
	freeCNDB( db );
	freeRegistry( ctx, NULL );
}

//===========================================================================
//	bm_context_finish / bm_context_activate
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
static inline void update_active( ActiveRV * );

void
bm_context_init( BMContext *ctx )
{
	CNDB *db = BMContextDB( ctx );
	// integrate cell's init conditions
	db_update( db, NULL );
	// activate parent connection, if there is
	update_active( BMContextActive(ctx) );
	db_init( db );
}
int
bm_context_update( CNEntity *this, BMContext *ctx )
{
#ifdef DEBUG
	fprintf( stderr, "bm_context_update: bgn\n" );
#endif
	CNDB *db = BMContextDB( ctx );
	// activate / deactivate connections according to user request
	ActiveRV *active = BMContextActive( ctx );
	update_active( active );
	// deprecate dangling connections
	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		if (( connection->sub[ 1 ] )) continue;
		CNInstance *proxy = connection->as_sub[0]->ptr;
		if ( db_deprecatable( proxy, db ) )
			db_deprecate( proxy, db ); }
	// invoke db_update - turning deprecated into released
	db_update( db, BMContextParent(ctx) );
	// deactivate released connections
	listItem **entries = &active->value;
	for ( listItem *i=*entries, *last_i=NULL, *next_i; i!=NULL; i=next_i ) {
		CNInstance *e = i->ptr;
		next_i = i->next;
		if ( db_deprecated( e, db ) )
			clipListItem( entries, i, last_i, next_i );
		else last_i = i; }
#ifdef DEBUG
	fprintf( stderr, "bm_context_update: end\n" );
#endif
	return DBExitOn( db );
}
static inline void
update_active( ActiveRV *active )
{
	CNInstance *proxy;
	listItem **current = &active->value;
	listItem **buffer = &active->buffer->deactivated;
	while (( proxy = popListItem(buffer) ))
		removeIfThere( current, proxy );
	buffer = &active->buffer->activated;
	while (( proxy = popListItem(buffer) ))
		addIfNotThere( current, proxy );
}

//===========================================================================
//	bm_context_set / bm_declare
//===========================================================================
typedef struct {
	CNInstance *instance;
	Registry *ctx;
} RegisterData;
static BMLocateCB register_CB;

void
bm_context_set( BMContext *ctx, char *proto, CNInstance *instance )
{
	if (( instance )) {
		Pair *entry = registryLookup( ctx, "." );
		entry->value = instance;
		listItem *xpn = NULL;
		RegisterData data = { instance, ctx };
		bm_locate_param( proto, &xpn, register_CB, &data ); }
	else {
		Pair *entry = registryLookup( ctx, "." );
		entry->value = BMContextSelf( ctx ); }
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
bm_declare( BMContext *ctx, char *p )
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
		if (( entry )) { // already registered
			CNInstance *instance = entry->value;
			fprintf( stderr, ">>>>> B%%: Error: LOCALE " );
			fprintf( stderr, ( instance->sub[ 0 ]==perso ?
				"multiple declarations: '.%s'\n" :
				"name %s conflict with sub-narrative .arg\n" ),
				p );
			exit( -1 ); }
		else {
			CNInstance *x = db_register( p, db );
			x = db_instantiate( perso, x, db );
			registryRegister( ctx, p, x ); }
		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++; }
	return 1;
}

//===========================================================================
//	bm_context_clear / bm_context_pipe_flush
//===========================================================================
void
bm_context_clear( BMContext *ctx )
{
	listItem **stack;
	listItem **entries = &ctx->entries;
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*entries; i!=NULL; i=next_i ) {
		Pair *entry = i->ptr;
		next_i = i->next;
		char *name = entry->name;
		if ( !is_separator( *name ) ) {
			clipListItem( entries, i, last_i, next_i );
			freePair( entry ); }
		else {
			EEnoRV *eeno;
			switch ( *name ) {
			case '<':
				stack = (listItem **) &entry->value;
				while (( eeno = popListItem( stack ) )) {
					freePair( eeno->event );
					freePair((Pair *) eeno ); }
				break;
			case '?':
			case '!':
			case '|':
				stack = (listItem **) &entry->value;
				freeListItem( stack );
				break;
			case '.':
				entry->value = NULL;
				break; }
			last_i = i; } }
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
				*marked = EMARK|QMARK;
				bm_push_mark( ctx, "?", xsub(x->sub[1],&xpn) );
				bm_push_mark( ctx, "!", x ); } }
		else {
			// we know x: (( *, e ), f )
			if (( bm_locate_mark( value, &xpn ) )) {
				*marked = EMARK|QMARK;
				bm_push_mark( ctx, "?", xsub(x->sub[1],&xpn) );
				bm_push_mark( ctx, "!", x ); }
			else if (( bm_locate_mark( expression, &xpn ) )) {
				*marked = EMARK|QMARK;
				bm_push_mark( ctx, "?", xsub(x->sub[0]->sub[1],&xpn) );
				bm_push_mark( ctx, "!", x ); } } }
	else {
		if (( bm_locate_mark( expression, &xpn ) )) {
			*marked = QMARK;
			bm_push_mark( ctx, "?", xsub(x,&xpn) ); } }
	return 1;
}
int
bm_context_mark_x( BMContext *ctx, char *expression, char *src, Pair *found, int *marked )
{
	CNInstance *proxy = found->value;
	CNInstance *x = found->name;
	listItem *xpn = NULL;
	Pair *event = NULL;
	if (( *src!='{' ) && ( bm_locate_mark(src,&xpn) )) {
		freeListItem( &xpn );
		event = newPair( proxy, x ); }
	else if ( !expression )
		return 1;
	else if ( *expression==':' ) {
		char *value = p_prune( PRUNE_FILTER, expression+1 ) + 1;
		if ( !strncmp( value, "~.", 2 ) ) {
			// we know x: ( *, . )
			if (( bm_locate_mark( expression, &xpn ) ))
				event = newPair( xsub(x->sub[1],&xpn), x ); }
		else {
			// we know x: (( *, . ), . )
			if (( bm_locate_mark( value, &xpn ) )) {
				event = newPair( xsub(x->sub[1],&xpn), x ); }
			else if (( bm_locate_mark( expression, &xpn ) )) {
				event = newPair( xsub(x->sub[0]->sub[1],&xpn), x ); } } }
	else if ((x) && ( bm_locate_mark( expression, &xpn ) )) {
		event = newPair( xsub(x,&xpn), x ); }

	if (( event )) {
		*marked = EENOK;
		bm_push_mark( ctx, "<", newPair(event,proxy) ); }

	return 1;
}
static inline CNInstance *
xsub( CNInstance *x, listItem **xpn )
/*
	Assumption: x.xpn exists by construction
*/
{
	if ( !*xpn ) return x;
	int exp;
	reorderListItem( xpn );
	while (( exp = pop_item( xpn ) ))
		x = x->sub[ exp & 1 ];
	return x;
}


void
bm_context_unmark( BMContext *ctx, int marked )
{
	if ( marked&EENOK )
		bm_pop_mark( ctx, "<" );
	else {
		if ( marked&QMARK ) bm_pop_mark( ctx, "?" );
		if ( marked&EMARK ) bm_pop_mark( ctx, "!" ); }
}

//===========================================================================
//	bm_push_mark / bm_pop_mark
//===========================================================================
listItem *
bm_push_mark( BMContext *ctx, char *mark, void *value )
{
	Pair *entry = registryLookup( ctx, mark );
	if (( entry ))
		return addItem((listItem **) &entry->value, value );
	return NULL;
}
void
bm_pop_mark( BMContext *ctx, char *mark )
{
	Pair *entry = registryLookup( ctx, mark );
	EEnoRV *eeno;
	if (( entry ))
		switch ( *mark ) {
		case '<':
			eeno = popListItem((listItem **) &entry->value );
			if (( eeno )) {
				freePair( eeno->event );
				freePair((Pair *) eeno ); }
			break;
		default:
			popListItem((listItem**) &entry->value ); }
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
		case '|':
		case '<': ;
			if ((entry->value))
				return ((listItem*)entry->value)->ptr;
			break;
		default:
			return entry->value; }
	return NULL;
}

//===========================================================================
//	bm_lookup / bm_lookup_x
//===========================================================================
static inline CNInstance *proxy_that( CNEntity *, BMContext *, CNDB *, int );

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
		case '%': return BMContextSelf( ctx );
		case '<': return eenov_lookup( ctx, db, p ); }
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

CNInstance *
bm_lookup_x( CNDB *db_x, CNInstance *x, BMContext *dst, CNDB *db_dst )
{
	if ( !x ) return NULL;
	if ( db_dst==db_x ) return x;

	struct { listItem *src, *dst; } stack = { NULL, NULL };
	CNInstance *instance;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(x,ndx) )) {
			add_item( &stack.src, ndx );
			addItem( &stack.src, x );
			x = x->sub[ ndx ];
			ndx = 0; continue; }
		if (( x->sub[ 0 ] )) { // proxy x:(( this, that ), NULL )
			CNEntity *that = DBProxyThat( x );
			instance = proxy_that( that, dst, db_dst, 0 ); }
		else {
			char *p = DBIdentifier( x, db_x );
			instance = db_lookup( 0, p, db_dst ); }
		if ( !instance ) break;
		for ( ; ; ) {
			if ( !stack.src ) return instance;
			x = popListItem( &stack.src );
			if (( ndx = pop_item( &stack.src ) )) {
				CNInstance *f = popListItem( &stack.dst );
				instance = cn_instance( f, instance, 0 ); }
				if ( !instance ) goto FAIL;
			else {
				addItem( &stack.dst, instance );
				ndx=1; break; } } }
FAIL:
	freeListItem( &stack.src );
	freeListItem( &stack.dst );
	return NULL;
}

static inline CNInstance *
proxy_that( CNEntity *that, BMContext *ctx, CNDB *db, int inform )
/*
	look for proxy: (( this, that ), NULL ) where
		this = BMContextCell( ctx )
*/
{
	if ( !that ) return NULL;

	CNEntity *this = BMContextCell( ctx );
	if ( this==that )
		return BMContextSelf( ctx );

	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		// Assumption: there is at most one ((this,~NULL), . )
		if ( connection->sub[ 1 ]==that )
			return connection->as_sub[ 0 ]->ptr; }

	return ( inform ? db_proxy(this,that,db) : NULL );
}

//===========================================================================
//	bm_register
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNDB *db )
/*
	Special case: ctx==NULL => p assumed identifier
*/
{
	if ( *p == '\'' ) {
		// registering single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) ) {
			return db_register( q.s, db ); }
		return NULL; }
	else if ( (ctx) && !is_separator(*p) ) {
		// registering normal identifier instance
		Pair *entry = registryLookup( ctx, p );
		if (( entry )) return entry->value; }

	return db_register( p, db );
}

//===========================================================================
//	bm_inform / bm_inform_context
//===========================================================================
listItem *
bm_inform( CNDB *db_src, listItem **instances, BMContext *dst )
{
	listItem *results = NULL;
	for ( CNInstance *e;( e = popListItem(instances) ); ) {
		e = bm_inform_context( db_src, e, dst );
		if (( e )) addItem( &results, e ); }
	return results;
}

CNInstance *
bm_inform_context( CNDB *db_src, CNInstance *e, BMContext *dst )
{
	if ( !e ) return NULL;
	CNDB *db_dst = BMContextDB( dst );
	if ( db_dst==db_src ) return e;
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
			CNEntity *that = DBProxyThat( e );
			instance = proxy_that( that, dst, db_dst, 1 ); }
		else {
			char *p = DBIdentifier( e, db_src );
			instance = db_register( p, db_dst ); }
		if ( !instance ) break;
		for ( ; ; ) {
			if ( !stack.src ) return instance;
			e = popListItem( &stack.src );
			if (( ndx = pop_item( &stack.src ) )) {
				CNInstance *f = popListItem( &stack.dst );
				instance = db_instantiate( f, instance, db_dst );
				if ( !instance ) goto FAIL; }
			else {
				addItem( &stack.dst, instance );
				ndx=1; break; } } }
FAIL:
	freeListItem( &stack.src );
	freeListItem( &stack.dst );
	return NULL;
}

