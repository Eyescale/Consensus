#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "scour.h"
#include "locate_mark.h"
#include "eenov.h"
#include "instantiate.h"
#include "parser.h"
#include "errout.h"

#include "context.h"
#include "actualize_traversal.h"

//===========================================================================
//	newContext / freeContext
//===========================================================================
BMContext *
newContext( CNEntity *cell ) {
	CNDB *db = newCNDB( cell );
	CNInstance *self = new_proxy( NULL, cell, db );
	Pair *active = newPair( newPair( NULL, NULL ), NULL );
	Registry *locales = newRegistry( IndexedByNameRef );
	registryRegister( locales, ".", self );

	Registry *ctx = newRegistry( IndexedByNameRef );
	registryRegister( ctx, "", db ); // BMContextDB( ctx )
	registryRegister( ctx, "@", active ); // active connections (proxies)
	registryRegister( ctx, ".", newItem( locales ) ); // locales stack
	registryRegister( ctx, "^", NULL ); // aka. [ ^^, *^^ ]
	registryRegister( ctx, "?", NULL ); // aka. [ %!, %? ]
	registryRegister( ctx, "|", NULL ); // aka. %|
	registryRegister( ctx, "<", NULL ); // aka. [ [ %<!>, %<?> ], %< ]
	return ctx; }

static freeRegistryCB untag_CB;
void
freeContext( BMContext *ctx )
/*
	Assumptions - cf freeCell()
		. CNDB is free from any proxy and arena attachment
		. id Self and Parent proxies have been released
		. ctx registry all flushed out
*/ {
	// free active connections register
	ActiveRV *active = BMContextActive( ctx );
	freeListItem( &active->buffer->activated );
	freeListItem( &active->buffer->deactivated );
	freePair((Pair *) active->buffer );
	freeListItem( &active->value );
	freePair((Pair *) active );

	// free locales
	listItem *stack = registryLookup( ctx, "." )->value;
	freeRegistry( stack->ptr, NULL );
	freeItem( stack );

	// free CNDB
	CNDB *db = BMContextDB( ctx );
	db_arena_unref( db );
	freeCNDB( db );

	// last, but not least
	freeRegistry( ctx, untag_CB ); }

static void
untag_CB( Registry *registry, Pair *entry ) {
	if ( !is_separator( *(char *)entry->name ) ) {
		free( entry->name );
		freeListItem((listItem **) &entry->value ); } }

//===========================================================================
//	bm_context_init
//===========================================================================
static inline void update_active( ActiveRV * );

void
bm_context_init( BMContext *ctx ) {
	CNDB *db = BMContextDB( ctx );
	// integrate cell's init conditions
	db_init( db );
	// activate parent connection, if there is
	update_active( BMContextActive(ctx) ); }

static inline void
update_active( ActiveRV *active ) {
	CNInstance *proxy;
	listItem **current = &active->value;
	listItem **buffer = &active->buffer->deactivated;
	while (( proxy = popListItem(buffer) ))
		removeIfThere( current, proxy );
	buffer = &active->buffer->activated;
	while (( proxy = popListItem(buffer) ))
		addIfNotThere( current, proxy ); }

//===========================================================================
//	bm_context_update
//===========================================================================
static DBRemoveCB remove_CB;
typedef struct {
	CNInstance *parent;
	BMContext *ctx;
	} RemoveData;

int
bm_context_update( BMContext *ctx ) {
#ifdef DEBUG
	fprintf( stderr, "bm_context_update: bgn\n" );
#endif
	CNEntity *this = BMContextCell( ctx );
	CNDB *db = BMContextDB( ctx );
	CNInstance *proxy;

	// update active registry according to user request
	ActiveRV *active = BMContextActive( ctx );
	update_active( active );

	// fire - resp. deprecate dangling - connections
	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		db_fire( connection->as_sub[0]->ptr, db ); }

	// invoke db_update
	RemoveData data;
	data.parent = BMContextParent( ctx );
	data.ctx = ctx;
	db_update( db );

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
	return DBExitOn( db ); }

