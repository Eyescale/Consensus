#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "cell.h"
#include "eenov.h"
#include "errout.h"

// #define DEBUG

#include "instantiate.h"
#include "instantiate_private.h"
#include "instantiate_traversal.h"

//===========================================================================
//	bm_instantiate
//===========================================================================
static void bm_assign( char *, BMTraverseData *, CNStory * );
static void assign_new( listItem **, char *, CNStory *, BMTraverseData * );
static void assign_string( listItem **, char *, BMTraverseData * );
static inline void cleanup( BMTraverseData *, char * );

void
bm_instantiate( char *expression, BMContext *ctx, CNStory *story ) {
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate: %s ........{\n", expression );
#endif
	InstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = BMContextDB( ctx );

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;

	switch ( *expression ) {
	case ':': bm_assign( expression, &traversal, story ); break;
	case '!': assign_new( NULL, expression, story, &traversal ); break;
	case '"': assign_string( NULL, expression, &traversal ); break;
	default:
		traversal.done = INFORMED|LITERAL;
		instantiate_traversal( expression, &traversal, FIRST );
		cleanup( &traversal, expression ); } }

static inline void
cleanup( BMTraverseData *traversal, char *expression ) {
	InstantiateData *data = traversal->user_data;
	DBG_CLEANUP( data, expression );
	freeListItem( &data->sub[ 0 ] );
	traversal->done = 0; }

//---------------------------------------------------------------------------
//	instantiate_traversal
//---------------------------------------------------------------------------
static listItem *instantiate_couple( listItem *sub[2], CNDB * );
static CNInstance *instantiate_literal( char **, CNDB * );
static listItem *instantiate_list( char **, listItem **, CNDB * );
static listItem *instantiate_xpan( listItem **, CNDB * );

BMTraverseCBSwitch( instantiate_traversal )
case_( string_CB )
	// p is either $(s,?:...) or $((s,~.),?:...)
	CNDB *db = data->db;
	char *s = p[2]=='(' ? p+3 : p+2;
	CNInstance *e = db_lookup( 0, s, db );
	CNInstance *ste = db_arena_makeup( e, db,
		(data->carry) ? BMContextDB(data->carry) : db );
	if (( ste )) data->sub[ NDX ] = newItem( ste );
	if ( s==p+3 ) db_clear( e, 0, db );
	// skip "s,~.),?:...)" resp. "s,?:...)"
	p = p_prune( PRUNE_IDENTIFIER, s ) + ( s==p+3 ? 11 : 7 );
	_continue( p )
case_( filter_CB )
	/* Assumption: p at filter pipe :|
	   all other cases (byref) have been verified by parser
	   so as to be handled by collect_CB
	*/
	listItem *r = data->sub[ NDX ];
	if ( !r ) _prune( BMCB_LEVEL, p+2 )
	else if ( !is_f(PIPED) )
		switch_over( bgn_pipe_CB, p+1, p )
	else {
		r = bm_context_take( data->ctx, "|", r );
		if ( r!=data->results->ptr )
			freeListItem( &r ); }
	_break
case_( bgn_set_CB )
	if ( NDX ) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	addItem( &data->results, NULL );
	_break
case_( end_set_CB )
	/* Assumption:	!is_f(LEVEL|SUB_EXPR)
	*/
	listItem *instances = popListItem( &data->results );
	instances = catListItem( instances, data->sub[ 0 ] );
	if is_f_next( FIRST ) {
		data->sub[ 0 ] = instances; }
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_break
case_( bgn_pipe_CB )
	if ( p[1]==',' ) _return( 1 );
	listItem *r;
	if is_f( PIPED ) { // keep %| as-it-is in this case
		// and discard previous pipe_expr results
		freeListItem( &data->sub[ NDX ] );
		_break }
	else if (( r=data->sub[ NDX ] )) {
		bm_context_push( data->ctx, "|", r );
		if is_f( FIRST ) // backup results
			addItem( &data->results, r );
		else {
			addItem( &data->results, data->sub[ 0 ] );
			addItem( &data->results, r );
			data->sub[ 1 ] = NULL; }
		data->sub[ 0 ] = NULL;
		_break }
	else _prune( BMCB_LEVEL, p+1 )
case_( end_pipe_CB )
	listItem *r, *pprv = bm_context_pop( data->ctx, "|" );
	freeListItem( &data->sub[ 0 ] );
	if is_f_next( FIRST )
		data->sub[ 0 ] = r = popListItem( &data->results );
	else {	data->sub[ 1 ] = r = popListItem( &data->results );
		data->sub[ 0 ] = popListItem( &data->results ); }
	if ( pprv!=r ) freeListItem( &pprv );
	_break
case_( open_CB )
	if ( !is_f_next(DOT) && !is_f(FIRST) ) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	_break
case_( close_CB )
	listItem *instances;
	if ( !data->sub[ 0 ] ) {
		instances = data->sub[ 1 ];
		data->sub[ 1 ] = NULL; }
	else if is_f( FIRST )
		instances = data->sub[ 0 ];
	else if ( !data->sub[ 1 ] ) {
		freeListItem( &data->sub[ 0 ] );
		instances = NULL; }
	else {
		BMContext *carry = data->carry;
		CNDB *db = (( carry )? BMContextDB(carry) : data->db );
		instances = is_f(ELLIPSIS) ?
			instantiate_xpan( data->sub, db ) :
			instantiate_couple( data->sub, db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if (( instances )) {
		if ( is_f(DOT) ) {
			// case carry here handled in dot_expression_CB
			CNInstance *perso = BMContextPerso( data->ctx );
			data->sub[ 0 ] = newItem( perso );
			data->sub[ 1 ] = instances;
			instances = instantiate_couple( data->sub, data->db );
			freeListItem( &data->sub[ 0 ] );
			freeListItem( &data->sub[ 1 ] ); }
		if is_f_next( FIRST )
			data->sub[ 0 ] = instances;
		else {
			data->sub[ 0 ] = popListItem( &data->results );
			data->sub[ 1 ] = instances; } }
	else {
		if ( !is_f_next(FIRST) ) {
			instances = popListItem( &data->results );
			freeListItem( &instances ); }
		_prune( BMCB_LEVEL_DOWN, p ) }
	_break
case_( loop_CB )
	// called past ) and } => test for a respin
	LoopData *loop = ((data->loop) ? data->loop->ptr : NULL );
	if (( loop )&&( loop->bgn->level==*traversal->stack )) {
		CNInstance *e;
		listItem **restore = &loop->end->results;
		listItem **results = &data->sub[ NDX ];
		if (( bm_context_check( data->ctx, loop->end->mark ) )) {
			if ( strncmp(p,":|",2) ) {
				while (( e=popListItem(results) ))
					addItem( restore, e ); }
			else if (( *results )) {
				listItem *pprv = bm_context_take( data->ctx, "|", *results );
				if (( pprv )&&( *restore ))
					freeListItem( &pprv );
				else freeListItem( restore );
				*restore = *results;
				*results = NULL; }
			_continue( loop->bgn->p ) }
		else if ( *restore ) {	// last iteration done
			if ( !strncmp(p,":|",2) ) {
				if ( *results ) {
					bm_context_take( data->ctx, "|", *results );
					*results = NULL; }
				freeListItem( restore ); }
			else {
				while (( e=popListItem(results) ))
					addItem( restore, e );
				*results = *restore;
				*restore = NULL; } }
		loopPop( data, 1 ); }
	else if ( !strncmp(p+1,":|",2) )
		switch_over( filter_CB, p+1, p )
	_break
case_( activate_CB )
	ActiveRV *active = BMContextActive( data->ctx );
	listItem **buffer = ( *p=='@' ) ?
		&active->buffer->activated :
		&active->buffer->deactivated;
	for ( listItem *i=data->sub[0]; i!=NULL; i=i->next ) {
		CNInstance *e = i->ptr;
		if ( cnIsProxy(e) && !cnIsSelf(e) )
			addIfNotThere( buffer, e ); }
	_break
case_( collect_CB )
	if ( !strncmp(p,"~(",2) && is_f(SET) && !data->carry && !is_f(VECTOR|SUB_EXPR|LEVEL) )
		bm_release( p+1, data->ctx );
	else if ( !strncmp(p,"*^%",3) ) {
		listItem *results = bm_scan( p+2, data->ctx );
		Pair *entry = registryLookup( data->ctx, ":" );
		if (( entry )) {
			Registry *buffer = entry->value;
			for ( listItem *i=results, *last_i=NULL, *next_i; i!=NULL; i=next_i ) {
				next_i = i->next;
				if (( entry=registryLookup(buffer,i->ptr) )) {
					i->ptr = entry->value;
					last_i = i; }
				else clipListItem( &results, i, last_i, next_i ); }
			if (( results ))
				data->sub[ NDX ] = bm_inform( data->carry, &results, data->db ); }
		else freeListItem( &results ); }
	else {
		listItem *results = bm_scan( p, data->ctx );
		data->sub[ NDX ] = bm_inform( data->carry, &results, data->db ); }
	_prune( BMCB_TERM, p )
case_( comma_CB )
	if (!is_f(LEVEL|SUB_EXPR)) { // Assumption: is_f(SET|VECTOR)
		listItem **results = &data->results;
		listItem *instances = popListItem( results );
		addItem( results, catListItem( instances, data->sub[ 0 ] ));
		data->sub[ 0 ] = NULL; }
	else if ( !data->sub[ 0 ] ) {
		_prune( BMCB_LEVEL, p+1 ) }
	else if ( !strncmp( p+1, "~.)", 3 ) ) {
		CNDB *db = data->db;
		for ( listItem *i=data->sub[0]; i!=NULL; i=i->next )
			db_clear( i->ptr, 0, db );
		data->sub[ 1 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		_continue( p+3 ) }
	_break
case_( wildcard_CB )
	switch ( *p ) {
	case '.':
		data->sub[ NDX ] = newItem( NULL );
		break;
	case '?':
		if ( !strncmp(p+1,"::",2) ) {
			CNInstance *e = BMContextRVV( data->ctx, p+3 );
			char *o = db_arena_origin( e );
			if (( o )) switch_over( identifier_CB, o, p )
			o = p_prune( PRUNE_IDENTIFIER, p+3 );
			if ( !strncmp(o,"::",2) ) _continue( o+2 )
			break; }
		p+=2; // FORE loop bgn
		listItem *results = bm_scan( p, data->ctx );
		if (( results )) {
			BMMark *mark = bm_mark( p, NULL, AS_PER|QMARK, results );
			listItem *level = *traversal->stack;
			p = p_prune( PRUNE_TERM, p );
			Pair *bgn = newPair( level, p );
			Pair *end = newPair( mark, NULL );
			addItem( &data->loop, newPair( bgn, end ));
			bm_context_mark( data->ctx, mark );
			_continue( p ) }
		else {
			_prune( BMCB_LEVEL, p ) } }
	_break
case_( register_variable_CB )
	BMContext *carry = data->carry;
	CNDB *db = data->db;
	listItem *found;
	CNInstance *e;
	switch ( p[1] ) {
	case '|':
		found = BMContextRVV( data->ctx, p );
		if ( !found ) _prune( BMCB_LEVEL, p )
		listItem **sub = &data->sub[ NDX ];
		if ( strncmp(p+2,"::",2) ) {
			for ( listItem *i=found; i!=NULL; i=i->next )
				addItem( sub, i->ptr ); }
		else {
			listItem *xpn = xpn_make(p+4);
			for ( listItem *i=found; i!=NULL; i=i->next )
				if (( e=xpn_sub(i->ptr,xpn) ))
					addItem( sub, e );
			freeListItem( &xpn ); }
		break;
	case '<':
		found = eenov_inform( data->ctx, db, p, carry );
		data->sub[ NDX ] = found;
		break;
	case '?':
	case '!':
		if ( p[2]=='~' ) {
			e = BMContextRVV( data->ctx, p );
			e = bm_translate( carry, e, db, 1 );
			if ( !e ) _prune( BMCB_LEVEL, p )
			data->sub[ NDX ] = newItem( e );
			_break }
	default:
		switch_over( collect_CB, p, p ) }
	_break
case_( literal_CB )
	/* We have	(:_sequence_:)
	   start p -----^             ^
		return p=*q ----------
	*/
	BMContext *carry = data->carry;
	CNDB *db = (( carry )? BMContextDB(carry) : data->db );
	CNInstance *e = instantiate_literal( q, db );
	data->sub[ NDX ] = newItem( e );
	_break
case_( list_CB )
	/* We have ((expression,...):_sequence_:)
	   start p -------------^               ^
		     return p=*q ---------------
	*/
	BMContext *carry = data->carry;
	data->sub[ 0 ] = (( carry )?
		instantiate_list( q, data->sub, BMContextDB(carry) ) :
		instantiate_list( q, data->sub, data->db ));
	_break
case_( dot_expression_CB )
	BMContext *carry = data->carry;
	if (( carry )) { // null-carry case handled in close_CB
		listItem *results = bm_scan( p+1, data->ctx );
		data->sub[ NDX ] = bm_inform( carry, &results, data->db );
		p = p_prune( PRUNE_TERM, p+1 );
		_continue( p ) }
	_break
case_( dot_identifier_CB )
	BMContext *ctx = data->ctx;
	BMContext *carry = data->carry;
	CNDB *db = data->db;
	CNInstance *e = NULL;
	if (( carry )) {
		// Special case: looking for locale here
		if (( e=BMContextRVV( ctx, p+1 ) ))
			e = bm_translate( carry, e, db, 1 ); }
	else {
		CNInstance *perso = BMContextPerso( ctx );
		e = bm_register( ctx, p+1, db );
		e = db_instantiate( perso, e, db ); }
	if (( e )) data->sub[ NDX ] = newItem( e );
	_break
case_( identifier_CB )
	BMContext *carry = data->carry;
	CNDB *db = (( carry )? BMContextDB(carry) : data->db );
	if (( carry )) {
		CNInstance *e = bm_register( NULL, p, db );
		data->sub[ NDX ] = newItem( e ); }
	else {
		CNInstance *e = bm_register( data->ctx, p, db );
		data->sub[ NDX ] = newItem( e ); }
	_break
case_( signal_CB )
	CNInstance *e = data->sub[ NDX ]->ptr;
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
*/ {
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
	return results; }

static inline void
couple( CNInstance *e, CNInstance *f, CNDB *db, listItem **trail, listItem **results ) {
	CNInstance *g = db_instantiate( e, f, db );
	if (( g )&&( addIfNotThere( trail, g ) )) {
		addItem( results, g ); } }

//---------------------------------------------------------------------------
//	instantiate_xpan
//---------------------------------------------------------------------------
static listItem *
instantiate_xpan( listItem *sub[2], CNDB *db )
/*
	Assuming we had expression:((sub[0],...),sub[1])
		where sub[1] is { a, b, ..., z }
	converts into ((((sub[0],a),b),...),z)
*/ {
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
			if (( addIfNotThere( &trail, e ) )) {
				for ( listItem *j=sub[1]; j!=NULL; j=j->next )
					e = db_instantiate( e, j->ptr, db );
				addItem( &results, e ); } }
		freeListItem( &trail ); }
	return results; }

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
*/ {
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
		if (( e )) addItem( &stack, e ); }
RETURN:
	*position = p+1;
	return e; }

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
*/ {
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
	return results; }

static char *
sequence_step( char *p, CNInstance **wi, CNDB *db )
/*
	wi means work-instance(s)
	wi[ 0 ]	represents Mod (%)
	wi[ 1 ] represents Backslash (\\)
	wi[ 2 ]	represents return value
*/ {
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
	return p; }

//===========================================================================
//	bm_assign
//===========================================================================
static int assign_v2v( char *, BMTraverseData * );
static inline void assign_one2v( listItem **, char *, BMTraverseData * );
static inline void assign_v2one( listItem **, char *, BMTraverseData *, char * );

static void
bm_assign( char *expression, BMTraverseData *traversal, CNStory *story )
/*
	start with handling the special cases
		:< variables >:< expressions >
		:~.
	then either one of the following
		:_
		:_: !! expression
		:_: "string"
		:_: < expressions >	// ~. included
		:_: expression(s)	// ~. included

	Note that the number of assignments actually performed is
		INF( cardinal(sub[0]), cardinal(sub[1]) )
	And that nothing is done with the excess, if there is -
		case sub[1]==NULL excepted
*/ {
	if ( assign_v2v( expression, traversal ) )
		return;
	CNInstance *self;
	InstantiateData *data = traversal->user_data;
	char *p = expression + 1; // skip leading ':'
	if ( !strcmp( p, "~." ) ) {
		self = BMContextSelf( data->ctx );
		db_unassign( self, data->db );
		return; }
	// other assignment cases
	ifn_instantiate_traversal( p, ASSIGN )
		cleanup( traversal, expression );
	else switch ( *p ) {
	case '\0':
		self = BMContextSelf( data->ctx );
		db_assign( self, data->sub[0]->ptr, data->db );
		freeListItem( &data->sub[0] );
		break;
	case '|': // Assumption we have "|:_"
		p++; // moving past "|"
		self = BMContextSelf( data->ctx );
		db_assign( self, data->sub[0]->ptr, data->db );
		if ( !strcmp( p+1, "~." ) ) {
			db_unassign( data->sub[0]->ptr, data->db );
			freeListItem( &data->sub[0] );
			break; }
		// no break
	default:
		p++; // moving past ',' aka. ':'
		listItem *sub[ 2 ]; // argument to BM_ASSIGN
		sub[ 0 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		switch ( *p ) {
		case '!': assign_new( sub, p, story, traversal ); break;
		case '"': assign_string( sub, p, traversal ); break;
		case '<': assign_one2v( sub, p, traversal ); break;
		default:
			assign_v2one( sub, p, traversal, expression ); } } }

//---------------------------------------------------------------------------
//	assign_v2v, assign_v2one, assign_one2v
//---------------------------------------------------------------------------
static int
assign_v2v( char *expression, BMTraverseData *traversal ) {
	char *q, *p=expression+1; // skip leading ':'
	if ( *p!='<' ) return 0;
	listItem *vector[ 2 ] = { NULL, NULL };
	for ( q=p; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
		addItem( &vector[ 0 ], q );
	if ( q[1]!='<' ) {
		freeListItem( &vector[ 0 ] );
		return 0; }
	for ( q++; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
		addItem( &vector[ 1 ], q );

	InstantiateData *data = traversal->user_data;
	CNDB *db = data->db;
	listItem *sub[ 2 ];
	CNInstance *e;
	while (( p=popListItem( &vector[0] ) )&&( q=popListItem(&vector[1]) )) {
		ifn_instantiate_traversal( p, 0 ) {
			cleanup( traversal, NULL );
			continue; }
		sub[ 0 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		assign_v2one( sub, q, traversal, NULL ); }
	freeListItem( &vector[ 0 ] );
	freeListItem( &vector[ 1 ] );
	return 1; }

static inline void assign_tag( listItem **, char *, CNDB *, BMContext * );
static inline void
assign_v2one( listItem **sub, char *p, BMTraverseData *traversal, char *expression ) {
	InstantiateData *data = traversal->user_data;
	CNDB *db = data->db;
	if ( !strncmp( p, "~.", 2 ) )
		BM_UNASSIGN( sub, db )
	else if ( *p=='^' )
		assign_tag( sub, p+1, db, data->ctx );
	else {
		ifn_instantiate_traversal( p, 0 ) {
			cleanup( traversal, expression );
			freeListItem( &sub[ 0 ] ); }
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = NULL;
			BM_ASSIGN( sub, db ); } } }
static inline void
assign_tag( listItem **sub, char *p, CNDB *db, BMContext *ctx ) {
	if ( *p_prune(PRUNE_IDENTIFIER,p)!='~' ) return;
	Pair *entry = BMTag( ctx, p );
	if ( !entry ) BM_UNASSIGN( sub, db )
	else {
		CNInstance *e, *f;
		listItem **list = (listItem **) &entry->value;
		while((e=popListItem(&sub[0]))&&(f=popListItem(list)))
			db_assign( e, f, db );
		if (( *list )) freeListItem( &sub[0] );
		else {
			bm_untag( ctx, p );
			BM_UNASSIGN( sub, db ) } } }

static inline void
assign_one2v( listItem **sub, char *p, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	CNDB *db = data->db;
	for ( listItem *i=sub[ 0 ]; (i) && *p++!='>'; i=i->next ) {
		if ( !strncmp( p, "~.", 2 ) ) {
			db_unassign( i->ptr, db );
			p+=2; continue; }
		ifn_instantiate_traversal( p, 0 )
			cleanup( traversal, NULL );
		else {
			db_assign( i->ptr, data->sub[ 0 ]->ptr, db );
			freeListItem( &data->sub[ 0 ] ); } }
	freeListItem( &sub[ 0 ] ); }

//---------------------------------------------------------------------------
//	assign_new
//---------------------------------------------------------------------------
static void inform_UBE( Registry *, char *, BMTraverseData * );
static inline CNInstance *carry( CNDB *, CNCell *, Registry *, Pair * );
static void inform_carry( Registry *, char *, CNCell *, BMTraverseData * );

static void
assign_new( listItem **list, char *p, CNStory *story, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	CNInstance *x, *ube, *proxy;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	p += 2; // skip '!!'
	//-----------------------------------------------------------
	//	UBE assignment
	//-----------------------------------------------------------
	if ( !*p ) { // do :_: !!
		while (( x=popListItem( list ))) {
			ube = db_arena_register( NULL, db );
			db_assign( x, ube, db ); }
		return; }
	Registry *buffer = newRegistry( IndexedByAddress );
	registryRegister( ctx, ":", buffer );
	Pair *entry;
	if ( *p=='|' ) {
		if ( !list ) {
			ube = db_arena_register( NULL, db );
			registryRegister( buffer, NULL, ube ); }
		else while (( x=popListItem( list ) )) {
			ube = db_arena_register( NULL, db );
			registryRegister( buffer, x, ube ); }
		p++; // skip '|'
		inform_UBE( buffer, p, traversal ); }
	//-----------------------------------------------------------
	//	carry assignment
	//-----------------------------------------------------------
	else if ( *p=='(' ) { // class-actor assignment
		CNCell *this = BMContextCell( ctx );
		while (( x=popListItem(list) )) {
			if ( !cnIsIdentifier(x) ) continue;
			entry = registryLookup( story, CNIdentifier(x) );
			if ( !entry ) continue;
			proxy = carry( db, this, story, entry );
			registryRegister( buffer, x, proxy ); }
		if ( !strncmp( p, "()", 2 ) ) p+=2;
		inform_carry( buffer, p, this, traversal ); }
	else if (( entry=registryLookup(story,p) )) {
		CNCell *this = BMContextCell( ctx );
		if ( !list ) {
			proxy = carry( db, this, story, entry );
			registryRegister( buffer, NULL, proxy ); }
		else while (( x=popListItem(list) )) {
			proxy = carry( db, this, story, entry );
			registryRegister( buffer, x, proxy ); }
		p = p_prune( PRUNE_IDENTIFIER, p );
		if ( !strncmp( p, "()", 2 ) ) p+=2;
		inform_carry( buffer, p, this, traversal ); }
	else {
		if ( !list ) errout( InstantiateClassNotFound, p );
		else {	errout( InstantiateClassNotFoundv, p );
			freeListItem( list ); } }
	registryDeregister( ctx, ":" );
	freeRegistry( buffer, NULL ); }

static inline CNInstance *
carry( CNDB *db, CNCell *this, Registry *story, Pair *entry ) {
	CNCell *child = newCell( story, entry );
	addItem( BMCellCarry(this), child );
	return new_proxy( this, child, db ); }

static void
inform_UBE( Registry *buffer, char *p, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	char *start_p = p;
	listItem *pipe_mark = newItem( NULL );
	bm_context_push( ctx, "|", pipe_mark );	
	// inform each UBE's connections
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next, p=start_p ) {
		Pair *entry = i->ptr;
		bm_context_push( ctx, "^", entry );
		pipe_mark->ptr = entry->value;
		ifn_instantiate_traversal( p, 0 )
			cleanup( traversal, NULL );
		else freeListItem( &data->sub[ 0 ] );
		bm_context_pop( ctx, "^" ); }
	bm_context_pop( ctx, "|" );
	freeItem( pipe_mark );
	// assign or deregister UBE depending on connected
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *x = entry->name;
		CNInstance *ube = entry->value;
		if ( !x && !CONNECTED(ube) ) {
			errout( InstantiateUBENotConnected, start_p );
			db_arena_deregister( ube, db ); }
		else if (( x )) db_assign( x, ube, db ); } }

static void
inform_carry( Registry *buffer, char *p, CNCell *this, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	char *start_p = p;
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next, p=start_p ) {
		Pair *entry = i->ptr;
		bm_context_push( ctx, "^", entry );
		CNInstance *x = entry->name;
		CNInstance *proxy = entry->value;
		CNCell *child = CNProxyThat( proxy );
		data->carry = BMCellContext( child );
		if ( *p=='(' ) {
			p++;
			do {	ifn_instantiate_traversal( p, CARRY )
					cleanup( traversal, NULL );
				else freeListItem( &data->sub[ 0 ] );
			} while ( *p++!=')' ); }
		if ( *p=='~' ) {
			if (( x )) db_unassign( x, db );
			db_deprecate( proxy, db ); }
		else {
			if (( x )) db_assign( x, proxy, db );
			bm_cell_bond( this, child, proxy ); }
		bm_context_pop( ctx, "^" ); } }

//---------------------------------------------------------------------------
//	assign_string
//---------------------------------------------------------------------------
static void
assign_string( listItem **list, char *s, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	CNDB *db = data->db;
	CNInstance *e = db_arena_register( s, db );
	if (( list )) {
		for ( CNInstance *x;(x=popListItem( list ));)
			db_assign( x, e, db ); } }

//===========================================================================
//	bm_instantiate_input
//===========================================================================
static int instantiate_input( char *, char *, BMTraverseData * );

void
bm_instantiate_input( char *arg, char *input, BMContext *ctx ) {
#ifdef DEBUG
	fprintf( stderr, "bm_instantiate_input: %s, arg=%s ........{\n",
		( (input) ? input : "EOF" ), arg );
#endif
	InstantiateData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = BMContextDB( ctx );

	BMTraverseData traversal;
	traversal.user_data = &data;
	traversal.stack = &data.stack.flags;

	if ( !instantiate_input( arg, input, &traversal ) ) return;
	errout( InstantiateInput, arg, ((input)?input:"EOF") );
	cleanup( &traversal, NULL ); }

static int
instantiate_input( char *arg, char *input, BMTraverseData *traversal ) {
	InstantiateData *data = traversal->user_data;
	CNDB *db = data->db;
	listItem *sub[ 2 ];
	char *p;
	ifn_instantiate_traversal( arg, 0 )
		return -1;
	sub[ 0 ] = data->sub[ 0 ];
	data->sub[ 0 ] = NULL;
	if ( input==NULL )
		BM_UNASSIGN( sub, db )
	else {
		ifn_instantiate_traversal( input, 0 ) {
			freeListItem( &sub[ 0 ] );
			return -1; }
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = NULL;
	 		BM_ASSIGN( sub, db ); } }
	return 0; }

