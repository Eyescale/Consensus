#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "scour.h"
#include "locate_mark.h"
#include "traverse.h"
#include "eenov.h"

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
		NULL );
	if (( parent )) id->value = db_proxy( cell, parent, db );
	Pair *active = newPair(
		newPair( NULL, NULL ),
		NULL );
	registryRegister( ctx, "", db ); // BMContextDB( ctx )
	registryRegister( ctx, "%", id ); // aka. %% and ..
	registryRegister( ctx, "@", active ); // active connections (proxies)
	registryRegister( ctx, ".", NULL ); // aka. perso
	registryRegister( ctx, "?", NULL ); // aka. %?
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
	CNInstance *proxy;

	// update active registry according to user request
	ActiveRV *active = BMContextActive( ctx );
	update_active( active );

	// fire [resp. deprecate dangling] connections
	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		db_fire( connection->as_sub[0]->ptr, db ); }

	// invoke db_update
	db_update( db, BMContextParent(ctx) );

	// update active registry wrt deprecated connections
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
//	bm_context_actualize
//===========================================================================
#include "actualize_traversal.h"

typedef struct {
	CNInstance *instance;
	BMContext *ctx;
	listItem *exponent, *level;
	struct { listItem *flags, *level; } stack;
} ActualizeData;

void
bm_context_actualize( BMContext *ctx, char *proto, CNInstance *instance )
{
	Pair *entry = registryLookup( ctx, "." );
	if (( instance )) {
		entry->value = instance;

		ActualizeData data;
		memset( &data, 0, sizeof(ActualizeData) );
		data.instance = instance;
		data.ctx = ctx;

		BMTraverseData traverse_data;
		traverse_data.user_data = &data;
		traverse_data.stack = &data.stack.flags;
		traverse_data.done = FILTERED;

		actualize_traversal( proto, &traverse_data, FIRST ); }
	else {
		entry->value = BMContextSelf( ctx ); }
#ifdef DEBUG
	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
#endif		
}

//---------------------------------------------------------------------------
//	actualize_traversal
//---------------------------------------------------------------------------
static void actualize_param( char *, CNInstance *, listItem *, BMContext * );

BMTraverseCBSwitch( actualize_traversal )
/*
	invokes actualize_param() on each .param found in expression, plus
	sets context's perso to context's (self,perso) in case of varargs.
	Note that we do not enter negated, starred or %(...) expressions
	Note also that the caller is expected to reorder the list of exponents
*/
case_( sub_expression_CB ) // provision
	_prune( BM_PRUNE_FILTER )
case_( dot_expression_CB )
	xpn_add( &data->exponent, SUB, 1 );
	_break
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( &data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = data->exponent;
	_break
case_( filter_CB )
	xpn_free( &data->exponent, data->level );
	_break
case_( decouple_CB )
	xpn_free( &data->exponent, data->level );
	xpn_set( data->exponent, SUB, 1 );
	_break
case_( close_CB )
	xpn_free( &data->exponent, data->level );
	if is_f( COUPLE )
		popListItem( &data->exponent );
	if is_f( DOT )
		popListItem( &data->exponent );
	data->level = popListItem( &data->stack.level );
	_break;
case_( wildcard_CB )
	if is_f( ELLIPSIS ) {
		listItem *exponent = data->exponent;
		union { void *ptr; int value; } exp;
		exp.ptr = exponent->ptr;
		if ((exp.value&1) && !exponent->next ) {
			BMContext *ctx = data->ctx;
			CNDB *db = BMContextDB( ctx );
			CNInstance *self = BMContextSelf( ctx );
			Pair *entry = registryLookup( ctx, "." );
			entry->value = db_instantiate( self, entry->value, db ); } }
	_break
case_( dot_identifier_CB )
	 actualize_param( p+1, data->instance, data->exponent, data->ctx );
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	actualize_param
//---------------------------------------------------------------------------
static inline int inand( listItem *i, int operand );

static void
actualize_param( char *p, CNInstance *instance, listItem *exponent, BMContext *ctx )
{
	// Special case: ( .identifier, ... )
	if ( inand( exponent, 1 ) ) {
		char *q = p;
		do q++; while ( !is_separator(*q) );
		if ( !strncmp(q,",...",4) ) {
			CNInstance *e;
			while (( e=CNSUB(instance,0) )) instance=e;
			registryRegister( ctx, p, instance );
			return; } }
	// General case
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=NULL; i=i->next )
		addItem( &xpn, i->ptr );
	int exp;
	while (( exp = pop_item( &xpn ) ))
		instance = instance->sub[ exp & 1 ];
	registryRegister( ctx, p, instance );
}