//===========================================================================
//	bm_context_actualize
//===========================================================================
/*
	invokes actualize_param() on each .param found in expression, plus
	sets context's perso to context's (self,perso) in case of varargs.
	Note that we do not enter negated, starred or %(...) expressions
*/
typedef struct {
	CNInstance *instance;
	BMContext *ctx;
	listItem *exponent, *level;
	struct { listItem *flags, *level; } stack;
	} ActualizeData;

void
bm_context_actualize( BMContext *ctx, char *proto, CNInstance *instance ) {
	if ( !proto || !instance ) {
		bm_context_rebase( ctx, instance ); return; }
	else if ( *proto==':' ) { // :func(_) or :
		proto = p_prune( PRUNE_IDENTIFIER, proto+1 );
		if ( !*proto ) {
			bm_context_rebase( ctx, instance );
			return; } }
	else bm_context_rebase( ctx, instance );

	ActualizeData data;
	memset( &data, 0, sizeof(ActualizeData) );
	data.instance = instance;
	data.ctx = ctx;
	
	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;
	traversal.done = FILTERED;
	
	actualize_traversal( proto, &traversal, FIRST );
	DBG_ACTUALIZE_TRAVERSAL }

//---------------------------------------------------------------------------
//	actualize_traversal
//---------------------------------------------------------------------------
static void actualize_param( char *, CNInstance *, listItem *, BMContext * );

BMTraverseCBSwitch( actualize_traversal )
case_( sub_expression_CB )
	_prune( BM_PRUNE_FILTER, p+1 )
case_( dot_expression_CB )
	xpn_add( &data->exponent, SUB, 1 );
	_break
case_( open_CB )
	if is_f_next( COUPLE )
		xpn_add( &data->exponent, SUB, 0 );
	addItem( &data->stack.level, data->level );
	data->level = data->exponent;
	_break
case_( filter_CB )
	xpn_free( &data->exponent, data->level );
	_break
case_( comma_CB )
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
#if 0 // requires more thinking
	if is_f( ELLIPSIS ) {
		listItem *xpn = data->exponent;
		if ((cast_i(xpn->ptr)&1) && !xpn->next ) { // B% vararg
			BMContext *ctx = data->ctx;
			bm_context_rebase( ctx, db_instantiate(
				BMContextSelf(ctx),
				BMContextPerso(ctx),
				BMContextDB(ctx))); } }
#endif
	_break
case_( dot_identifier_CB )
	 actualize_param( p+1, data->instance, data->exponent, data->ctx );
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	actualize_param
//---------------------------------------------------------------------------
#define inand( i, operand ) ((i) && !( operand & cast_i(i->ptr) ))

static void
actualize_param( char *p, CNInstance *instance, listItem *exponent, BMContext *ctx ) {
	// Special case: ( .identifier, ... )
	if ( inand( exponent, 1 ) ) {
		char *q = p;
		do q++; while ( !is_separator(*q) );
		if ( !strncmp(q,",...",4) ) {
			CNInstance *e;
			while (( e=CNSUB(instance,0) )) instance = e;
			goto RETURN; } }
	// General case
	listItem *xpn = NULL; // reorder exponent
	for ( listItem *i=exponent; i!=NULL; i=i->next )
		addItem( &xpn, i->ptr );
	for ( int exp;( exp=pop_item( &xpn ) );)
		instance = instance->sub[ exp& 1 ];
RETURN:
	registryRegister( BMContextCurrent(ctx), p, instance ); }

//===========================================================================
//	bm_context_release
//===========================================================================
static inline void locale_flush( listItem *stack );

