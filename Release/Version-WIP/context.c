#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "context.h"
#include "scour.h"
#include "locate_mark.h"
#include "eenov.h"
#include "instantiate.h"
#include "parser.h"

//===========================================================================
//	actualize_traversal
//===========================================================================
/*
	invokes actualize_param() on each .param found in expression, plus
	sets context's perso to context's (self,perso) in case of varargs.
	Note that we do not enter negated, starred or %(...) expressions
*/
#include "actualize_traversal.h"
typedef struct {
	CNInstance *instance;
	BMContext *ctx;
	listItem *exponent, *level;
	struct { listItem *flags, *level; } stack;
	} ActualizeData;

static void actualize_param( char *, CNInstance *, listItem *, BMContext * );

BMTraverseCBSwitch( actualize_traversal )
case_( sub_expression_CB ) // provision
	_prune( BM_PRUNE_FILTER, p+1 )
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
		if ((cast_i(exponent->ptr)&1) && !exponent->next ) { // vararg
			context_rebase( data->ctx ); } }
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
	registryRegister( BMContextLocales(ctx), p, instance ); }

//===========================================================================
//	bm_context_actualize
//===========================================================================
void
bm_context_actualize( BMContext *ctx, char *proto, CNInstance *instance ) {
	Pair *current = BMContextCurrent( ctx );
	if (( instance )) {
		current->name = instance;

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
		current->name = BMContextSelf( ctx ); }

#ifdef DEBUG
	freeListItem( &data.stack.flags );
	freeListItem( &data.stack.level );
#endif		
	}

//===========================================================================
//	bm_context_load
//===========================================================================
static int load_CB( BMParseOp, BMParseMode, void * );

int
bm_context_load( BMContext *ctx, char *path ) {
	FILE *stream = fopen( path, "r" );
	if ( !stream ) {
		fprintf( stderr, "B%%: Error: no such file or directory: '%s'\n", path );
		return -1; }
	CNIO io;
	io_init( &io, stream, path, IOStreamFile );

	BMParseData data;
	memset( &data, 0, sizeof(BMParseData) );
        data.io = &io;
	data.ctx = ctx;
	bm_parse_init( &data, BM_LOAD );
	//-----------------------------------------------------------------
	int event = 0;
	do {	event = io_read( &io, event );
		if ( io.errnum ) data.errnum = io_report( &io );
		else data.state = bm_parse_load( event, BM_LOAD, &data, load_CB );
		} while ( strcmp( data.state, "" ) && !data.errnum );
	//-----------------------------------------------------------------
	bm_parse_exit( &data );
	io_exit( &io );

	fclose( stream );
	return data.errnum; }

static int
load_CB( BMParseOp op, BMParseMode mode, void *user_data ) {
	BMParseData *data = user_data;
	if ( op==ExpressionTake ) {
		char *expression = StringFinish( data->string, 0 );
		bm_instantiate( expression, data->ctx, NULL );
		StringReset( data->string, CNStringAll ); }
	return 1; }

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
int
bm_context_update( BMContext *ctx, CNStory *story ) {
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
	CNInstance *parent = BMContextParent( ctx );
	db_update( db, parent, story->arena );

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
//	bm_context_release
//===========================================================================
static inline void flush_perso( listItem * );

void
bm_context_release( BMContext *ctx )
/*
	Assumption: ActiveRV dealt with separately
*/ {
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
				flush_perso( entry->value );
				break; }
			last_i = i; } } }

static inline void
flush_perso( listItem *stack ) {
	Pair *current = stack->ptr;
	for ( listItem *i=stack->next; i!=NULL; i=i->next ) {
		Pair *next = i->ptr;
		freeRegistry((Registry *) next->value, NULL );
		freePair( next ); }
	freeListItem( &stack->next );
	freeRegistry((Registry *) current->value, NULL );
	current->value = newRegistry( IndexedByCharacter ); }