static inline int inand( listItem *i, int operand ) {
	if (( i )) {
		union { void *ptr; int value; } icast;
		icast.ptr = i->ptr; return !( icast.value & operand ); }
	return 0; }

//===========================================================================
//	bm_context_release
//===========================================================================
void
bm_context_release( BMContext *ctx )
/*
	Assumption: ActiveRV dealt with separately
*/
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
			Pair *mark;
			switch ( *name ) {
			case '?':
				stack = (listItem **) &entry->value;
				while (( mark = popListItem( stack ) ))
					freePair( mark );
				break;
			case '<':
				stack = (listItem **) &entry->value;
				while (( mark = popListItem( stack ) )) {
					freePair( mark->name );
					freePair( mark ); }
				break;
			case '|':
				stack = (listItem **) &entry->value;
				freeListItem( stack );
				break;
			case '.':
				entry->value = NULL;
				break; }
			last_i = i; } }
}

//===========================================================================
//	bm_mark
//===========================================================================
Pair *
bm_mark( int pre, char *expression, char *src, void *user_data )
/*
	return either one of the following from expression < src
	in case src==NULL // in_condition or on_event (pre==0)
		[ [ type, xpn ], [ user_data, NULL ] ]
	otherwise // on_event_x (pre==0 or pre==BM_AS_PER)
		[ [ type, xpn ], user_data ]
*/
{
	union { void *ptr; int value; } type;
	type.value = 0;
	listItem *xpn = NULL;

	if (( src ) && *src!='{' && ( bm_locate_mark(src,&xpn) ))
		type.value = EENOK; // Assumption: xpn==NULL
	else if ( !expression ) ;
	else if ( *expression==':' ) {
		char *value = p_prune( PRUNE_FILTER, expression+1 ) + 1;
		if ( !strncmp( value, "~.", 2 ) ) {
			// we know instances will be ( *, e )
			if (( bm_locate_mark( expression, &xpn ) ))
				type.value = EMARK|QMARK; }
		else if (( bm_locate_mark( value, &xpn ) ))
			// we know instances will be ((*,.), e )
			type.value = EMARK|QMARK;
		else if (( bm_locate_mark( expression, &xpn ) ))
			// we know instances will be ((*,e), . )
			type.value = EMARK; }
	else if (( bm_locate_mark( expression, &xpn ) ))
		type.value = QMARK;

	Pair *mark = NULL;
	if ( type.value ) {
		if ( pre ) type.value |= AS_PER;
		if (( xpn )) reorderListItem( &xpn );
		if (( src )) type.value |= EENOK;
		else user_data = newPair( user_data, NULL );
		mark = newPair( newPair( type.ptr, xpn ), user_data ); }

	return mark;
}

//===========================================================================
//	bm_context_mark / bm_context_unmark
//===========================================================================
static Pair * mark_prep( Pair *, CNInstance *, CNInstance * );
/*
	marked is either
	in case ( type & AS_PER )
		[ mark:[ type, xpn ], { batch:[ { instance(s) }, proxy ] } ]
	    or
		[ mark:[ type, xpn ], { batch:[ NULL, proxy ] } ]
	otherwise
		[ mark:[ type, xpn ], batch:[ instance, proxy ] ]
*/
void
bm_context_mark( BMContext *ctx, Pair *marked )
{
	Pair *batch, *event;
	CNInstance *instance, *proxy;
	if (( marked )) {
		Pair *mark = marked->name;
		union { void *ptr; int value; } type;
		type.ptr = mark->name;
		if ( type.value & AS_PER ) {
			batch = ((listItem *) marked->value )->ptr;
			instance = popListItem((listItem **) &batch->name );
			proxy = batch->value;
			if ( !batch->name ) {
				freePair( batch ); // one batch per proxy
				popListItem((listItem **) &marked->value ); } }
		else {
			batch = marked->value;
			instance = batch->name;
			proxy = batch->value;
			freePair( batch );
			marked->value = NULL; }

		event = mark_prep( mark, instance, proxy );
		if (( event )) {
			if (( proxy ))
				bm_push_mark( ctx, "<", newPair( event, proxy ) );
			else
				bm_push_mark( ctx, "?", event ); } }
}
Pair *
bm_context_unmark( BMContext *ctx, Pair *marked )
{
	if (( marked )) {
		Pair *mark = marked->name;
		union { void *ptr; int value; } type;
		type.ptr = mark->name;
		if ( type.value & EENOK )
			bm_pop_mark( ctx, "<" );
		else
			bm_pop_mark( ctx, "?" );
		if ( !marked->value ) {
			listItem *xpn = mark->value;
			freeListItem( &xpn );
			freePair( mark );
			freePair( marked );
			marked = NULL; } }
	return marked;
}

