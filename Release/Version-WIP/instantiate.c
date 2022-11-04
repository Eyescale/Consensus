#include <stdio.h>
#include <stdlib.h>

#include "traverse.h"
#include "instantiate_private.h"
#include "instantiate_traversal.h"
#include "string_util.h"
#include "database.h"
#include "proxy.h"

//===========================================================================
//	bm_instantiate
//===========================================================================
static void bm_instantiate_assignment( char *, BMTraverseData *, CNStory * );
static CNInstance * bm_conceive( Pair *, char *, BMTraverseData * );

static BMTraversal instantiate_traversal;

#define BMTermCB		term_CB
#define BMNotCB			collect_CB
#define BMDereferenceCB		collect_CB
#define BMBgnSetCB		bgn_set_CB
#define BMEndSetCB		end_set_CB
#define BMBgnPipeCB		bgn_pipe_CB
#define BMEndPipeCB		end_pipe_CB
#define BMSubExpressionCB	collect_CB
#define BMModCharacterCB	identifier_CB
#define BMStarCharacterCB	identifier_CB
#define BMRegisterVariableCB	register_variable_CB
#define BMLiteralCB		literal_CB
#define BMListCB		list_CB
#define BMOpenCB		open_CB
#define BMDecoupleCB		decouple_CB
#define BMCloseCB		close_CB
#define BMCharacterCB		identifier_CB
#define BMWildCardCB		wildcard_CB
#define BMDotIdentifierCB	dot_identifier_CB
#define BMIdentifierCB		identifier_CB
#define BMSignalCB		signal_CB

void
bm_instantiate( char *expression, BMContext *ctx, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate: %s ........{\n", expression );
#endif
	BMInstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = BMContextDB( ctx );

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	if ( *expression==':' )
		bm_instantiate_assignment( expression, &traverse_data, story );
	else if ( *expression=='!' ) {
		char *p = expression + 2; // skip '!!'
		Pair *entry = registryLookup( story, p );
		if (( entry )) {
			p = p_prune( PRUNE_IDENTIFIER, p );
			bm_conceive( entry, p, &traverse_data ); }
		else {
			fprintf( stderr, ">>>>> B%%: Error: class not found in expression\n"
				"\t\tdo !! %s\n\t<<<<<\n", p );
			exit( -1 ); } }
	else {
		DBG_VOID( expression )
		traverse_data.done = INFORMED;
		instantiate_traversal( expression, &traverse_data, FIRST ); }

	if ( traverse_data.done==2 ) {
		fprintf( stderr, ">>>>> B%%: Warning: unable to complete instantation\n"
                        "\t\tdo %s\n\t<<<<< failed on '%s'\n", expression, traverse_data.p );
		freeListItem( &data.stack.flags );
		freeListItem( &data.sub[ 1 ] );
		listItem *instances;
		listItem **results = &data.results;
		while (( instances = popListItem(results) ))
			freeListItem( &instances );
		bm_context_pipe_flush( ctx ); }
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
static CNInstance *
bm_conceive( Pair *entry, char *p, BMTraverseData *traverse_data )
/*
	Assumption: *p=='('
*/
{
	BMInstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNEntity *parent = BMContextThis( ctx );
	// instantiate new cell
	CNCell *cell = newCell( entry, parent );
	BMContext *cell_ctx = BMCellContext( cell );
	// inform cell
	data->carry = cell_ctx;
	if ( strncmp(p,"()",2) ) {
		traverse_data->done = INFORMED|NEW;
		p = instantiate_traversal( p, traverse_data, FIRST ); }
	// carry cell
	CNInstance *proxy = NULL;
	addItem( BMCellCarry(parent), cell );
	if ( *p=='~' ) 
		bm_context_finish( cell_ctx, 0 );
	else {
		bm_context_finish( cell_ctx, 1 );
		proxy = db_proxy( parent, cell, data->db );
		bm_context_activate( ctx, proxy ); }
	return proxy;
}

//---------------------------------------------------------------------------
//	instantiate_traversal
//---------------------------------------------------------------------------
#include "traversal.h"

#define current ( is_f( FIRST ) ? 0 : 1 )

BMTraverseCBSwitch( instantiate_traversal )
case_( term_CB )
	if is_f( FILTERED ) {
		if (!( data->sub[current] = bm_scan(p,data->ctx) ))
			_return( 2 )
		_prune( BM_PRUNE_TERM ) }
	_break
case_( collect_CB )
	BMContext *ctx = data->ctx;
	listItem *results = bm_scan( p, ctx );
	if (( results )) {
		BMContext *carry = data->carry;
		data->sub[ current ] = ( (carry) ?
			bm_inform( data->db, &results, carry ) :
			results );
		_prune( BM_PRUNE_TERM ) }
	else _return( 2 )
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
	bm_push_mark( data->ctx, "|", data->sub[ current ] );
	addItem( &data->results, data->sub[ 0 ] );
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 1 ] );
		data->sub[ 1 ] = NULL; }
	data->sub[ 0 ] = NULL;
	_break
