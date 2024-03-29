#include <stdio.h>
#include <stdlib.h>

#include "cell.h"
#include "instantiate.h"
#include "eenov.h"
#include "errout.h"

// #define DEBUG

//===========================================================================
//	instantiate_traversal
//===========================================================================
#include "instantiate_traversal.h"
#include "instantiate_private.h"

static listItem *instantiate_couple( listItem *sub[2], CNDB * );
static CNInstance *instantiate_literal( char **, CNDB * );
static listItem *instantiate_list( char **, listItem **, CNDB * );
static listItem *instantiate_xpan( listItem **, CNDB * );

BMTraverseCBSwitch( instantiate_traversal )
case_( filter_CB )
	errout( InstantiateFiltered, p );
	_prune( BM_PRUNE_TERM, p+1 )
case_( bgn_set_CB )
	if ( NDX ) {
		addItem( &data->results, data->sub[ 0 ] );
		data->sub[ 0 ] = NULL; }
	addItem( &data->results, NULL );
	add_item( &data->newborn.stack, data->newborn.current );
	data->newborn.current = 0;
	_break
case_( end_set_CB )
	/* Assumption:	!is_f(LEVEL|SUB_EXPR)
	*/
	listItem *instances = popListItem( &data->results );
	instances = catListItem( instances, data->sub[ 0 ] );
	if ( !instances ) {
		popListItem( &data->stack.flags );
		_prune( BM_PRUNE_LEVEL, p+1 ) }
	else if ( f_next & FIRST ) {
		data->sub[ 0 ] = instances; }
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	data->newborn.current = pop_item( &data->newborn.stack );
	_break
case_( bgn_pipe_CB )
	if ( !data->sub[ NDX ] ) _prune( BM_PRUNE_LEVEL, p+1 )
	bm_push_mark( data->ctx, "|", data->sub[ NDX ] );
	if ( is_f(FIRST) ) // backup results
		addItem( &data->results, data->sub[ 0 ] );
	else {
		addItem( &data->results, data->sub[ 0 ] );
		addItem( &data->results, data->sub[ 1 ] );
		data->sub[ 1 ] = NULL; }
	data->sub[ 0 ] = NULL;
	add_item( &data->newborn.stack, data->newborn.current );
	data->newborn.current = 0;
	_break
case_( end_pipe_CB )
	bm_pop_mark( data->ctx, "|" );
	freeListItem( &data->sub[ 0 ] );
	if ( f_next&FIRST ) // restore results
		data->sub[ 0 ] = popListItem( &data->results );
	else {	data->sub[ 1 ] = popListItem( &data->results );
		data->sub[ 0 ] = popListItem( &data->results ); }
	data->newborn.current = pop_item( &data->newborn.stack );
	_break
case_( open_CB )
	if ( !(f_next&DOT) ) {
		if ( NDX ) {
			addItem( &data->results, data->sub[ 0 ] );
			data->sub[ 0 ] = NULL; } }
	_break
case_( close_CB )
	listItem *instances;
	if ( !data->sub[ 0 ] ) {
		instances = data->sub[ 1 ];
		data->sub[ 1 ] = NULL; }
	else if ( !NDX )
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
	if ( !instances ) {
		popListItem( &data->stack.flags );
		_prune( BM_PRUNE_LEVEL, p+1 ) }
	if ( is_f(DOT) ) {
		// case carry here handled in dot_expression_CB
		CNInstance *perso = BMContextPerso( data->ctx );
		data->sub[ 0 ] = newItem( perso );
		data->sub[ 1 ] = instances;
		instances = instantiate_couple( data->sub, data->db );
		freeListItem( &data->sub[ 0 ] );
		freeListItem( &data->sub[ 1 ] ); }
	if ( f_next & FIRST )
		data->sub[ 0 ] = instances;
	else {
		data->sub[ 0 ] = popListItem( &data->results );
		data->sub[ 1 ] = instances; }
	_break
case_( newborn_CB )
	switch ( data->newborn.current ) {
	case 1: break;
	default:
		if ( db_has_newborn( data->sub[NDX], data->db ) )
			data->newborn.current = 1;
		else {	data->newborn.current = -1; } }
	_break