//===========================================================================
//	bm_mark
//===========================================================================
/*
	return mark:[ type, xpn ] if found in expression < src
*/
Pair *
bm_mark( char *expression, char *src ) {
	int type = 0;
	listItem *xpn = NULL;

	if (( src ) && *src!='{' && ( bm_locate_mark(src,&xpn) ))
		type = EENOK; // Assumption: xpn==NULL
	else if ( !expression ) ;
	else if ( *expression==':' ) {
		char *value = p_prune( PRUNE_FILTER, expression+1 );
		if ( *value++=='\0' ) { // move past ',' aka. ':' if there is
			// we know instance to be ((*,.),e)
			if (( bm_locate_mark( expression, &xpn ) ))
				type = EMARK|QMARK; }
		else if ( !strncmp( value, "~.", 2 ) ) {
			// we know instances will be ( *, e )
			if (( bm_locate_mark( expression, &xpn ) ))
				type = EMARK|QMARK; }
		else if (( bm_locate_mark( value, &xpn ) ))
			// we know instances will be ((*,.), e )
			type = EMARK|QMARK;
		else if (( bm_locate_mark( expression, &xpn ) ))
			// we know instances will be ((*,e), . )
			type = EMARK; }
	else if (( bm_locate_mark( expression, &xpn ) ))
		type = QMARK;

	Pair *mark = NULL;
	if ( type ) {
		if (( xpn )) reorderListItem( &xpn );
		mark = newPair( cast_ptr(type), xpn ); }

	return mark; }

//===========================================================================
//	bm_context_mark / bm_context_unmark
//===========================================================================
/*
	MarkData can be - depending on type=data->mark->type
	if (( type&EENOK )&&( type&AS_PER )) // <<<< per expression < src >>>>
	 either	[ mark:[ type, xpn ], match.list:{ batch:[ { instance(s) }, proxy ] } ]
	    or  [ mark:[ type, xpn ], match.list:{ batch:[ NULL, proxy ] } ]
	else if ( type&EENOK ) // <<<< on expression < src >>>>
		[ mark:[ type, xpn ], match.record:[ instance, proxy ] ]
	else if ( type&AS_PER ) // <<<< per expression >>>>
		[ mark:[ type, xpn ], match.list:{ instance(s) } ]
	else // <<<< on expression >>>>
		[ mark:[ type, xpn ], match.list: instance ]
*/
static inline Pair * mark_pair( Pair *, CNInstance *, CNInstance * );

void
bm_context_mark( BMContext *ctx, MarkData *data ) {
	CNInstance *instance, *proxy;
	if (( data )) {
		int type = cast_i( data->mark->type );
		if (( type&EENOK )&&( type&AS_PER )) {
			Pair *batch = data->match.list->ptr;
			proxy = batch->value;
			instance = popListItem((listItem **) &batch->name );
			if ( !batch->name ) {
				freePair( batch ); // one batch per proxy
				popListItem( &data->match.list ); } }
		else if ( type&EENOK ) {
			Pair *record = data->match.record;
			instance = record->name;
			proxy = record->value;
			freePair( record );
			data->match.record = NULL; }
		else if ( type&AS_PER )
			instance = popListItem( &data->match.list );
		else {
			instance = (CNInstance *) data->match.list;
			data->match.record = NULL; }

		Pair *event = mark_pair((Pair *) data->mark, instance, proxy );
		if (( event )) {
			if ( type & EENOK )
				bm_push_mark( ctx, "<", newPair( event, proxy ) );
			else {	bm_push_mark( ctx, "?", event ); } } } }

MarkData *
bm_context_unmark( BMContext *ctx, MarkData *data ) {
	if (( data )) {
		int type = cast_i( data->mark->type );
		bm_pop_mark( ctx, ( type&EENOK ? "<" : "?" ));
		if ( !data->match.record ) {
			freeListItem( &data->mark->xpn );
			freePair((Pair *) data->mark );
			freePair((Pair *) data );
			data = NULL; } }
	return data; }

//---------------------------------------------------------------------------
//	mark_pair
//---------------------------------------------------------------------------
/*
	Assumption: mark is not NULL
	However: x may be NULL, in which case we have type.value==EENOK
	Use cases:	on . < ?
			on init < ?
			on exit < ?
	Note: we may also have type.value==EENOK and x not NULL
*/
static inline CNInstance * mark_sub( CNInstance *, listItem * );

static inline Pair *
mark_pair( Pair *mark, CNInstance *x, CNInstance *proxy ) {
	int type = cast_i( mark->name );
	listItem *xpn = mark->value;
	CNInstance *y;
	if ( type & EENOK ) {
		y = ( type & EMARK ) ?
			( type & QMARK ) ?
				mark_sub( x->sub[1], xpn ) :
				mark_sub( x->sub[0]->sub[1], xpn ) :
			( type & QMARK ) ?
				mark_sub( x, xpn ) : proxy; }
	else if ( type & EMARK ) {
		y = ( type & QMARK ) ?
			mark_sub( x->sub[1], xpn ) :
			mark_sub( x->sub[0]->sub[1], xpn ); }
	else if ( type & QMARK ) {
		y = mark_sub( x, xpn ); }
	else {
		fprintf( stderr, ">>>>> B%%: Error: bm_context_mark: "
			"unknown mark type\n" );
		exit( -1 ); }

	return newPair( x, y ); }