void
bm_context_release( BMContext *ctx )
/*
	Assumption: ActiveRV and Pipe dealt with separately
*/ {
	Pair *mark;
	listItem **stack;
	Registry *locales;
	for ( listItem *i=ctx->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		char *name = entry->name;
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
		case '.':
			stack = (listItem **) &entry->value;
			while (( locales = popListItem( stack ) ))
				freeRegistry( locales, NULL );
			locales = newRegistry( IndexedByNameRef );
			registryRegister( locales, ".", BMContextSelf(ctx) );
			entry->value = newItem( locales );
			break; } } }

//===========================================================================
//	bm_mark
//===========================================================================
/*
	return mark:[ [ type, xpn ], match ] if found in expression < src
	Assumption: flags is a bitwise combination of AS_PER and EENOK
*/
BMMark *
bm_mark( char *expression, char *src, unsigned int flags, void *match ) {
	int type = 0;
	listItem *xpn = NULL;

	if (( src ) && *src!='{' && ( bm_locate_mark(0,src,&xpn) ))
		type = EENOK; // Assumption: xpn==NULL
	else if ( !expression ) ;
	else if ( *expression==':' ) {
		char *value = p_prune( PRUNE_FILTER, expression+1 );
		if ( *value++=='\0' ) { // move past ',' aka. ':' if there is
			// we know instance to be ((*,.),e)
			if (( bm_locate_mark( 0, expression, &xpn ) ))
				type = EMARK|QMARK; }
		else if ( !strncmp( value, "~.", 2 ) ) {
			// we know instances will be ( *, e )
			if (( bm_locate_mark( 0, expression, &xpn ) ))
				type = EMARK|QMARK; }
		else if (( bm_locate_mark( 0, value, &xpn ) ))
			// we know instances will be ((*,.), e )
			type = EMARK|QMARK;
		else if (( bm_locate_mark( 0, expression, &xpn ) ))
			// we know instances will be ((*,e), . )
			type = EMARK; }
	else if (( bm_locate_mark( 0, expression, &xpn ) ))
		type = QMARK;

	if (!((type|flags)&(QMARK|EMARK|EENOK|VMARK))) {
		// expression is not marked
		if ( !(flags&AS_PER) ) return NULL;
		addItem( &xpn, NULL );
		type |= flags|VMARK; }
	else {
		if (( xpn )) reorderListItem( &xpn );
		type |= flags; }

	Pair *mark = newPair( cast_ptr(type), xpn );
	return (BMMark *) newPair( mark, match ); }

BMMark *
bm_lmark( Pair *match ) {
	Pair *mark = newPair( cast_ptr(LMARK), NULL );
	return (BMMark *) newPair( mark, match ); }

void
bm_vmark( BMContext *ctx, char *vm, BMMark *mark ) {
	bm_context_push( ctx, ".", NULL );
	Registry *locales = BMContextCurrent( ctx );
	Pair *entry = registryRegister( locales, strdup(vm), NULL );
	addItem( &mark->data->xpn, entry ); }

//---------------------------------------------------------------------------
//	bm_context_mark / bm_context_unmark / bm_context_check
//---------------------------------------------------------------------------
/*
	BMMark can be - depending on type=mark->data->type
	if (( type&EENOK )&&( type&AS_PER )) // <<<< per expression < src >>>>
	 either	[ data:[ type, xpn ], match.list:{ batch:[ { instance(s) }, proxy ] } ]
	    or  [ data:[ type, xpn ], match.list:{ batch:[ NULL, proxy ] } ]
	else if ( type&EENOK ) // <<<< on expression < src >>>>
		[ data:[ type, xpn ], match.record:[ instance, proxy ] ]
	else if ( type&AS_PER ) // <<<< per expression >>>>
		[ data:[ type, xpn ], match.list:{ instance(s) } ]
	else // <<<< on expression >>>>
		[ data:[ type, xpn ], match.record: instance ]
*/
static inline Pair * mark_pop( BMMark *, int type );