case_( loop_CB )
	// called past ) and } => test for a respin
	if (( *p!='|' )&&( data->loop )) {
		LoopData *loop = data->loop->ptr;
		if ( loop->info->level==*traverse_data->stack ) {
			if (( bm_context_remark( data->ctx, loop->mark ) ))
				_continue( loop->info->p )
			else {
				popListItem( &data->loop );
				freePair((Pair *) loop->info );
				freePair((Pair *) loop ); } } }
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
case_( collect_CB )
	listItem *results = bm_scan( p, data->ctx );
	p = p_prune( PRUNE_TERM, p+1 );
	int *nb = ( *p=='^' ?( p++, &data->newborn.current ): NULL );
	data->sub[ NDX ] = bm_list_inform( data->carry, &results, data->db, nb );
	_continue( p )
case_( decouple_CB )
	if (!is_f(LEVEL|SUB_EXPR)) { // Assumption: is_f(SET|VECTOR)
		listItem **results = &data->results;
		listItem *instances = popListItem( results );
		addItem( results, catListItem( instances, data->sub[ 0 ] ));
		data->sub[ 0 ] = NULL; }
	else if ( !data->sub[ 0 ] )
		_prune( BM_PRUNE_LEVEL, p+1 )
	else if ( !strncmp( p+1, "~.)", 3 ) ) {
		db_clear( data->sub[ 0 ], data->db );
		data->sub[ 1 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		_continue( p+3 ) }
	_break
case_( wildcard_CB )
	switch ( *p ) {
	case '!':
		if ( p[1]=='^' ) {
			if ( !newborn_authorized( data ) )
				_prune( BM_PRUNE_TERM, p+3 )
			else break; }
		else { // p[1]=='?'
			if ( !newborn_authorized( data ) )
				_prune( BM_PRUNE_LEVEL, p+3 )
			p++; } // no break
	case '?':
		p+=2;
		MarkData *mark = markscan( p, data->ctx );
		if ( !mark ) _prune( BM_PRUNE_LEVEL, p )
		else {
			p = p_prune( PRUNE_TERM, p );
			Pair *info = newPair( *traverse_data->stack, p );
			addItem( &data->loop, newPair( info, mark ) );
			bm_context_mark( data->ctx, mark );
			_continue( p ) }
	case '.':
		data->sub[ NDX ] = newItem( NULL ); }
	_break
case_( register_variable_CB )
	BMContext *carry = data->carry;
	CNDB *db = data->db;
	listItem *found;
	listItem **sub;
	CNInstance *e;
	switch ( p[1] ) {
	case '<': ;
		found = eenov_inform( data->ctx, db, p, carry );
		data->sub[ NDX ] = found;
		break;
	case '|':
		found = bm_context_lookup( data->ctx, p );
		if ( !found ) _prune( BM_PRUNE_LEVEL, p )
		sub = &data->sub[ NDX ];
		listItem *xpn = ( p[2]==':' ? subx(p+3) : NULL );
		for ( listItem *i=found; i!=NULL; i=i->next )
			if (( e=xsub(i->ptr,xpn) )) {
				e = bm_inform( carry, e, db );
				if (( e )) addItem( sub, e ); }
		freeListItem( &xpn );
		break;
	case '?':
	case '!':
		e = bm_context_lookup( data->ctx, p );
		if (( e )&&( p[2]==':' )) {
			listItem *xpn = subx(p+3);
			e = xsub( e, xpn );
			freeListItem( &xpn ); }
		if (( e )) e = bm_inform( carry, e, db );
		if (( e )) data->sub[ NDX ] = newItem( e );
		else _prune( BM_PRUNE_LEVEL, p );
		break;
	default:
		if ( is_separator(p[1]) && p[1]!='@' ) {
			e = bm_context_lookup( data->ctx, p );
			if (( e )) e = bm_inform( carry, e, db );
			if (( e )) data->sub[ NDX ] = newItem( e );
			else _prune( BM_PRUNE_LEVEL, p ) }
		else {
			found = bm_context_lookup( data->ctx, p );
			if ( !found ) _prune( BM_PRUNE_LEVEL, p+2 )
			sub = &data->sub[ NDX ];
			for ( listItem *i=found; i!=NULL; i=i->next ) {
				e = bm_inform( carry, i->ptr, db );
				if (( e )) addItem( sub, e ); } } }
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
		p = p_prune( PRUNE_TERM, p+1 );
		int *nb = ( *p=='^' ?( p++, &data->newborn.current ): NULL );
		data->sub[ NDX ] = bm_list_inform( carry, &results, data->db, nb );
		_continue( p ) }
	_break