static inline CNInstance * mark_sub( CNInstance *x, listItem *xpn ) {
	// Assumption: x.xpn exists by construction
	if ( !xpn ) return x;
	for ( listItem *i=xpn; i!=NULL; i=i->next )
		x = x->sub[ cast_i(i->ptr) & 1 ];
	return x; }

//===========================================================================
//	bm_push_mark / bm_pop_mark / bm_reset_mark
//===========================================================================
listItem *
bm_push_mark( BMContext *ctx, char *mark, void *value ) {
	Pair *entry = registryLookup( ctx, mark );
	if (( entry ))
		return addItem((listItem **) &entry->value, value );
	return NULL; }

void
bm_pop_mark( BMContext *ctx, char *p ) {
	Pair *entry = registryLookup( ctx, p );
	if (( entry )) {
		Pair *mark = popListItem((listItem **) &entry->value );
		switch ( *p ) {
		case '<':
			freePair( mark->name );
			// no break
		case '?':
			freePair( mark );
			// no break
		default:
			break; } } }

void
bm_reset_mark( BMContext *ctx, char *p, void *value ) {
	Pair *entry = registryLookup( ctx, p );
	if (( entry )) {
		Pair *mark = entry->value;
		switch ( *p ) {
		case '<':
			freePair( mark->name );
			// no break
		case '?':
			freePair( mark );
			// no break
		default:
			entry->value = value; } } }

//===========================================================================
//	bm_lookup / lookup_mark
//===========================================================================
static inline void * lookup_mark( BMContext *, char *, int *rv );

void *
bm_lookup( BMContext *ctx, char *p, CNDB *db, int privy ) {
	// lookup first in ctx registry
	int rv = 1;
	void *rvv = lookup_mark( ctx, p, &rv );
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
		Registry *locales = BMContextLocales( ctx );
		Pair *entry = registryLookup( locales, p );
		if (( entry )) return entry->value; }

	// not found in ctx registry
	if (( db )) {
		switch ( *p ) {
		case '\'': ; // looking up single character identifier instance
			for ( char_s q; charscan( p+1, &q ); )
				return db_lookup( privy, q.s, db );
			return NULL;
		default:
			return db_lookup( privy, p, db ); } }
	return NULL; }

