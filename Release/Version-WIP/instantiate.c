#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "narrative.h"
#include "expression.h"
#include "traverse.h"
#include "locate.h"

// #define DEBUG

static int bm_void( char *, BMContext * );
static listItem *bm_couple( listItem *sub[2], BMContext * );
static CNInstance *bm_literal( char **, BMContext * );
static listItem *bm_list( char **, listItem **, BMContext * );
static listItem *bm_scan( char *, BMContext * );

//===========================================================================
//	bm_instantiate
//===========================================================================
static BMTraverseCB
	preempt_CB, collect_CB, bgn_set_CB, end_set_CB, bgn_pipe_CB, end_pipe_CB,
	open_CB, close_CB, decouple_CB, mark_register_CB, literal_CB, list_CB,
	wildcard_CB, dot_identifier_CB, identifier_CB, signal_CB, end_CB;
typedef struct {
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	BMContext *ctx;
	int empty;
} BMInstantiateData;
#define case_( func ) \
	} static BMCB_take func( BMTraverseData *traverse_data, char *p, int flags ) { \
		BMInstantiateData *data = traverse_data->user_data;
#define current \
	( is_f( FIRST ) ? 0 : 1 )

void
bm_instantiate( char *expression, BMContext *ctx )
{
#ifdef DEBUG
	if ( bm_void( expression, ctx ) ) {
		fprintf( stderr, ">>>>> B%%: bm_instantiate(): VOID: %s\n", expression );
		exit( -1 );
	}
	else fprintf( stderr, "bm_instantiate: %s ........{\n", expression );
#endif
	BMInstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.empty = db_is_empty( 0, BMContextDB(ctx) );
	data.ctx = ctx;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMPreemptCB ]		= preempt_CB;
	table[ BMNotCB ]		= collect_CB;
	table[ BMDereferenceCB ]	= collect_CB;
	table[ BMBgnSetCB ]		= bgn_set_CB;
	table[ BMEndSetCB ]		= end_set_CB;
	table[ BMBgnPipeCB ]		= bgn_pipe_CB;
	table[ BMEndPipeCB ]		= end_pipe_CB;
	table[ BMSubExpressionCB ]	= collect_CB;
	table[ BMModCharacterCB ]	= identifier_CB;
	table[ BMStarCharacterCB ]	= identifier_CB;
	table[ BMMarkRegisterCB ]	= mark_register_CB;
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
	bm_traverse( expression, &traverse_data, &data.stack.flags, FIRST );

	if ( traverse_data.done == 2 ) {
		freeListItem( &data.sub[ 1 ] );
		listItem *instances;
		listItem **results = &data.results;
		while (( instances = popListItem(results) ))
			freeListItem( &instances );
	}
#ifdef DEBUG
	if (( data.sub[ 0 ] )) {
		fprintf( stderr, "bm_instantiate:........} " );
		if ( traverse_data.done == 2 ) fprintf( stderr, "***INCOMPLETE***\n" );
		else db_outputf( stderr, BMContextDB(ctx), "first=%_\n", data.sub[0]->ptr ); }
	else fprintf( stderr, "bm_instantiate: } no result\n" );
#endif
	freeListItem( &data.stack.flags );
	freeListItem( &data.sub[ 0 ] );
}

BMTraverseCBSwitch( bm_instantiate_traversal )
case_( preempt_CB )
	if ( p_filtered( p ) )
		return collect_CB( traverse_data, p, flags );
	_continue
case_( collect_CB )
	if (!( data->sub[ current ] = bm_scan( p, data->ctx ) ))
		traverse_data->done = 2;
	else {
		p = p_prune( PRUNE_TERM, p );
		f_set( INFORMED ); }
	_break( p )
case_( bgn_set_CB )
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	addItem( &data->results, NULL );
	_continue