//---------------------------------------------------------------------------
//	mark_prep
//---------------------------------------------------------------------------
static inline CNInstance * xsub( CNInstance *, listItem * );

static Pair *
mark_prep( Pair *mark, CNInstance *x, CNInstance *proxy )
/*
	Assumption: mark is not NULL
	However: x may be NULL, in which case we have type.value==EENOK
	Use cases:	on . < ?
			on init < ?
			on exit < ?
	Note: we may have type.value==EENOK and x not NULL
*/
{
	union { void *ptr; int value; } type;
	type.ptr = mark->name;
	listItem *xpn = mark->value;

	CNInstance *y;
	if ( type.value & EENOK ) {
		y = ( type.value & EMARK ) ?
			( type.value & QMARK ) ?
				xsub( x->sub[1], xpn ) :
				xsub( x->sub[0]->sub[1], xpn ) :
			( type.value & QMARK ) ?
				xsub( x, xpn ) : proxy; }
	else if ( type.value & EMARK ) {
		y = ( type.value & QMARK ) ?
			xsub( x->sub[1], xpn ) :
			xsub( x->sub[0]->sub[1], xpn ); }
	else if ( type.value & QMARK ) {
		y = xsub( x, xpn ); }
	else {
		fprintf( stderr, ">>>>> B%%: Error: bm_context_mark: "
			"unknown mark type\n" );
		exit( -1 ); }

	return newPair( x, y );
}

static inline CNInstance * xsub( CNInstance *x, listItem *xpn ) {
	// Assumption: x.xpn exists by construction
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next ) {
		union { void *ptr; int value; } exp;
		exp.ptr = i->ptr;
		x = x->sub[ exp.value & 1 ]; }
	return x; }

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
bm_pop_mark( BMContext *ctx, char *p )
{
	Pair *entry = registryLookup( ctx, p );
	Pair *mark;
	if (( entry ))
		switch ( *p ) {
		case '?':
			mark = popListItem((listItem **) &entry->value );
			freePair( mark );
			break;
		case '<':
			mark = popListItem((listItem **) &entry->value );
			freePair( mark->name );
			freePair( mark );
			break;
		default:
			popListItem((listItem**) &entry->value ); }
}

//===========================================================================
//	bm_register / bm_lookup / bm_match
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
	else if (( ctx ) && !is_separator(*p) ) {
		// registering normal identifier instance
		Pair *entry = registryLookup( ctx, p );
		if (( entry )) return entry->value; }

	return db_register( p, db );
}

//---------------------------------------------------------------------------
//	bm_lookup
//---------------------------------------------------------------------------
CNInstance *
bm_lookup( BMContext *ctx, char *p, int privy, CNDB *db )
{
	switch ( *p ) {
	case '%':
		switch ( p[1] ) {
		case '?': return bm_context_lookup( ctx, "?" );
		case '!': return bm_context_lookup( ctx, "!" );
		case '%': return bm_context_lookup( ctx, "%" );
		case '<': return eenov_lookup( ctx, db, p );
		case '|': return bm_context_lookup( ctx, "|" );
		case '@': return bm_context_lookup( ctx, "@" ); }
		break;
	case '.':
		return bm_context_lookup( ctx, p );
	default:
		if ( !is_separator(*p) ) {
			// looking up normal identifier instance
			CNInstance *instance = bm_context_lookup( ctx, p );
			if (( instance )) return instance; } }

	// not found in ctx
	switch ( *p ) {
	case '\'': ; // looking up single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) )
			return db_lookup( privy, q.s, db );
		return NULL;
	default:
		return db_lookup( privy, p, db ); }
}

