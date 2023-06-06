#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "traverse.h"
#include "instantiate.h"
#include "eenov.h"
#include "cell.h"

// #define DEBUG

//===========================================================================
//	bm_instantiate
//===========================================================================
#include "instantiate_traversal.h"

static void instantiate_new( char *, CNStory *, BMTraverseData *, listItem ** );
static void instantiate_assignment( char *, BMTraverseData *, CNStory * );
typedef struct {
	struct { listItem *flags; } stack;
	listItem *sub[ 2 ];
	listItem *results;
	BMContext *ctx, *carry;
	CNDB *db;
} InstantiateData;
static void cleanup( InstantiateData *, BMContext * );

void
bm_instantiate( char *expression, BMContext *ctx, CNStory *story )
{
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate: %s ........{\n", expression );
#endif
	InstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = BMContextDB( ctx );

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	if ( *expression=='!' ) {
		instantiate_new( expression, story, &traverse_data, NULL ); }
	else if ( *expression==':' )
		instantiate_assignment( expression, &traverse_data, story );
	else {
		traverse_data.done = INFORMED|LITERAL;
		if ( !story ) traverse_data.done |= CLEAN;
		instantiate_traversal( expression, &traverse_data, FIRST ); }

	if ( traverse_data.done==2 ) {
		fprintf( stderr, ">>>>> B%%: Warning: unable to complete instantiation\n"
                        "\t\tdo %s\n\t<<<<< failed on '%s'\n", expression, traverse_data.p );
		cleanup( &data, ctx ); }
#ifdef DEBUG
	if (( data.sub[ 0 ] )) {
		fprintf( stderr, "bm_instantiate:........} " );
		if ( traverse_data.done==2 ) fprintf( stderr, "***INCOMPLETE***\n" );
		else db_outputf( stderr, data.db, "first=%_\n", data.sub[0]->ptr ); }
	else fprintf( stderr, "bm_instantiate: } no result\n" );
#endif
	freeListItem( &data.sub[ 0 ] );
}
static void
cleanup( InstantiateData *data, BMContext *ctx )
{
	freeListItem( &data->stack.flags );
	freeListItem( &data->sub[ 1 ] );
	listItem *instances;
	listItem **results = &data->results;
	while (( instances = popListItem(results) ))
		freeListItem( &instances );
	bm_context_pipe_flush( ctx );
}

//---------------------------------------------------------------------------
//	instantiate_new
//---------------------------------------------------------------------------
static CNInstance * conceive( Pair *entry, char *, BMTraverseData * );

static void
instantiate_new( char *expression, CNStory *story, BMTraverseData *data, listItem **per )
{
	char *p = expression + 2; // skip '!!'
	Pair *entry = registryLookup( story, p );
	if (( entry )) {
		p = p_prune( PRUNE_IDENTIFIER, p );
		if (( per )) {
			CNDB *db = ((InstantiateData *)( data->user_data ))->db;
			for ( CNInstance *x; (x=popListItem( per )); ) {
				CNInstance *proxy = conceive( entry, p, data );
				if (( proxy )) db_assign( x, proxy, db );
				else db_unassign( x, db ); } }
		else conceive( entry, p, data ); }
	else {
		fprintf( stderr, ">>>>> B%%: Error: class not found in expression\n"
			"\tdo !! %s <<<<<\n", p );
		if (( per )) freeListItem( per ); }
}

static CNInstance *
conceive( Pair *entry, char *p, BMTraverseData *traverse_data )
/*
	Assumption: *p=='('
*/
{
	InstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNEntity *cell = BMContextCell( ctx );
	// instantiate new cell
	CNCell *new = newCell( entry, cell );
	BMContext *carry = BMCellContext( new );
	// inform new cell's context
	data->carry = carry;
	p++; // skipping opening '('
	if ( *p==')' ) p++;
	else do {
		char *q = p;
		traverse_data->done = INFORMED|LITERAL;
		p = instantiate_traversal( p, traverse_data, FIRST );
		if ( traverse_data->done==2 ) {
			cleanup( data, ctx );
			traverse_data->done = 1;
			p = p_prune( PRUNE_TERM, q ); }
		freeListItem( &data->sub[ 0 ] ); }
	while ( *p++!=')' );
	// carry new and return proxy
	return bm_cell_carry( cell, new, !(*p=='~') );
}

//---------------------------------------------------------------------------
//	instantiate_traversal
//---------------------------------------------------------------------------
static listItem *instantiate_couple( listItem *sub[2], CNDB * );
static CNInstance *instantiate_literal( char **, CNDB * );
static listItem *instantiate_list( char **, listItem **, CNDB * );
static listItem *instantiate_xpan( listItem **, CNDB * );

#define current ( is_f( FIRST ) ? 0 : 1 )

BMTraverseCBSwitch( instantiate_traversal )
case_( term_CB )
	if ( !strncmp(p,"(:",2) ) {
		/*		(:_sequence_:)
		   start p -----^             ^
			return p=*q ----------
		*/
		BMContext *carry = data->carry;
		CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
		CNInstance *e = instantiate_literal( q, db );
		data->sub[ current ] = newItem( e ); }
	else if is_f( FILTERED ) {
		listItem *results = bm_scan( p, data->ctx );
		if (( results )) {
			BMContext *carry = data->carry;
			data->sub[ current ] = ( (carry) ?
				bm_inform( 1, carry, &results, data->db ) :
				results );
			_prune( BM_PRUNE_TERM ) }
		else _return( 2 ) }
	_break
case_( collect_CB )
	listItem *results = bm_scan( p, data->ctx );
	if (( results )) {
		BMContext *carry = data->carry;
		data->sub[ current ] = ( (carry) ?
			bm_inform( 1, carry, &results, data->db ) :
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
	if ( f_next & FIRST ) {
		data->sub[ 0 ] = instances; }
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
	BMContext *carry = data->carry;
	CNInstance *e;
	listItem *i;
	switch ( p[1] ) {
	case '<': ;
		i = eenov_inform( data->ctx, data->db, p, data->carry );
		if (( i )) {
			data->sub[ current ] = i;
			_break }
		break;
	case '|':
	case '@':
		i = bm_context_lookup( data->ctx, p );
		if (( i )) {
			listItem **sub = &data->sub[ current ];
			if (( carry )) {
				CNDB *db = data->db;
				for ( ; i!=NULL; i=i->next )
					addItem( sub, bm_inform( 0, carry, i->ptr, db ) ); }
			else {
				for ( ; i!=NULL; i=i->next )
					addItem( sub, i->ptr ); }
			_break }
		break;
	default:
		e = bm_context_lookup( data->ctx, p );
		if (( e )) {
			if (( carry ))
				e = bm_inform( 0, carry, e, data->db );
			data->sub[ current ] = newItem( e );
			_break } }
	_return( 2 )
case_( list_CB )
	/* We have ((expression,...):_sequence_:)
	   start p -------------^               ^
		     return p=*q ---------------
	*/
	BMContext *carry = data->carry;
	data->sub[ 0 ] = ((carry) ?
		instantiate_list( q, data->sub, BMContextDB(carry) ) :
		instantiate_list( q, data->sub, data->db ));
	_break
case_( dot_expression_CB )
	BMContext *carry = data->carry;
	if (( carry )) {
		listItem *results = bm_scan( p, data->ctx );
		if (( results )) {
			for ( listItem *i=results; i!=NULL; i=i->next )
				i->ptr = ((CNInstance *) i->ptr )->sub[ 1 ];
			data->sub[ current ] = bm_inform( 1, carry, &results, data->db );
			_prune( BM_PRUNE_TERM ) }
		else _return( 2 ) }
	_break
case_( open_CB )
	if ( f_next & DOT )
		_break
	if (!is_f(FIRST)) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	_break
case_( decouple_CB )
	if (!is_f(LEVEL|SUB_EXPR)) {	// Assumption: is_f(SET|VECTOR)
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
		instances = is_f(ELLIPSIS) ?
			instantiate_xpan( data->sub, db ) :
			instantiate_couple( data->sub, db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if ( !instances ) _return( 2 )
	if ( is_f(DOT) ) {
		// case carry here handled in dot_expression_CB
		CNInstance *perso = BMContextPerso( ctx );
		data->sub[ 0 ] = newItem( perso );
		data->sub[ 1 ] = instances;
		instances = instantiate_couple( data->sub, db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if ( f_next & FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_break
case_( activate_CB )
	ActiveRV *active = BMContextActive( data->ctx );
	listItem **buffer = ( *p=='@' ) ?
		&active->buffer->activated :
		&active->buffer->deactivated;
	for ( listItem *i=data->sub[0]; i!=NULL; i=i->next ) {
		CNInstance *e = i->ptr;
		if ( isProxy(e) && !isProxySelf(e) )
			addIfNotThere( buffer, e ); }
	_break
case_( wildcard_CB )
	data->sub[ current ] = newItem( NULL );
	_break
case_( dot_identifier_CB )
	BMContext *ctx = data->ctx;
	BMContext *carry = data->carry;
	CNDB *db = data->db;
	CNInstance *e;
	if (( carry )) {
		if (( e = bm_context_lookup( ctx, p+1 ) ))
			e = bm_inform( 0, carry, e, db );
		else _return( 2 ) }
	else {
		CNInstance *perso = BMContextPerso( ctx );
		e = bm_register( ctx, p+1, db );
		e = db_instantiate( perso, e, db ); }
	data->sub[ current ] = newItem( e );
	_break
case_( identifier_CB )
	BMContext *carry = data->carry;
	CNDB *db = ( (carry) ? BMContextDB(carry) : data->db );
	if (( carry )) {
		CNInstance *e = bm_register( NULL, p, db );
		data->sub[ current ] = newItem( e ); }
	else {
		CNInstance *e = bm_register( data->ctx, p, db );
		data->sub[ current ] = newItem( e ); }
	_break
case_( signal_CB )
	CNInstance *e = data->sub[ current ]->ptr;
	BMContext *carry = data->carry;
	db_signal( e, ((carry) ? BMContextDB(carry) : data->db ));
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	instantiate_couple
//---------------------------------------------------------------------------
static inline void
couple( CNInstance *, CNInstance *, CNDB *, listItem **, listItem ** );

static listItem *
instantiate_couple( listItem *sub[2], CNDB *db )
/*
	Instantiates and/or returns all ( sub[0], sub[1] )
*/
{
	listItem *results=NULL, *trail=NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *s = NULL;
			listItem *t = NULL;
			for ( CNInstance *e=DBFirst(db,&s); e!=NULL; e=DBNext(db,e,&s) )
			for ( CNInstance *f=DBFirst(db,&t); f!=NULL; f=DBNext(db,f,&t) )
				couple( e, f, db, &trail, &results ); }
		else {
			listItem *s = NULL;
			for ( CNInstance *e=DBFirst(db,&s); e!=NULL; e=DBNext(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				couple( e, j->ptr, db, &trail, &results ); } }
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=DBFirst(db,&t); f!=NULL; f=DBNext(db,f,&t) )
			couple( i->ptr, f, db, &trail, &results ); }
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			couple( i->ptr, j->ptr, db, &trail, &results ); }
	freeListItem( &trail );
	return results;
}

static inline void
couple( CNInstance *e, CNInstance *f, CNDB *db, listItem **trail, listItem **results )
{
	CNInstance *g = db_instantiate( e, f, db );
	if ((g) && !lookupIfThere( *trail, g ) ) {
		addIfNotThere( trail, g );
		addItem( results, g ); }
}

//---------------------------------------------------------------------------
//	instantiate_xpan
//---------------------------------------------------------------------------
static listItem *
instantiate_xpan( listItem *sub[2], CNDB *db )
/*
	Assuming we had expression:((sub[0],...),sub[1])
		where sub[1] is { a, b, ..., z }
	converts into ((((sub[0],a),b),...),z)
*/
{
	listItem *results = NULL;
	if ( !sub[ 0 ] ) {
		results = sub[ 1 ];
		sub[ 1 ] = NULL; }
	else if ( !sub[ 1 ] ) {
		results = sub[ 0 ];
		sub[ 0 ] = NULL; }
	else {
		listItem *trail = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next ) {
			CNInstance *e = i->ptr;
			if ( !lookupIfThere( trail, e ) ) {
				addIfNotThere( &trail, e );
				for ( listItem *j=sub[1]; j!=NULL; j=j->next )
					e = db_instantiate( e, j->ptr, db );
				addItem( &results, e ); } }
		freeListItem( &trail ); }
	return results;
}

//---------------------------------------------------------------------------
//	instantiate_literal, instantiate_list
//---------------------------------------------------------------------------
static char *sequence_step( char *, CNInstance **, CNDB * );

static CNInstance *
instantiate_literal( char **position, CNDB *db )
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
	*position = p+1;
	return e;
}

static listItem *
instantiate_list( char **position, listItem **sub, CNDB *db )
/*
	Assumption: ((expression,...):_sequence_:)
	  start	*position -------^               ^
		 return *position ---------------
	where
		_sequence_ = ab...yz
	converts into (((((expression,*),a),b)...,y),z)
	  inserting star -------------^
*/
{
	if ( !sub[0] ) {
		*position = p_prune( PRUNE_LIST, *position );
		return NULL; }
	listItem *results = NULL;
	CNInstance *work_instance[ 3 ] = { NULL, NULL, NULL };
	CNInstance *star = db_register( "*", db );
	CNInstance *e;
	char *q = *position + 5; // start past "...):"
	char *p;
	while (( e = popListItem(&sub[0]) )) {
		CNInstance *f = star;
		e = db_instantiate( e, f, db );
		p = q;
		while ( strncmp( p, ":)", 2 ) ) {
			p = sequence_step( p, work_instance, db );
			f = work_instance[ 2 ];
			if (( f )) e = db_instantiate( e, f, db ); }
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
	int len;
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
			p = p_prune( PRUNE_IDENTIFIER, p );
			if ( *p=='\'' ) p++; }
		break;
	case '\\':
		switch (( q.value = p[1] )) {
		case '0':
		case 'w':
		case 'd':
		case 'i':
		case ' ':
			if ( !wi[1] ) wi[1] = db_register( "\\", db );
			e = db_register( q.s, db );
			e = db_instantiate( wi[1], e, db );
			p+=2; break;
		default: ;
			if (( len = charscan( p, &q ) )) {
				e = db_register( q.s, db );
				p += len; }
			else {
				e = db_register( q.s, db );
				p+=2; } }
		break;
	default:
		q.value = p[0];
		e = db_register( q.s, db );
		p++; }
	wi[ 2 ] = e;
	return p;
}

//---------------------------------------------------------------------------
//	instantiate_assignment
//---------------------------------------------------------------------------
#define bm_assign( sub, db ) \
	for ( listItem *i=sub[0], *j=sub[1]; (i)&&(j); i=i->next, j=j->next ) \
		db_assign( i->ptr, j->ptr, db ); \
	freeListItem( &sub[ 0 ] ); \
	freeListItem( &sub[ 1 ] );

static void
instantiate_assignment( char *expression, BMTraverseData *traverse_data, CNStory *story )
/*
	Note that the number of assignments actually performed is
		INF( cardinal(sub[0]), cardinal(sub[1]) )
	And that nothing is done with the excess, if there is -
		case sub[1]==NULL excepted
*/
{
	InstantiateData *data = traverse_data->user_data;
	CNDB *db = data->db;
	listItem *sub[ 2 ], *vector[ 2 ];
	CNInstance *e;

	char *p = expression+1, *q; // skip leading ':'
	if ( !strcmp( p, "~." ) ) {
		BMContext *ctx = data->ctx;
		CNInstance *self = BMContextSelf( ctx );
		db_unassign( self, db );
		return; }
	else if ( *p=='<' ) {
		vector[ 0 ] = NULL;
		for ( q=p; *q++!='>'; q=p_prune(PRUNE_TERM,q) )
			addItem( &vector[ 0 ], q );
		if ( q[1]=='<' ) {
//------------------

	/* here we have :< variables >:< expressions >
	*/
	vector[ 1 ] = NULL;
	for ( q++; *q++!='>'; q=p_prune(PRUNE_TERM,q) )
		addItem( &vector[ 1 ], q );
	while (( p=popListItem( &vector[0] ) )&&( q=popListItem(&vector[1]) )) {
		traverse_data->done = INFORMED|LITERAL;
		instantiate_traversal( p, traverse_data, FIRST );
		if ( traverse_data->done==2 || !data->sub[ 0 ] )
			continue;

		sub[ 0 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		if ( !strncmp( q, "~.", 2 ) ) {
			while (( e = popListItem( &sub[0] ) ))
				db_unassign( e, db ); }
		else {
			traverse_data->done = INFORMED|LITERAL;
			instantiate_traversal( q, traverse_data, FIRST );
			if ( traverse_data->done==2 || !data->sub[ 0 ] )
				freeListItem( &sub[ 0 ] );
			else {
				sub[ 1 ] = data->sub[ 0 ];
				data->sub[ 0 ] = NULL;
				bm_assign( sub, db ); } } }
	freeListItem( &vector[ 0 ] );
	freeListItem( &vector[ 1 ] );
	return; }

//------------------
		else freeListItem( &vector[0] ); }

	/* here we have either one of the following
		: state
		: variable(s) : ~.
		:< variables >: ~.
		: variable(s) : !! expression
		:< variables >: !! expression
		: variable(s) :< expressions >
		: variable(s) : expression(s)
		:< variables >: expression(s)
	*/
	traverse_data->done = INFORMED|LITERAL;
	p = instantiate_traversal( p, traverse_data, FIRST );
	if ( traverse_data->done==2 || !data->sub[ 0 ] )
		return;

	sub[ 0 ] = data->sub[ 0 ];
	data->sub[ 0 ] = NULL;
	if ( *p++=='\0' ) { // moving past ',' aka. ':' if there is
		BMContext *ctx = data->ctx;
		CNInstance *self = BMContextSelf( ctx );
		while (( e = popListItem( &sub[0] ) ))
			db_assign( self, e, db ); }
	else if ( !strncmp( p, "~.", 2 ) ) {
		while (( e = popListItem( &sub[0] ) ))
			db_unassign( e, db ); }
	else if ( *p=='!' ) {
		instantiate_new( p, story, traverse_data, &sub[ 0 ] ); }
	else if ( *p=='<' ) {
		for ( listItem *i=sub[ 0 ]; (i) && *p++!='>'; i=i->next ) {
			if ( !strncmp( p, "~.", 2 ) ) {
				db_unassign( i->ptr, db );
				p+=2; continue; }
			traverse_data->done = INFORMED|LITERAL;
			q = instantiate_traversal( p, traverse_data, FIRST );
			if ( traverse_data->done==2 || !data->sub[ 0 ] )
				p = p_prune( PRUNE_TERM, p );
			else {
				p = q;
				sub[ 1 ] = data->sub[ 0 ];
				db_assign( i->ptr, sub[ 1 ]->ptr, db );
				freeListItem( &sub[ 1 ] ); } }
		freeListItem( &sub[ 0 ] ); }
	else {
		traverse_data->done = INFORMED|LITERAL;
		p = instantiate_traversal( p, traverse_data, FIRST );
		if ( traverse_data->done==2 || !data->sub[ 0 ] )
			freeListItem( &sub[ 0 ] );
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = NULL;
			bm_assign( sub, db ); } }
}

//===========================================================================
//	bm_instantiate_input
//===========================================================================
void
bm_instantiate_input( char *input, char *arg, BMContext *ctx )
{
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate_input: %s, arg=%s ........{\n",
		( (input) ? input : "EOF" ), arg );
#endif
	listItem *sub[ 2 ];
	CNDB *db = BMContextDB( ctx );

	InstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = db;

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	traverse_data.done = INFORMED|LITERAL|CLEAN;
	char *p = instantiate_traversal( arg, &traverse_data, FIRST );
	if ( traverse_data.done==2 || !data.sub[ 0 ] )
		goto FAIL;

	sub[ 0 ] = data.sub[ 0 ];
	data.sub[ 0 ] = NULL;
	if ( input==NULL ) {
		CNInstance *e;
		while (( e = popListItem( &sub[0] ) ))
			db_unassign( e, db );
		return; }

	traverse_data.done = INFORMED|LITERAL;
	p = instantiate_traversal( input, &traverse_data, FIRST );
	if ( traverse_data.done==2 || !data.sub[ 0 ] )
		freeListItem( &sub[ 0 ] );
	else {
		sub[ 1 ] = data.sub[ 0 ];
		bm_assign( sub, db );
		freeListItem( &sub[ 0 ] );
		freeListItem( &sub[ 1 ] );
		return; }
FAIL:
	if ( traverse_data.done==2 ) {
		fprintf( stderr, ">>>>> B%%: Warning: input instantiation failed\n"
			"\t<<<<< arg=%s, input=%s, failed on '%s'\n", arg,
			( (input) ? input : "EOF" ), traverse_data.p );
		cleanup( &data, ctx ); }
	freeListItem( &data.sub[ 0 ] );
}