void
bm_context_mark( BMContext *ctx, BMMark *mark ) {
	if ( !mark ) return;
	int type = cast_i( mark->data->type );
	if ( type==LMARK ) {
		Pair *lmark = mark->match.record;
		bm_context_push( ctx, "^", lmark );
		return; }
	Pair *markv = mark_pop( mark, type );
	if ( type&EENOK ) {
		bm_context_push( ctx, "<", markv );
		if ( type&VMARK ) {
			Pair *vm = mark->data->xpn->ptr;
			if (( vm )) vm->value = markv->value; } }
	else if ( type&(EMARK|QMARK) ) {
		bm_context_push( ctx, "?", markv );
		if ( type&VMARK ) {
			Pair *vm = mark->data->xpn->ptr;
			if (( vm )) vm->value = markv->value; } }
	else if ( type&VMARK ) {
		Pair *vm = mark->data->xpn->ptr;
		if (( vm )) vm->value = markv; } }

BMMark *
bm_context_unmark( BMContext *ctx, BMMark *mark ) {
	if ( !mark ) return NULL;
	int type = cast_i( mark->data->type );
	if ( type==LMARK ) {
		bm_context_pop( ctx, "^" );
		freePair( mark->match.record );
		freePair((Pair *) mark->data );
		freePair((Pair *) mark );
		return NULL; }
	if ( type&EENOK )
		bm_context_pop( ctx, "<" );
	else if ( type&(EMARK|QMARK) )
		bm_context_pop( ctx, "?" );
	if ( !mark->match.record ) {
		listItem **xpn = &mark->data->xpn;
		if ( type&VMARK ) {
			Pair *vm = (*xpn)->ptr;
			if (( vm )) free( vm->name );
			bm_context_pop( ctx, "." ); }
		freeListItem( xpn );
		freePair((Pair *) mark->data );
		freePair((Pair *) mark );
		return NULL; }
	return mark; }

int
bm_context_check( BMContext *ctx, BMMark *mark )
/*
	Assumption: mark type = AS_PER|QMARK - instantiate.c:loop_CB() specific
	recycle mark if feasible, unmark otherwise
*/ {
	int type = cast_i( mark->data->type );
	if (( mark->match.record )) {
		Pair *markv = mark_pop( mark, type );
		bm_context_take( ctx, "?", markv );
		return 1; }
	else {
		bm_context_pop( ctx, "?" );
		freeListItem( &mark->data->xpn );
		freePair((Pair *) mark->data );
		freePair((Pair *) mark );
		return 0; } }

//---------------------------------------------------------------------------
//	mark_pop
//---------------------------------------------------------------------------
/*
	Assumption: mark is not NULL
	However:
	. x may be NULL, in which case we have type.value==EENOK
	  Use cases:	on . < ?
			on init < ?
			on exit < ?
	. we may have type.value==EENOK and x not NULL
*/
static inline CNInstance * mark_sub( CNInstance *, listItem * );

static inline Pair *
mark_pop( BMMark *mark, int type ) {
	CNInstance *x, *y, *proxy=NULL;
	if ( type&EENOK ) {
		if ( type&AS_PER ) {
			Pair *batch = mark->match.list->ptr;
			x = popListItem((listItem **) &batch->name );
			proxy = batch->value;
			if ( !batch->name ) {
				freePair( batch ); // one batch per proxy
				popListItem( &mark->match.list ); } }
		else {
			Pair *record = mark->match.record;
			x = record->name;
			proxy = record->value;
			freePair( record );
			mark->match.record = NULL; } }
	else if ( type&AS_PER )
		x = popListItem( &mark->match.list );
	else {
		x = (CNInstance *) mark->match.record;
		mark->match.record = NULL; }

	if ( type&(EMARK|QMARK|EENOK) ) {
		listItem *xpn = mark->data->xpn;
		if ( type & VMARK ) xpn = xpn->next;
		y = ( type & EMARK ) ?
			( type & QMARK ) ?
				mark_sub( x->sub[1], xpn ) :
				mark_sub( x->sub[0]->sub[1], xpn ) :
			( type & QMARK ) ?
				mark_sub( x, xpn ) : proxy;
		if ( !y ) {
			errout( ContextMarkType );
			exit( -1 ); }
		Pair *event = newPair( x, y );
		return type&EENOK ? newPair(event,proxy) : event; }
	else // assuming VMARK
		return (Pair *) x; }

