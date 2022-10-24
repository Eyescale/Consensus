#include <stdio.h>
#include <stdlib.h>

#include "instantiate_private.h"
#include "string_util.h"
#include "database.h"

//===========================================================================
//	bm_instantiate
//===========================================================================
static BMTraverseCB
	term_CB, collect_CB, bgn_set_CB, end_set_CB, bgn_pipe_CB, end_pipe_CB,
	open_CB, close_CB, decouple_CB, register_variable_CB, literal_CB, list_CB,
	wildcard_CB, dot_identifier_CB, identifier_CB, signal_CB, end_CB;

void
bm_instantiate( char *expression, BMContext *ctx, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate: %s ........{\n", expression );
#endif
	BMInstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMTermCB ]		= term_CB;
	table[ BMNotCB ]		= collect_CB;
	table[ BMDereferenceCB ]	= collect_CB;
	table[ BMBgnSetCB ]		= bgn_set_CB;
	table[ BMEndSetCB ]		= end_set_CB;
	table[ BMBgnPipeCB ]		= bgn_pipe_CB;
	table[ BMEndPipeCB ]		= end_pipe_CB;
	table[ BMSubExpressionCB ]	= collect_CB;
	table[ BMModCharacterCB ]	= identifier_CB;
	table[ BMStarCharacterCB ]	= identifier_CB;
	table[ BMRegisterVariableCB ]	= register_variable_CB;
	table[ BMLiteralCB ]		= literal_CB;
	table[ BMListCB ]		= list_CB;
	table[ BMOpenCB ]		= open_CB;
	table[ BMDecoupleCB ]		= decouple_CB;
	table[ BMCloseCB ]		= close_CB;
	table[ BMCharacterCB ]		= identifier_CB;
	table[ BMWildCardCB ]		= wildcard_CB;
	table[ BMDotIdentifierCB ]	= dot_identifier_CB;
	table[ BMIdentifierCB ]		= identifier_CB;
	table[ BMSignalCB ]		= signal_CB;

	if ( *expression==':' )
		bm_instantiate_assignment( expression, &traverse_data, story );
	else if ( *expression=='!' ) {
		char *p = expression + 2; // skip '!!'
		Pair *entry = registryLookup( story, p );
		p = p_prune( PRUNE_IDENTIFIER, p );
		bm_conceive( p, &traverse_data, entry ); }
	else {
		DBG_VOID( expression )
		traverse_data.done = INFORMED;
		bm_traverse( expression, &traverse_data, FIRST ); }

	if ( traverse_data.done==2 ) {
		fprintf( stderr, ">>>>> B%%: Warning: unable to complete instantation\n"
                        "\t\tdo %s\n\t<<<<< failed on '%s'\n", expression, traverse_data.p );
		freeListItem( &data.stack.flags );
		freeListItem( &data.sub[ 1 ] );
		listItem *instances;
		listItem **results = &data.results;
		while (( instances = popListItem(results) ))
			freeListItem( &instances );
		bm_flush_pipe( ctx ); }
#ifdef DEBUG
	if (( data.sub[ 0 ] )) {
		fprintf( stderr, "bm_instantiate:........} " );
		if ( traverse_data.done==2 ) fprintf( stderr, "***INCOMPLETE***\n" );
		else db_outputf( stderr, BMContextDB(ctx), "first=%_\n", data.sub[0]->ptr ); }
	else fprintf( stderr, "bm_instantiate: } no result\n" );
#endif
	freeListItem( &data.sub[ 0 ] );
}