case_( end_pipe_CB )
	bm_pop_mark( data->ctx, "|" );
	freeListItem( &data->sub[ 0 ] );
	if (!(f_next&FIRST))
		data->sub[ 1 ] = popListItem( &data->results );
	data->sub[ 0 ] = popListItem( &data->results );
	_break
case_( register_variable_CB )
	CNInstance *e = NULL;
	switch ( p[1] ) {
	case '|': ;
		listItem *i = bm_context_lookup( data->ctx, "|" );
		if ( !i ) _return( 2 )
		listItem **sub = &data->sub[ current ];
		for ( ; i!=NULL; i=i->next )
			addItem( sub, i->ptr );
		_break
	case '<':
		e = eeno_inform( data->ctx, data->db, p, data->carry );
		if ( !e ) _return( 2 );
		data->sub[ current ] = newItem( e );
		_break
	case '?': e = bm_context_lookup( data->ctx, "?" ); break;
	case '!': e = bm_context_lookup( data->ctx, "!" ); break;
	case '.': e = BMContextParent( data->ctx ); break;
	case '%': e = BMContextSelf( data->ctx ); break; }
	if ( !e ) _return( 2 )
	BMContext *carry = data->carry;
	if (( carry ))
		e = bm_inform_context( data->db, e, carry );
	data->sub[ current ] = newItem( e );
	_break
case_( list_CB )
	/*	((expression,...):_sequence_:)
	   start p ----------^               ^
		     return p ---------------
	*/
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	data->sub[ 0 ] = bm_list( q, data->sub, db );
	_prune( BM_PRUNE_LIST )
case_( literal_CB )
	/*		(:_sequence_:)
	   start p -----^             ^
		return p -------------
	*/
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	CNInstance *e = bm_literal( q, db );
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
	listItem *instances;
	BMContext *ctx = data->ctx;
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	if ( is_f( FIRST ) )
		instances = data->sub[ 0 ];
	else {
		instances = bm_couple( data->sub, db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if ( !instances ) _return( 2 )
	if ( is_f(DOT) ) {
		CNInstance *perso = BMContextPerso( ctx );
		if (( carry ))
			perso = bm_inform_context( db, perso, carry );
		data->sub[ 0 ] = newItem( perso );
		data->sub[ 1 ] = instances;
		listItem *localized = bm_couple( data->sub, db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] );
		instances = localized; }
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
	BMContext *ctx = data->ctx;
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	CNInstance *e, *perso = BMContextPerso( ctx );
	if (( carry )) {
		perso = bm_inform_context( db, perso, carry );
		e = db_register( p+1, db ); }
	else {
		e = bm_register( ctx, p+1, db ); }
	e = db_instantiate( perso, e, db );
	data->sub[ current ] = newItem( e );
	_break
case_( identifier_CB )
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	if (( carry )) {
		CNInstance *e = db_register( p, db );
		data->sub[ current ] = newItem( e ); }
	else {
		CNInstance *e = bm_register( data->ctx, p, db );
		data->sub[ current ] = newItem( e ); }
	_break
case_( signal_CB )
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	CNInstance *e = data->sub[ current ]->ptr;
	db_signal( e, db );
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	bm_couple
//---------------------------------------------------------------------------
static listItem *
bm_couple( listItem *sub[2], CNDB *db )
/*
	Instantiates and/or returns all ( sub[0], sub[1] )
*/
{
	listItem *results = NULL, *s = NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *t = NULL;
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
				addIfNotThere( &results, db_instantiate(e,f,db) ); }
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, db_instantiate(e,j->ptr,db) ); } }
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, db_instantiate(i->ptr,f,db) ); }
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, db_instantiate(i->ptr,j->ptr,db) ); }
	return results;
}