static inline CNInstance *
mark_sub( CNInstance *x, listItem *xpn ) {
	// Assumption: x.xpn exists by construction
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next )
		x = x->sub[ cast_i(i->ptr) & 1 ];
	return x; }

//===========================================================================
//	bm_context_push / bm_context_pop / bm_context_take
//===========================================================================
Pair *
bm_context_push( BMContext *ctx, char *p, void *value ) {
	Pair *entry = registryLookup( ctx, p );
	if (( entry ))
		switch ( *p ) {
		case '.': ;
			Registry *locales = newRegistry( IndexedByNameRef );
			if (( value )) registryRegister( locales, ".", value );
			addItem((listItem **) &entry->value, locales );
			break;
		default:
			addItem((listItem **) &entry->value, value ); }
	return entry; }

void *
bm_context_pop( BMContext *ctx, char *p ) {
	Pair *entry = registryLookup( ctx, p );
	if (( entry ))
		switch ( *p ) {
		case '.':
			entry = popListItem((listItem **) &entry->value );
			freeRegistry((Registry *) entry, NULL );
			return NULL;
		default: ;
			Pair *previous = popListItem((listItem **) &entry->value );
			switch ( *p ) {
			case '<': freePair( previous->name ); // no break
			case '?': freePair( previous ); // no break
			default: return previous; } }
	return NULL; }

listItem *
bm_context_take( BMContext *ctx, char *p, void *value ) {
	Pair *entry = registryLookup( ctx, p );
	if (( entry )) {
		listItem *stack = entry->value;
		Pair *previous = stack->ptr;
		switch ( *p ) {
		case '|':
			stack->ptr = value;
			return (listItem *) previous;
		case '<':
			freePair( previous->name ); // no break
		case '?':
			freePair( previous ); // no break
		default:
			stack->ptr = value; } }
	return NULL; }

void
bm_context_debug( BMContext *ctx, char *p ) {
	Pair *entry = registryLookup( ctx, p );
	CNDB *db = BMContextDB( ctx );
	if (( entry )) {
		listItem *stack = entry->value;
		for ( listItem *i=stack->ptr; i!=NULL; i=i->next ) {
			CNInstance *e = i->ptr;
			db_outputf( db, stderr, "%_, ", e ); } }
		fprintf( stderr, "\n" ); }

//===========================================================================
//	bm_lookup
//===========================================================================
static inline void * lookup_rv( BMContext *, char *, int *rv );

void *
bm_lookup( int privy, char *p, BMContext *ctx, CNDB *db )
/*
	Assumption: only used directly by query.c:pivot_query()
	otherwise as BMContextRVV => rv or locales only
*/ {
	// lookup first in ctx registry
	int rv = 1;
	void *rvv = lookup_rv( ctx, p, &rv );
	switch ( rv ) {
	case 0: break; // not a register variable
	case 2: return eenov_lookup( ctx, db, p );
	default: return rvv; }

	// not a register variable
	if ( !strncmp( p, "(:", 2 ) ) {
		CNInstance *star = db_lookup( 0, "*", db );
		CNInstance *self = BMContextSelf( ctx );
		return (( star ) ? cn_instance( star, self, 0 ) : NULL ); }
	else if ( !is_separator( *p ) ) {
		Pair *entry = BMLocaleEntry( ctx, p );
		if (( entry )) return entry->value; }

	// not found in ctx registry
	if (( db )) {
		switch ( *p ) {
		case '/': return db_rxscan( privy, p, db );
		case '\'': // looking up single character identifier instance
			for ( char_s q; charscan( p+1, &q ); )
				return db_lookup( privy, q.s, db );
			break;
		default:
			return db_lookup( privy, p, db ); } }
	return NULL; }