case_( dot_identifier_CB )
	BMContext *ctx = data->ctx;
	BMContext *carry = data->carry;
	CNDB *db = data->db;
	CNInstance *e = NULL;
	if (( carry )) {
		// Special case: looking for locale here
		if (( e=bm_context_lookup( ctx, p+1 ) ))
			e = bm_inform( carry, e, db ); }
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
	if ((g) && !lookupIfThere( *trail, g ) ) {
		addIfNotThere( trail, g );
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
			if ( !lookupIfThere( trail, e ) ) {
				addIfNotThere( &trail, e );
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
//	bm_instantiate
//===========================================================================
static void bm_assign( char *, BMTraverseData *, CNStory * );
static void assign_new( listItem **, char *, CNStory *, BMTraverseData * );
static void assign_string( listItem **, char *, CNStory *, BMTraverseData * );
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

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	switch ( *expression ) {
	case ':': bm_assign( expression, &traverse_data, story ); break;
	case '!': assign_new( NULL, expression, story, &traverse_data ); break;
	case '"': assign_string( NULL, expression, story, &traverse_data ); break;
	default:
		traverse_data.done = INFORMED|LITERAL;
		instantiate_traversal( expression, &traverse_data, FIRST );
		cleanup( &traverse_data, expression ); } }

static inline void
cleanup( BMTraverseData *traverse_data, char *expression ) {
	InstantiateData *data = traverse_data->user_data;
	if ( !data->sub[0] && ( expression ))
		errout( InstantiatePartial, expression );
#ifdef DEBUG
	if ((data->sub[1]) || (data->stack.flags) || (data->results) ||
	    (bm_context_lookup( data->ctx, "%|" ))) {
		fprintf( stderr, ">>>>> B%%: Error: bm_instantiate: memory leak" );
		if (( expression )) {
			fprintf( stderr, "\t\tdo %s\n\t<<<<< failed", expression );
			if ( !strmatch(":!\"", *expression ) )
				fprintf( stderr, " in expression\n" ); }
			else	fprintf( stderr, " in assignment\n" ); }
		else fprintf( stderr, " in assignment\n" );
		fprintf( stderr, " on:" );
		if ((data->sub[1])) {
			fprintf( stderr, " data->sub[ 1 ]" );
			if (!data->sub[0]) fprintf( stderr, "/!data->sub[ 0 ]" ); }
		if ((data->stack.flags)) fprintf( stderr, " data->stack.flags" );
		if ((data->results)) fprintf( stderr, " data->results" );
		if ((bm_context_lookup(data->ctx,"%|"))) fprintf( stderr, " %%|" );
		fprintf( stderr, "\n" );

		freeListItem( &data->sub[ 1 ] );
		freeListItem( &data->stack.flags );
		listItem *instances;
		listItem **results = &data->results;
		while (( instances = popListItem(results) ))
			freeListItem( &instances );
		bm_context_pipe_flush( data->ctx );
		exit( -1 ); }
#endif
	freeListItem( &data->sub[ 0 ] );
	traverse_data->done = 0; }

//===========================================================================
//	bm_assign
//===========================================================================
static int assign_v2v( char *, BMTraverseData * );
static inline void assign_one2v( listItem **, char *, BMTraverseData * );
static inline void assign_v2one( listItem **, char *, BMTraverseData *, char * );

static void
bm_assign( char *expression, BMTraverseData *traverse_data, CNStory *story )
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
	InstantiateData *data = traverse_data->user_data;
	char *p = expression+1; // skip leading ':'
	// special cases
	if ( *p=='<' && assign_v2v( expression, traverse_data ) )
		return;
	else if ( !strcmp( p, "~." ) ) {
		CNInstance *self = BMContextSelf( data->ctx );
		db_unassign( self, data->db );
		return; }
	// other assignment cases
	ifn_instantiate_traversal( p, 0 )
		cleanup( traverse_data, expression );
	else if ( !*p++ ) { // moving past ',' aka. ':' if there is
		CNInstance *self = BMContextSelf( data->ctx );
		db_assign( self, data->sub[0]->ptr, data->db ); }
	else {
		listItem *sub[ 2 ]; // argument to BM_ASSIGN
		sub[ 0 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		switch ( *p ) {
		case '!': assign_new( sub, p, story, traverse_data ); break;
		case '"': assign_string( sub, p, story, traverse_data ); break;
		case '<': assign_one2v( sub, p, traverse_data ); break;
		default:
			assign_v2one( sub, p, traverse_data, expression ); } } }