//---------------------------------------------------------------------------
//	bm_literal
//---------------------------------------------------------------------------
static char *sequence_step( char *, CNInstance **, CNDB * );

static CNInstance *
bm_literal( char **position, CNDB *db )
/*
	Assumption: expression = (:_sequence_) resp. (:_sequence_:)
	where
		_sequence_ = ab...yz
	converts into (a,(b,...(y,z))) resp. (a,(b,...(z,'\0')))
*/
{
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
				p++; break; }
			// no break
		default:
			p = sequence_step( p, work_instance, db );
			e = work_instance[ 2 ]; }
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
bm_list( char **position, listItem **sub, CNDB *db )
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
		return NULL; }
	listItem *results = NULL;
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
				f = work_instance[ 2 ]; }
			if (( f )) e = db_instantiate( e, f, db ); }
BREAK:
		addItem( &results, e ); }
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
				if ( *p=='\'' ) p++; } }
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
			e = db_register( q.s, db ); }
		break;
	default:
		q.value = p[0];
		e = db_register( q.s, db );
		p++; }
	wi[ 2 ] = e;
	return p;
}

//===========================================================================
//	bm_instantiate_assignment
//===========================================================================
static listItem * bm_assign( listItem *sub[2], BMContext *ctx );

static void
bm_instantiate_assignment( char *expression, BMTraverseData *traverse_data, CNStory *story )
{
	BMInstantiateData *data = traverse_data->user_data;
	char *p = expression + 1; // skip leading ':'
	listItem *sub[ 2 ];

	DBG_VOID( p )
	traverse_data->done = INFORMED;
	p = instantiate_traversal( p, traverse_data, FIRST );
	if ( traverse_data->done==2 || !data->sub[ 0 ] )
		return;
	sub[ 0 ] = data->sub[ 0 ];
	data->sub[ 0 ] = NULL;
	p++; // move past ',' - aka. ':'

	if ( *p=='!' ) {
		p += 2; // skip '!!'
		CNDB *db = BMContextDB( data->ctx );
		Pair *entry = registryLookup( story, p );
		if (( entry )) {
			char *q = p = p_prune( PRUNE_IDENTIFIER, p );
			for ( listItem *i=sub[0]; i!=NULL; i=i->next, p=q ) {
				CNInstance *proxy = bm_conceive( entry, p, traverse_data );
				if (( proxy )) db_assign( i->ptr, proxy, db );
				else db_unassign( i->ptr, db ); } }
		else {
			fprintf( stderr, ">>>>> B%%: Error: class not found in expression\n"
				"\t\tdo !! %s\n\t<<<<<\n", p );
			exit( -1 ); } }
	else if ( !strncmp( p, "~.", 2 ) ) {
		sub[ 1 ] = NULL;
		data->sub[ 0 ] = bm_assign( sub, data->ctx );
		freeListItem( &sub[ 0 ] ); }
	else {
		DBG_VOID( p )
		traverse_data->done = INFORMED;
		p = instantiate_traversal( p, traverse_data, FIRST );
		if ( traverse_data->done==2 )
			freeListItem( &sub[ 0 ] );
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = bm_assign( sub, data->ctx );
			freeListItem( &sub[ 0 ] );
			freeListItem( &sub[ 1 ] ); } }
}
static listItem *
bm_assign( listItem *sub[2], BMContext *ctx )
/*
	Note that the number of assignments actually performed is
		INF( cardinal(sub[0]), cardinal(sub[1]) )
	And that nothing is done with the excess, if there is -
		case sub[1]==NULL excepted
*/
{
	CNDB *db = BMContextDB( ctx );
	listItem *results = NULL;
	if ( !sub[1] )
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
			addItem( &results, db_unassign( i->ptr, db ) );
	else
		for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next )
			addItem( &results, db_assign( i->ptr, j->ptr, db ) );
	return results;
}