case_( end_set_CB )
	/* Assumption:	!is_f(LEVEL)
	*/
	listItem *instances = popListItem( &data->results );
	instances = catListItem( instances, data->sub[ 0 ] );
	flags = traverse_data->f_next;
	if is_f( FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_continue
case_( bgn_pipe_CB )
	bm_push_mark( data->ctx, '!', data->sub[ current ] );
	addItem( &data->results, data->sub[ 0 ] );
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 1 ] );
		data->sub[ 1 ] = NULL; }
	data->sub[ 0 ] = NULL;
	_continue
case_( end_pipe_CB )
	bm_pop_mark( data->ctx, '!' );
	freeListItem( &data->sub[ 0 ] );
	flags = traverse_data->f_next;
	if (!is_f(FIRST))
		data->sub[ 1 ] = popListItem( &data->results );
	data->sub[ 0 ] = popListItem( &data->results );
	_continue
case_( mark_register_CB )
	if ( p[1]=='?' ) {
		CNInstance *e = bm_context_lookup( data->ctx, "?" );
		if ( !e ) { traverse_data->done=2; _break( p ) }
		data->sub[ current ] = newItem( e ); }
	else if ( p[1]=='!' ) {
		listItem *i = bm_context_lookup( data->ctx, "!" );
		if ( !i ) { traverse_data->done=2; _break( p ) }
		listItem **sub = &data->sub[ current ];
		for ( ; i!=NULL; i=i->next ) addItem( sub, i->ptr ); }
	_continue
case_( list_CB )
	/*	((expression,...):_sequence_:)
	   start p ----------^               ^
		     return p ---------------
	*/
	data->sub[ 0 ] = bm_list( &p, data->sub, data->ctx );
	f_pop( &data->stack.flags, 0 )
	f_set( INFORMED )
	_break( p )
case_( literal_CB )
	/*		(:_sequence_:)
	   start p -----^             ^
		return p -------------
	*/
	CNInstance *e = bm_literal( &p, data->ctx );
	data->sub[ current ] = newItem( e );
	f_set( INFORMED )
	_break( p+1 )
case_( open_CB )
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	_continue
case_( decouple_CB )
	if (!is_f(LEVEL)) {	// Assumption: is_f(SET)
		listItem **results = &data->results;
		listItem *instances = popListItem( results );
		addItem( results, catListItem( instances, data->sub[ 0 ] ));
		data->sub[ 0 ] = NULL; }
	_continue
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
			instances = localized; } }
		else ; // err !!!
	flags = traverse_data->f_next;
	if is_f( FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_continue
case_( wildcard_CB )
	if ( data->empty ) { traverse_data->done=2; _break( p ); }
	data->sub[ current ] = newItem( NULL );
	_continue
case_( dot_identifier_CB )
	CNInstance *this = bm_context_fetch( data->ctx, "." );
	CNInstance *e = bm_register( data->ctx, p+1 );
	if ((this) && (e)) {
		e = db_instantiate( this, e, BMContextDB(data->ctx) );
		data->sub[ current ] = newItem( e ); }
	else ; // err !!!
	_continue
case_( identifier_CB )
	CNInstance *e = bm_register( data->ctx, p );
	data->sub[ current ] = newItem( e );
	_continue
case_( signal_CB )
	CNInstance *e = data->sub[ current ]->ptr;
	db_signal( e, BMContextDB(data->ctx) );
	_continue
BMTraverseCBEnd

//===========================================================================
//	bm_void
//===========================================================================
static BMTraverseCB feel;
static BMTraverseCB
	feel_CB, check_CB, touch_CB, sound_CB;
typedef struct {
	BMContext *ctx;
	int empty;
} BMVoidData;
#undef case_
#define case_( func ) \
	} static BMCB_take func( BMTraverseData *traverse_data, char *p, int flags ) { \
		BMVoidData *data = traverse_data->user_data;

static int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is instantiable, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	BMVoidData data;
	memset( &data, 0, sizeof(data) );
	data.empty = db_is_empty( 0, BMContextDB(ctx) );
	data.ctx = ctx;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = &data;
	listItem *stack = NULL;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMPreemptCB ]		= feel_CB;
	table[ BMNotCB ]		= check_CB;
	table[ BMDereferenceCB ]	= check_CB;
	table[ BMSubExpressionCB ]	= check_CB;
	table[ BMMarkRegisterCB ]	= touch_CB;
	table[ BMWildCardCB ]		= sound_CB;
	bm_traverse( expression, &traverse_data, &stack, FIRST );

	freeListItem( &stack );
	return ( traverse_data.done==2 );
}