//---------------------------------------------------------------------------
//	bm_conceive
//---------------------------------------------------------------------------
CNInstance *
bm_conceive( char *p, BMTraverseData *traverse_data, Pair *entry )
/*
	Assumption: *p=='('
*/
{
	BMInstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNEntity *this = ctx->this;
	// instantiate new cell
	CNDB *db = newCNDB();
	CNCell *cell = newCell( entry, db, this );
	BMContext *new = cell->ctx;
	// inform cell
	data->new = new;
	traverse_data->done = INFORMED|NEW;
	p = bm_traverse( p, traverse_data, FIRST );
	// carry cell
	CNInstance *proxy = NULL;
	addItem( BMContextCarry(ctx), cell );
	if ( *p=='~' ) 
		bm_context_finish( cell->ctx, 0 );
	else {
		bm_context_finish( cell->ctx, 1 );
		proxy = db_proxy( this, new->this, BMContextDB(ctx) );
		bm_activate( proxy, ctx, NULL ); }
	return proxy;
}

//---------------------------------------------------------------------------
//	bm_instantiate_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( bm_instantiate_traversal )
case_( term_CB )
	if is_f( FILTERED ) {
		if (!( data->sub[current] = bm_scan(p,data->ctx) ))
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( collect_CB )
	if (!( data->sub[current] = bm_scan(p,data->ctx) ))
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( bgn_set_CB )
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	addItem( &data->results, NULL );
	_break
case_( end_set_CB )
	/* Assumption:	!is_f(LEVEL|SUB_EXPR)
	*/
	listItem *instances = popListItem( &data->results );
	instances = catListItem( instances, data->sub[ 0 ] );
	if ( f_next & FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_break
case_( bgn_pipe_CB )
	bm_push_mark( data->ctx, '|', data->sub[ current ] );
	addItem( &data->results, data->sub[ 0 ] );
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 1 ] );
		data->sub[ 1 ] = NULL; }
	data->sub[ 0 ] = NULL;
	_break
case_( end_pipe_CB )
	bm_pop_mark( data->ctx, '|' );
	freeListItem( &data->sub[ 0 ] );
	if (!(f_next&FIRST))
		data->sub[ 1 ] = popListItem( &data->results );
	data->sub[ 0 ] = popListItem( &data->results );
	_break
case_( register_variable_CB )
	CNInstance *e;
	switch ( p[1] ) {
	case '?':
	case '!':
		e = bm_context_lookup( data->ctx, ((char_s)(int)p[1]).s );
		if ( !e ) _return( 2 )
		data->sub[ current ] = newItem( e );
		break;
	case '|': ;
		listItem *i = bm_context_lookup( data->ctx, "|" );
		if ( !i ) _return( 2 )
		listItem **sub = &data->sub[ current ];
		for ( ; i!=NULL; i=i->next ) addItem( sub, i->ptr );
		break;
	case '.':
		e = BMContextParent( data->ctx );
		if ( !e ) _return( 2 )
		data->sub[ current ] = newItem( e );
		break;
	case '%':
		e = BMContextSelf( data->ctx );
		data->sub[ current ] = newItem( e );
		break; }
	_break
case_( list_CB )
	/*	((expression,...):_sequence_:)
	   start p ----------^               ^
		     return p ---------------
	*/
	data->sub[ 0 ] = bm_list( q, data->sub, data->ctx );
	_prune( BM_PRUNE_LIST )
case_( literal_CB )
	/*		(:_sequence_:)
	   start p -----^             ^
		return p -------------
	*/
	CNInstance *e = bm_literal( q, data->ctx );
	data->sub[ current ] = newItem( e );
	(*q)++;
	_prune( BM_PRUNE_LITERAL )
case_( open_CB )
	if ( f_next & DOT )
		_break
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	_break
case_( decouple_CB )
	if (!is_f(LEVEL|SUB_EXPR)) {	// Assumption: is_f(SET)
		listItem **results = &data->results;
		listItem *instances = popListItem( results );
		addItem( results, catListItem( instances, data->sub[ 0 ] ));
		data->sub[ 0 ] = NULL; }
	_break