static inline void *
lookup_rv( BMContext *ctx, char *p, int *rv ) {
	Pair *entry;
	listItem *i;
	switch ( *p ) {
	case '^':
		switch ( p[1] ) {
		case '^':
			entry = registryLookup( ctx, "^" );
			if (( i=entry->value ))
				return ((Pair *) i->ptr )->name;
			return NULL;
		case '.':
			entry = registryLookup( ctx, "~" );
			return ((entry) ? entry->value : NULL ); }
		break;
	case '*':
		if ( !strncmp(p+1,"^^",2) ) {
			entry = registryLookup( ctx, "^" );
			if (( i=entry->value ))
				return ((Pair *) i->ptr )->value;
			return NULL; }
		break;
	case '%':
		switch ( p[1] ) {
		case '?':
		case '!':
			entry = registryLookup( ctx, "?" );
			if (!( i=entry->value )) return NULL;
			CNInstance *e = p[1]=='!' ?
				((Pair *) i->ptr )->name :
				((Pair *) i->ptr )->value;
			if ( !strncmp(p+2,"::",2) ) {
				listItem *xpn = xpn_make( p+4 );
				e = xpn_sub( e, xpn );
				freeListItem( &xpn ); }
			return e;
		case '%':
			return BMContextSelf( ctx );
		case '|':
			// Assumption: never from bm_match
			entry = registryLookup( ctx, "|" );
			if (( i=entry->value ))
				return i->ptr;
			return NULL;
		case '<':
			*rv = 2;
			return NULL;
		case '@':
			*rv = 3;
			return BMContextActive( ctx )->value;
		default:
			if ( !is_separator( p[1] ) ) {
				*rv = 3;
				entry = registryLookup( ctx, p+1 );
				return (( entry )? entry->value : NULL ); } }
		break;
	case '.':
		return ( p[1]=='.' ) ?
			BMContextParent( ctx ) :
			BMContextPerso( ctx ); }

	*rv = 0; // not a register variable
	return NULL; }

//===========================================================================
//	bm_match
//===========================================================================
int
bm_match( BMContext *ctx, CNDB *db, char *p, CNInstance *x, CNDB *db_x ) {
	// lookup & match first in ctx registry
	listItem *xpn;
	int rv = 1;
	void *rvv = lookup_rv( ctx, p, &rv );
	switch ( rv ) {
	case 0: break; // not a register variable
	case 1: return db_match( x, db_x, rvv, db );
	case 2: return eenov_match( ctx, p, x, db_x );
	case 3: for ( listItem *i=rvv; i!=NULL; i=i->next )
			if ( db_match( x, db_x, i->ptr, db ) )
				return 1;
		return 0; }

	// not a register variable
	if ( !strncmp(p,"!!",2) )
		return cnIsShared(x) && cnIsUnnamed(x);
	else if ( !strncmp( p, "(:", 2 ) )
		return ( db==db_x ?
			cnStarMatch(x->sub[ 0 ]) && x->sub[ 1 ]==BMContextSelf(ctx) :
			!!errout( ContextEENOSelfAssignment, p ) );
	else if ( db==db_x && !is_separator( *p ) ) {
		Pair *entry = BMLocaleEntry( ctx, p );
		if (( entry )) return x==entry->value; }

	// not found in ctx registry
	return db_match_identifier( x, p ); }

//===========================================================================
//	bm_register / bm_register_locales
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNDB *db )
/*
	Special case: ctx==NULL => p assumed identifier
*/ {
	if ( *p == '\'' ) {
		// registering single character identifier instance
		for ( char_s q; charscan( p+1, &q ); )
			return db_register( q.s, db );
		return NULL; }
	else if (( ctx ) && !is_separator(*p) ) {
		Pair *entry = BMLocaleEntry( ctx, p );
		if (( entry )) return entry->value; }

	return db_register( p, db ); }