//---------------------------------------------------------------------------
//	assign_v2v, assign_v2one, assign_one2v
//---------------------------------------------------------------------------
static int
assign_v2v( char *expression, BMTraverseData *traverse_data ) {
	char *q, *p = expression+1; // skip leading ':'
	listItem *vector[ 2 ] = { NULL, NULL };
	for ( q=p; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
		addItem( &vector[ 0 ], q );
	if ( q[1]=='<' ) {
		for ( q++; *q++!='>'; q=p_prune(PRUNE_LEVEL,q) )
			addItem( &vector[ 1 ], q ); }
	else {	freeListItem( &vector[ 0 ] );
		return 0; }

	InstantiateData *data = traverse_data->user_data;
	CNDB *db = data->db;
	listItem *sub[ 2 ];
	CNInstance *e;
	while (( p=popListItem( &vector[0] ) )&&( q=popListItem(&vector[1]) )) {
		ifn_instantiate_traversal( p, 0 ) {
			cleanup( traverse_data, NULL );
			continue; }
		sub[ 0 ] = data->sub[ 0 ];
		data->sub[ 0 ] = NULL;
		assign_v2one( sub, q, traverse_data, NULL ); }
	freeListItem( &vector[ 0 ] );
	freeListItem( &vector[ 1 ] );
	return 1; }

static inline void
assign_v2one( listItem **sub, char *p, BMTraverseData *traverse_data, char *expression ) {
	InstantiateData *data = traverse_data->user_data;
	CNDB *db = data->db;
	if ( !strncmp( p, "~.", 2 ) )
		BM_UNASSIGN( sub, db )
	else {
		ifn_instantiate_traversal( p, 0 ) {
			cleanup( traverse_data, expression );
			freeListItem( &sub[ 0 ] ); }
		else {
			sub[ 1 ] = data->sub[ 0 ];
			data->sub[ 0 ] = NULL;
			BM_ASSIGN( sub, db ); } } }

static inline void
assign_one2v( listItem **sub, char *p, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
	CNDB *db = data->db;
	for ( listItem *i=sub[ 0 ]; (i) && *p++!='>'; i=i->next ) {
		if ( !strncmp( p, "~.", 2 ) ) {
			db_unassign( i->ptr, db );
			p+=2; continue; }
		ifn_instantiate_traversal( p, 0 )
			cleanup( traverse_data, NULL );
		else {
			db_assign( i->ptr, data->sub[ 0 ]->ptr, db );
			freeListItem( &data->sub[ 0 ] ); } }
	freeListItem( &sub[ 0 ] ); }

//---------------------------------------------------------------------------
//	assign_new
//---------------------------------------------------------------------------
static void inform_UBE( Registry *, char *, CNArena *, BMTraverseData * );
static void inform_carry( Registry *, char *, CNCell *, BMTraverseData * );

static void
assign_new( listItem **list, char *p, CNStory *story, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	Registry *buffer;
	Pair *entry;
	p += 2; // skip '!!'
	//-----------------------------------------------------------
	//	UBE assignment
	//-----------------------------------------------------------
	if ( !*p ) { // do :_: !!
		CNArena *arena = story->arena;
		CNInstance *ube, *x;
		while (( x=popListItem( list ))) {
			ube = bm_arena_register( arena, NULL, db );
			bm_register_ube( ctx, ube );
			db_assign( x, ube, db ); }
		return; }
	else if ( *p=='|' ) {
		CNArena *arena = story->arena;
		buffer = newRegistry( IndexedByAddress );
		registryRegister( ctx, ";", buffer );
		CNInstance *ube, *x = (list) ? popListItem(list) : NULL;
		do {	ube = bm_arena_register( arena, NULL, db );
			registryRegister( buffer, x, ube );
			} while (( list )&&( x=popListItem(list) ));
		p++; // skip '|'
		inform_UBE( buffer, p, arena, traverse_data ); }
	//-----------------------------------------------------------
	//	carry assignment
	//-----------------------------------------------------------
	else if (( entry=registryLookup( story->narratives, p ) )) {
		buffer = newRegistry( IndexedByAddress );
		registryRegister( ctx, ";", buffer );
		CNCell *this = BMContextCell( ctx );
		CNInstance *proxy, *x = (list) ? popListItem(list) : NULL;
		do {	CNCell *child = newCell( entry, NULL );
			addItem( BMCellCarry(this), child );
			proxy = new_proxy( this, child, db );
			registryRegister( buffer, x, proxy );
			} while (( list )&&( x=popListItem(list) ));
		p = p_prune( PRUNE_IDENTIFIER, p );
		if ( !strncmp( p, "()", 2 ) ) p+=2;
		inform_carry( buffer, p, this, traverse_data ); }
	else {
		fprintf( stderr, ">>>>> B%%: Error: class not found in expression\n" );
		errout((list)?InstantiateClassNotFoundv:InstantiateClassNotFound, p );
		if (( list )) freeListItem( list );
		return; }
	registryDeregister( ctx, ":" );
	registryDeregister( ctx, ";" );
	freeRegistry( buffer, NULL ); }

static void
inform_UBE( Registry *buffer, char *p, CNArena *arena, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	char *start_p = p;
	listItem *pipe_mark = newItem( NULL );
	bm_push_mark( ctx, "|", pipe_mark );	
	Pair *current = registryRegister( ctx, ":", NULL );
	// inform each UBE's connections
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next, p=start_p ) {
		Pair *entry = i->ptr;
		current->value = entry;
		pipe_mark->ptr = entry->value;
		ifn_instantiate_traversal( p, 0 )
			cleanup( traverse_data, NULL );
		else freeListItem( &data->sub[ 0 ] ); }
	// assign or deregister UBE depending on connected
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *x = entry->name;
		CNInstance *ube = entry->value;
		if ( !x && !CONNECTED(ube) ) {
			errout( InstantiateUBENotConnected, start_p );
			bm_arena_deregister( arena, ube, db ); }
		else {
			if (( x )) db_assign( x, ube, db );
			bm_register_ube( ctx, ube ); } }
	bm_pop_mark( ctx, "|" );
	freeItem( pipe_mark ); }