case_( close_CB )
	listItem *instances = is_f( FIRST ) ?
		data->sub[ 0 ] : bm_couple( data->sub, data->ctx );
	if (!is_f(FIRST)) {
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if ( is_f(DOT) ) {
		CNInstance *this = bm_context_fetch( data->ctx, "." );
		if ((this) && (instances)) {
			data->sub[ 0 ] = newItem( this );
			data->sub[ 1 ] = instances;
			listItem *localized = bm_couple( data->sub, data->ctx );
			freeListItem( &data->sub[ 0 ] );
			freeListItem( &data->sub[ 1 ] );
			instances = localized; }
		else _return( 2 ) }
	if ( f_next & FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_break
case_( wildcard_CB )
	data->sub[ current ] = newItem( NULL );
	_break
case_( dot_identifier_CB )
	CNInstance *this = bm_context_fetch( data->ctx, "." );
	CNInstance *e = bm_register( data->ctx, p+1 );
	if ((this) && (e)) {
		e = db_instantiate( this, e, BMContextDB(data->ctx) );
		data->sub[ current ] = newItem( e ); }
	else _return( 2 )
	_break
case_( identifier_CB )
	CNInstance *e = bm_register( data->ctx, p );
	data->sub[ current ] = newItem( e );
	_break
case_( signal_CB )
	CNInstance *e = data->sub[ current ]->ptr;
	db_signal( e, BMContextDB(data->ctx) );
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	bm_scan
//---------------------------------------------------------------------------
static BMQueryCB scan_CB;

static listItem *
bm_scan( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
	return results;
}
static BMCBTake
scan_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}

//---------------------------------------------------------------------------
//	bm_couple
//---------------------------------------------------------------------------
static listItem *
bm_couple( listItem *sub[2], BMContext *ctx )
/*
	Instantiates and/or returns all ( sub[0], sub[1] )
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL, *s = NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *t = NULL;
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
				addIfNotThere( &results, db_instantiate(e,f,db) );
		}
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, db_instantiate(e,j->ptr,db) );
		}
	}
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, db_instantiate(i->ptr,f,db) );
	}
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, db_instantiate(i->ptr,j->ptr,db) );
	}
	return results;
}

//---------------------------------------------------------------------------
//	bm_literal
//---------------------------------------------------------------------------
static char *sequence_step( char *, CNInstance **, CNDB * );

static CNInstance *
bm_literal( char **position, BMContext *ctx )
/*
	Assumption: expression = (:_sequence_) resp. (:_sequence_:)
	where
		_sequence_ = ab...yz
	converts into (a,(b,...(y,z))) resp. (a,(b,...(z,'\0')))
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *stack = NULL;
	CNInstance *work_instance[ 3 ] = { NULL, NULL, NULL };
	CNInstance *e, *f;
	char *p = *position + 2; // skip opening "(:"
	for ( ; ; ) {
		switch ( *p ) {
		case ')':
			e = popListItem(&stack);
			while (( f = popListItem(&stack) ))
				e = db_instantiate( f, e, db );
			goto RETURN;
		case ':':
			if ( p[1]==')' ) {
				e = db_register( "\0", db );
				p++; break;
			}
			// no break
		default:
			p = sequence_step( p, work_instance, db );
			e = work_instance[ 2 ];
		}
		if (( e )) addItem( &stack, e );
	}
RETURN:
	*position = p;
	return e;
}

//---------------------------------------------------------------------------
//	bm_list
//---------------------------------------------------------------------------
static listItem *
bm_list( char **position, listItem **sub, BMContext *ctx )
/*
	Assumption: ((expression,...):_sequence_:)
	  start	*position -------^               ^
		 return *position ---------------
	where
		_sequence_ = ab...yz
	converts into (((((expression,*),a),b)...,y)z)
	  inserting star -------------^
*/
{
	if ( !sub[0] ) {
		*position = p_prune( PRUNE_LIST, *position );
		return NULL;
	}
	listItem *results = NULL;
	CNDB *db = BMContextDB( ctx );
	CNInstance *work_instance[ 3 ] = { NULL, NULL, NULL };
	CNInstance *star = db_star( db );
	CNInstance *e;
	char *q = *position + 5; // start past "...):"
	char *p;
	while (( e = popListItem(&sub[0]) )) {
		CNInstance *f = star;
		e = db_instantiate( e, f, db );
		p = q;
		for ( ; ; ) {
			switch ( *p ) {
			case ':':
				if ( p[1]==')' ) goto BREAK;
				// no break
			default:
				p = sequence_step( p, work_instance, db );
				f = work_instance[ 2 ];
			}
			if (( f )) e = db_instantiate( e, f, db );
		}
BREAK:
		addItem( &results, e );
	}
	*position = p+1;
	return results;
}

static char *
sequence_step( char *p, CNInstance **wi, CNDB *db )
/*
	wi means work-instance(s)
	wi[ 0 ]	represents Mod (%)
	wi[ 1 ] represents Backslash (\\)
	wi[ 2 ]	represents return value
*/
{
	CNInstance *e;
	char_s q;
	switch ( *p ) {
	case '%':
		if ( !wi[0] ) wi[0] = db_register( "%", db );
		switch ( p[1] ) {
		case '%':
			e = db_instantiate( wi[0], wi[0], db );
			p+=2; break;
		default:
			p++;
			e = db_register( p, db );
			e = db_instantiate( wi[0], e, db );
			if ( !is_separator(*p) ) {
				p = p_prune( PRUNE_IDENTIFIER, p+1 );
				if ( *p=='\'' ) p++;
			}
		}
		break;
	case '\\':
		switch (( q.value = p[1] )) {
		case '0':
		case ' ':
		case 'w':
			if ( !wi[1] ) wi[1] = db_register( "\\", db );
			e = db_register( q.s, db );
			e = db_instantiate( wi[1], e, db );
			p+=2; break;
		case ')':
			e = db_register( q.s, db );
			p+=2; break;
		default:
			p += charscan( p, &q );
			e = db_register( q.s, db );
		}
		break;
	default:
		q.value = p[0];
		e = db_register( q.s, db );
		p++;
	}
	wi[ 2 ] = e;
	return p;
}

//===========================================================================
//	bm_void
//===========================================================================
static BMTraverseCB feel;
static BMTraverseCB
	feel_CB, sound_CB, touch_CB;
#undef case_
#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		BMContext *ctx = traverse_data->user_data; char *p = *q;

int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	returns 0 if it is instantiable, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	listItem *stack = NULL;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = ctx;
	traverse_data.stack = &stack;
	traverse_data.done = INFORMED;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMTermCB ]		= feel_CB;
	table[ BMNotCB ]		= sound_CB;
	table[ BMDereferenceCB ]	= sound_CB;
	table[ BMSubExpressionCB ]	= sound_CB;
	table[ BMRegisterVariableCB ]	= touch_CB;
	bm_traverse( expression, &traverse_data, FIRST );

	freeListItem( &stack );
	return ( traverse_data.done==2 );
}

//---------------------------------------------------------------------------
//	bm_void_traversal
//---------------------------------------------------------------------------
BMTraverseCBSwitch( bm_void_traversal )
case_( feel_CB )
	if is_f( FILTERED ) {
		if ( !bm_feel( BM_CONDITION, p, ctx ) )
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( sound_CB )
	int target = bm_scour( p, PMARK );
	if ( target&PMARK && target!=PMARK ) {
		fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
		"dubious combination of query terms with %%!\n", p );
	}
	if ( !bm_feel( BM_CONDITION, p, ctx ) )
		_return( 2 )
	_prune( BM_PRUNE_TERM )
case_( touch_CB )
	switch ( p[1] ) {
	case '.':
		if ( !BMContextParent( ctx ) )
			_return( 2 )
		break;
	case '?':
	case '!':
		if ( !bm_context_lookup( ctx, ((char_s)(int)p[1]).s ) )
			_return( 2 )
		break; }
	_break
BMTraverseCBEnd