//---------------------------------------------------------------------------
//	bm_match
//---------------------------------------------------------------------------
static inline int ctx_match_v( BMContext *, CNDB *, char *, CNInstance *, CNDB * );

int
bm_match( BMContext *ctx, CNDB *db, char *p, CNInstance *x, CNDB *db_x )
{
	switch ( *p ) {
	case '%':
		switch ( p[1] ) {
		case '?': return db_match( x, db_x, bm_context_lookup(ctx,"?"), db );
		case '!': return db_match( x, db_x, bm_context_lookup(ctx,"!"), db );
		case '%': return db_match( x, db_x, bm_context_lookup(ctx,"%"), db );
		case '<': return eenov_match( ctx, p, x, db_x );
		case '@': return ctx_match_v( ctx, db, "@", x, db_x );
		case '|': return ctx_match_v( ctx, db, "|", x, db_x ); }
		break;
	case '.':
		return db_match( x, db_x, bm_context_lookup(ctx,p), db );
	default:
		if ( db==db_x && !is_separator(*p) ) {
			CNInstance *y = bm_context_lookup( ctx, p );
			if (( y )) return ( x==y ); } }

	// not found in ctx
	if ( !x->sub[0] ) {
		char *identifier = DBIdentifier( x, db_x );
		char_s q;
		switch ( *p ) {
		case '\'':
			if ( charscan( p+1, &q ) )
				return !strcomp( q.s, identifier, 1 );
			break;
		case '/':
			return !strcomp( p, identifier, 2 );
		default:
			return !strcomp( p, identifier, 1 ); } }

	return 0; // not a base entity
}

static inline int
ctx_match_v( BMContext *ctx, CNDB *db, char *v, CNInstance *x, CNDB *db_x )
{
	for ( listItem *i=bm_context_lookup( ctx, v ); i!=NULL; i=i->next )
		if ( db_match( x, db_x, i->ptr, db ) )
			return 1;
	return 0;
}

//===========================================================================
//	bm_context_register / bm_context_lookup
//===========================================================================
int
bm_context_register( BMContext *ctx, char *p )
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

void *
bm_context_lookup( BMContext *ctx, char *p )
{
	ActiveRV *active;
	Pair *entry;
	listItem *i;
	switch ( *p ) {
	case '?':
		entry = registryLookup( ctx, "?" );
		if (( entry ) && ( i=entry->value ))
			return ((Pair *) i->ptr )->value;
		break;
	case '!':
		entry = registryLookup( ctx, "?" );
		if (( entry ) && ( i=entry->value ))
			return ((Pair *) i->ptr )->name;
		break;
	case '%':
		return BMContextSelf( ctx );
	case '.':
		if ( p[1]=='.' )
			return BMContextParent( ctx );
		return BMContextPerso( ctx );
	case '<':
	case '|': ;
		entry = registryLookup( ctx, p );
		if (( entry ) && ( i=entry->value ))
			return i->ptr;
		break;
	case '@':
		active = BMContextActive( ctx );
		return active->value;
	default: // p==identifier
		entry = registryLookup( ctx, p );
		if (( entry )) 
			return entry->value; }
	return NULL;
}

//===========================================================================
//	bm_inform
//===========================================================================
static inline CNInstance * _inform( BMContext *, CNInstance *, CNDB * );

void *
bm_inform( int list, BMContext *dst, void *data, CNDB *db_src )
{
	if ( list ) {
		listItem **instances = data;
		listItem *results = NULL;
		for ( CNInstance *e;( e = popListItem(instances) ); ) {
			e = _inform( dst, e, db_src );
			if (( e )) addItem( &results, e ); }
		return results; }
	else
		return _inform( dst, data, db_src );
}

static inline CNInstance *proxy_that( CNEntity *, BMContext *, CNDB *, int );
static inline CNInstance *
_inform( BMContext *dst, CNInstance *e, CNDB *db_src )
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
//	bm_intake
//===========================================================================
CNInstance *
bm_intake( BMContext *dst, CNDB *db_dst, CNInstance *x, CNDB *db_x )
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