int
bm_register_locales( BMContext *ctx, char *p ) {
	Registry *locales = BMContextCurrent( ctx );
	CNInstance *perso = BMContextPerso( ctx );
	CNDB *db = BMContextDB( ctx );
	while ( *p=='.' ) {
		p++;
		Pair *entry = registryLookup( locales, p );
		if (( entry )) { // already registered
			CNInstance *instance = entry->value;
			errout( instance->sub[0]==perso ?
				ContextLocaleMultiple : ContextLocaleConflict, p );
			exit( -1 ); }
		else {
			CNInstance *x = db_register( p, db );
			x = db_instantiate( perso, x, db );
			registryRegister( locales, p, x ); }

		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++; }
	return 1; }

//===========================================================================
//	bm_inform / bm_translate
//===========================================================================
static inline CNInstance *proxy_that( CNEntity *, BMContext *, CNDB *, int );

listItem *
bm_inform( BMContext *dst, listItem **list, CNDB *db_src ) {
	if ( !dst || !*list ) return *list;
	listItem *results = NULL;
	for ( CNInstance *e; ( e=popListItem(list) ); ) {
		e = bm_translate( dst, e, db_src, 1 );
		if (( e )) addItem( &results, e ); }
	return results; }

CNInstance *
bm_translate( BMContext *dst, CNInstance *x, CNDB *db_x, int inform ) {
	if ( !dst ) return x;
	if ( !x ) return NULL;
	CNDB *db_dst = BMContextDB( dst );
	if ( db_dst==db_x && ( !inform || !db_deprecated(x,db_x) ))
		return x;

	struct { listItem *src, *dst; } stack = { NULL, NULL };
	CNInstance *instance, *e = x;
	int ndx = 0;
	for ( ; ; ) {
		if (( CNSUB(e,ndx) )) {
			add_item( &stack.src, ndx );
			addItem( &stack.src, e );
			e = e->sub[ ndx ];
			ndx = 0; continue; }
		if ( cnIsProxy(e) ) { // proxy e:(( this, that ), NULL )
			CNEntity *that = CNProxyThat( e );
			instance = proxy_that( that, dst, db_dst, inform ); }
		else if ( cnIsShared(e) )
			instance = db_arena_translate( e, db_dst, inform );
		else {
			char *p = CNIdentifier( e );
			instance = inform ?
				db_register( p, db_dst ) :
				db_lookup( 0, p, db_dst ); }
		if ( !instance ) break;
		for ( ; ; ) {
			if ( !stack.src ) return instance;
			e = popListItem( &stack.src );
			if (( ndx = pop_item( &stack.src ) )) {
				CNInstance *f = popListItem( &stack.dst );
				instance = inform ?
					db_instantiate( f, instance, db_dst ) :
					cn_instance( f, instance, 0 );
				if ( !instance ) goto FAIL; }
			else {
				addItem( &stack.dst, instance );
				ndx=1; break; } } }
FAIL:
	freeListItem( &stack.src );
	freeListItem( &stack.dst );
	if ( inform ) errout( CellInform, db_x, x );
	return NULL; }

static inline CNInstance *
proxy_that( CNEntity *that, BMContext *ctx, CNDB *db, int inform )
/*
	look for proxy: (( this, that ), NULL ) where
		this = BMContextCell( ctx )
*/ {
	if ( !that ) return NULL;

	CNEntity *this = BMContextCell( ctx );
	if ( this==that )
		return BMContextSelf( ctx );

	for ( listItem *i=this->as_sub[0]; i!=NULL; i=i->next ) {
		CNEntity *connection = i->ptr;
		// Assumption: there is at most one ((this,~NULL), . )
		if ( connection->sub[ 1 ]==that )
			return connection->as_sub[ 0 ]->ptr; }

	return ( inform ? new_proxy(this,that,db) : NULL ); }

//===========================================================================
//	bm_tag, bm_untag
//===========================================================================
// bm_tag: see include/context.h
void
bm_untag( BMContext *ctx, char *tag ) {
	registryCBDeregister( ctx, untag_CB, tag ); }