static void
inform_carry( Registry *buffer, char *p, CNCell *this, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	char *start_p = p;
	Pair *current = registryRegister( ctx, ":", NULL );
	for ( listItem *i=buffer->entries; i!=NULL; i=i->next, p=start_p ) {
		Pair *entry = i->ptr;
		current->value = entry;
		CNInstance *x = entry->name;
		CNInstance *proxy = entry->value;
		CNCell *child = DBProxyThat( proxy );
		data->carry = BMCellContext( child );
		if ( *p=='(' ) {
			p++;
			do {	ifn_instantiate_traversal( p, CARRY )
					cleanup( traverse_data, NULL );
				else freeListItem( &data->sub[ 0 ] );
			} while ( *p++!=')' ); }
		if ( *p=='~' ) {
			if (( x )) db_unassign( x, db );
			db_deprecate( proxy, db ); }
		else {
			if (( x )) db_assign( x, proxy, db );
			bm_cell_bond( this, child, proxy ); } } }

//---------------------------------------------------------------------------
//	assign_string
//---------------------------------------------------------------------------
static void
assign_string( listItem **list, char *s, CNStory *story, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
	CNDB *db = data->db;
	CNInstance *e = bm_arena_register( story->arena, s, db );
	bm_register_string( data->ctx, e );
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

	BMTraverseData traverse_data;
	traverse_data.user_data = &data;
	traverse_data.stack = &data.stack.flags;

	if ( !instantiate_input( arg, input, &traverse_data ) ) return;
	errout( InstantiateInput, arg, ((input)?input:"EOF") );
	cleanup( &traverse_data, NULL ); }

static int
instantiate_input( char *arg, char *input, BMTraverseData *traverse_data ) {
	InstantiateData *data = traverse_data->user_data;
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