BMTraverseCBSwitch( bm_void_traversal )
case_( feel_CB )
	if ( p_filtered( p ) )
		return feel( traverse_data, p, flags );
	_continue
case_( check_CB )
	int target = bm_scour( p, EMARK );
	if ( target&EMARK && target!=EMARK ) {
			fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
			"dubious combination of query terms with %%!\n", p );
	}
	return feel( traverse_data, p, flags );
case_( touch_CB )
	if ( p[1]=='?' && !bm_context_lookup( data->ctx, "?" ) )
		{ traverse_data->done=2; return BM_DONE; }
	_continue
case_( sound_CB )
	if ( data->empty )
		{ traverse_data->done=2; return BM_DONE; }
	_continue
BMTraverseCBEnd

static BMCB_take
feel( BMTraverseData *traverse_data, char *p, int flags )
{
	BMVoidData *data = traverse_data->user_data;
	if ( data->empty || !bm_feel( BM_CONDITION, p, data->ctx ) )
		traverse_data->done = 2;
	else {
		traverse_data->p = p_prune( PRUNE_TERM, p );
		traverse_data->flags = f_set( INFORMED ); }
	return BM_DONE;
}

//===========================================================================
//	bm_couple
//===========================================================================
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

//===========================================================================
//	bm_literal
//===========================================================================
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
				e = db_register( "\0", db, 0 );
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

//===========================================================================
//	bm_list
//===========================================================================
static listItem *
bm_list( char **position, listItem **sub, BMContext *ctx )
/*
	Assumption: ((expression,...):_sequence_:)
	  start	*position -------^               ^
		 return *position ---------------
	where
		_sequence_ = ab...yz
	converts into ((((expression,a),b)...,y)z)
*/
{
	if ( !sub[0] ) {
		*position = p_prune( PRUNE_LIST, *position );
		return NULL;
	}
	listItem *results = NULL;
	CNDB *db = BMContextDB( ctx );
	CNInstance *work_instance[ 3 ] = { NULL, NULL, NULL };
	CNInstance *e;
	char *q = *position + 5; // start past "...):"
	char *p;
	while (( e = popListItem(&sub[0]) )) {
		CNInstance *f = NULL;
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
		if ( !wi[0] ) wi[0] = db_register( "%", db, 0 );
		switch ( p[1] ) {
		case '%':
			e = db_instantiate( wi[0], wi[0], db );
			p+=2; break;
		default:
			p++;
			e = db_register( p, db, 0 );
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
			if ( !wi[1] ) wi[1] = db_register( "\\", db, 0 );
			e = db_register( q.s, db, 0 );
			e = db_instantiate( wi[1], e, db );
			p+=2; break;
		case ')':
			e = db_register( q.s, db, 0 );
			p+=2; break;
		default:
			p += charscan( p, &q );
			e = db_register( q.s, db, 0 );
		}
		break;
	default:
		q.value = p[0];
		e = db_register( q.s, db, 0 );
		p++;
	}
	wi[ 2 ] = e;
	return p;
}

//===========================================================================
//	bm_scan
//===========================================================================
static BMQueryCB scan_CB;

static listItem *
bm_scan( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_query( BM_CONDITION, expression, ctx, scan_CB, &results );
	return results;
}
static BMCB_take
scan_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}