static inline void *
lookup_mark( BMContext *ctx, char *p, int *rv ) {
	Pair *entry;
	listItem *i;
	switch ( *p ) {
	case '%':
		switch ( p[1] ) {
		case '?':
			entry = registryLookup( ctx, "?" );
			if (( i=entry->value ))
				return ((Pair *) i->ptr )->value;
			else {	return NULL; }
		case '!':
			entry = registryLookup( ctx, "?" );
			if (( i=entry->value ))
				return ((Pair *) i->ptr )->name;
			else {	return NULL; }
		case '%':
			return BMContextSelf( ctx );
		case '<':
			*rv = 2;
			return NULL;
		case '@':
			*rv = 3;
			return BMContextActive( ctx )->value;
		case '|': ;
			entry = registryLookup( ctx, "|" );
			if (( i=entry->value )) {
				*rv = 3;
				return i->ptr; }
			else {	return NULL; } }
		break;
	case '^':
		if ( p[1]=='^' ) {
			entry = registryLookup( ctx, "^^" );
			if (( entry )) {
				return ((Pair *)(entry->value))->name; }
			else {	return NULL; }
		break;
	case '*':
		if ( p[1]!='^' ) break;
		switch ( p[2] ) {
		case '^':
			entry = registryLookup( ctx, "^^" );
			if (( entry )) {
				return ((Pair *)(entry->value))->value; }
			else {	return NULL; }
		case '?':
			p+=3; // skip '*^?'
			entry = registryLookup( ctx, "?" );
			if (!( i=entry->value )) return NULL;
			CNInstance *e = ((Pair *) i->ptr )->value;
			entry = registryLookup( ctx, "^*" );
			if ( !entry ) return NULL;
			Registry *buffer = entry->value;
			entry = registryLookup( buffer, e );
			if ( !entry ) return NULL;
			if ( *p==':' ) {
				listItem *xpn = subx( p+1 );
				e = xsub( e, xpn );
				freeListItem( &xpn ); }
			return e; }
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
	int rv = 1;
	void *rvv = lookup_mark( ctx, p, &rv );
	switch ( rv ) {
	case 0: break; // not a register variable
	case 1: return db_match( x, db_x, rvv, db );
	case 2: return eenov_match( ctx, p, x, db_x );
	case 3: for ( listItem *i=rvv; i!=NULL; i=i->next )
			if ( db_match( x, db_x, i->ptr, db ) )
				return 1;
		return 0; }

	// not a register variable
	if ( !strncmp( p, "(:", 2 ) ) {
		if ( db==db_x )
			return DBStarMatch( x->sub[ 0 ] ) &&
				x->sub[ 1 ]==BMContextSelf( ctx );
		else
			fprintf( stderr, ">>>>> B%%::Warning: Self-assignment\n"
				"\t\t(%s\n\t<<<<< not supported in EENO\n", p );
		return 0; }
	else if ( db==db_x && !is_separator( *p ) ) {
		Registry *locales = BMContextLocales( ctx );
		Pair *entry = registryLookup( locales, p );
		if (( entry )) return x==entry->value; }

	// not found in ctx registry
	if (( x->sub[0] )) return 0; // not a base entity
	char *identifier = DBIdentifier( x );
	switch ( *p ) {
		case '\'':
			for ( char_s q; charscan( p+1, &q ); )
				return !strcomp( q.s, identifier, 1 );
			return 0;
		case '/': return !strcomp( p, identifier, 2 );
		default: return !strcomp( p, identifier, 1 ); } }

//===========================================================================
//	bm_register / bm_register_locale
//===========================================================================
CNInstance *
bm_register( BMContext *ctx, char *p, CNDB *db )
/*
	Special case: ctx==NULL => p assumed identifier
*/ {
	if ( *p == '\'' ) {
		// registering single character identifier instance
		char_s q;
		if ( charscan( p+1, &q ) ) {
			return db_register( q.s, db ); }
		return NULL; }
	else if (( ctx ) && !is_separator(*p) ) {
		Registry *locales = BMContextLocales( ctx );
		Pair *entry = registryLookup( locales, p );
		if (( entry )) return entry->value; }

	return db_register( p, db ); }

int
bm_register_locale( BMContext *ctx, char *p ) {
	Pair *entry = BMContextCurrent( ctx );
	CNInstance *perso = entry->name;
	Registry *locales = entry->value;
	CNDB *db = BMContextDB( ctx );
	while ( *p=='.' ) {
		p++;
		entry = registryLookup( locales, p );
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
			registryRegister( locales, p, x ); }

		p = p_prune( PRUNE_IDENTIFIER, p );
		while ( *p==' ' || *p=='\t' ) p++; }
	return 1; }

//===========================================================================
//	newContext / freeContext
//===========================================================================
BMContext *
newContext( CNEntity *cell ) {
	CNDB *db = newCNDB();
	CNInstance *self = new_proxy( NULL, cell, db );
	Pair *id = newPair( self, NULL );
	Pair *active = newPair( newPair( NULL, NULL ), NULL );
	Pair *perso = newPair( self, newRegistry(IndexedByCharacter) );

	Registry *ctx = newRegistry( IndexedByCharacter );
	registryRegister( ctx, "", db ); // BMContextDB( ctx )
	registryRegister( ctx, "%", id ); // aka. %% and ..
	registryRegister( ctx, "@", active ); // active connections (proxies)
	registryRegister( ctx, ".", newItem( perso ) ); // perso stack
	registryRegister( ctx, "?", NULL ); // aka. %?
	registryRegister( ctx, "|", NULL ); // aka. %|
	registryRegister( ctx, "<", NULL ); // aka. %<?> %<!> %<
	return ctx; }

void
freeContext( BMContext *ctx )
/*
	Assumptions - cf freeCell()
		. CNDB is free from any proxy attachment
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

	// free context id register value
	Pair *id = BMContextId( ctx );
	freePair( id );

	// free perso stack
	listItem *stack = registryLookup( ctx, "." )->value;
	freeRegistry(((Pair *) stack->ptr )->value, NULL );
	freePair( stack->ptr );
	freeItem( stack );

	// free CNDB & context registry
	CNDB *db = BMContextDB( ctx );
	freeCNDB( db );
	freeRegistry( ctx, NULL ); }

