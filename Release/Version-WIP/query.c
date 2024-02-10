#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_pivot.h"
#include "locate_mark.h"
#include "query.h"
#include "eenov.h"

// #define DEBUG

static inline CNInstance * x_sub( CNInstance *, listItem *, listItem * );

//===========================================================================
//	query_traversal
//===========================================================================
#include "query_traversal.h"
#include "query_private.h"

static int match( char *, BMQueryData * );
static char *tag( char *, BMQueryData * );

BMTraverseCBSwitch( query_traversal )
case_( filter_CB )
	if ( data->op==BM_BGN && data->stack.flags==data->OOS )
		_return( 1 )
	else if ( !data->success )
		_prune( BM_PRUNE_LEVEL, p+1 )
	else if ( *p=='|' ) {
		if ( p[2]=='.' ) // special case: |^.
			*(int *)data->user_data = (p[3]=='~');
		else {
			p = tag( p, data );
			if (( p )) _continue( p ) } }
	_break
case_( match_CB )
	switch ( match( p, data ) ) {
	case -1: data->success = 0; break;
	case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
	case  1: data->success = is_f( NEGATED ) ? 0 : 1; break; }
	_break
case_( dot_identifier_CB )
	xpn_add( &data->stack.exponent, AS_SUB, 0 );
	switch ( match( p, data ) ) {
	case -1: data->success = 0; break;
	case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
	case  1: if ( p[1]=='?' ) { // special case: .? (cannot be negated)
			data->success = 1;
			break; }
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		switch ( match( p+1, data ) ) {
		case -1: data->success = 0; break;
		case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
		default: data->success = is_f( NEGATED ) ? 0 : 1; break; } }
	popListItem( &data->stack.exponent );
	_break
case_( dereference_CB )
	xpn_add( &data->mark_exp, SUB, 1 );
	_return( 1 )
case_( sub_expression_CB )
	char *mark = bm_locate_mark( p+1, &data->mark_exp );
	if (( data->mark_exp )) {
		if ( !strncmp( mark, "...", 3 ) )
			data->list_exp = 2;
		else if ( !strncmp( mark+1, ":...", 4 ) )
			data->list_exp = 4;
		else if ( !strncmp( mark+1, ",...", 4 ) )
			data->list_exp = 6;
		_return( 1 ) }
	_break
case_( dot_expression_CB )
	listItem **stack = &data->stack.exponent;
	xpn_add( stack, AS_SUB, 0 );
	switch ( match( p, data ) ) {
	case 0: if is_f( NEGATED ) {
		data->success=1; popListItem( stack );
		_prune( BM_PRUNE_FILTER, p+1 ) }
	case -1: // no break
		data->success=0; popListItem( stack );
		_prune( BM_PRUNE_LEVEL, p+1 )
	default: xpn_set( *stack, AS_SUB, 1 ); }
	_break
case_( open_CB )
	if ( f_next & ASSIGN ) {
		listItem **stack = &data->stack.exponent;
		xpn_add( stack, AS_SUB, 0 );
		switch ( match( p, data ) ) {
		case 0: if is_f( NEGATED ) {
			data->success=1; popListItem( stack );
			_prune( BM_PRUNE_FILTER, p ) }
		case -1: // no break
			data->success=0; popListItem( stack );
			_prune( BM_PRUNE_LEVEL, p )
		default: xpn_set( *stack, AS_SUB, 1 ); } }
	else if ( f_next & COUPLE )
		xpn_add( &data->stack.exponent, AS_SUB, 0 );
	_break
case_( decouple_CB )
	if ( data->stack.flags==data->OOS ) {
		_return( 1 ) }
	else if ( !data->success ) {
		_prune( BM_PRUNE_LEVEL, p+1 ) }
	else {
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		_break }
case_( close_CB )
	if ( data->stack.flags==data->OOS )
		_return( 1 )
	if is_f( COUPLE ) popListItem( &data->stack.exponent );
	else if is_f( ASSIGN ) popListItem( &data->stack.exponent );
	if is_f( DOT ) popListItem( &data->stack.exponent );
	if ( f_next & NEGATED ) data->success = !data->success;
	if ( data->op==BM_END && (data->stack.scope) && data->stack.flags->next==data->OOS )
		traverse_data->done = 1; // after popping
	_break
case_( wildcard_CB )
	if is_f( NEGATED ) data->success = 0;
	else if ( !uneq( data->stack.exponent, 1 ) ) {
		data->success = 1; } // wildcard is any or as_sub[1]
	else switch ( match( NULL, data ) ) {
		case -1: // no break
		case  0: data->success = 0; break;
		case  1: data->success = 1; break; }
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	match, tag
//---------------------------------------------------------------------------
static int
match( char *p, BMQueryData *data )
/*
	tests x.sub[kn]...sub[k0] where exponent=as_sub[k0]...as_sub[kn]
	is either NULL or the exponent of p as expression term
*/ {
	CNInstance *x = x_sub( data->instance, data->stack.exponent, data->base );
	if ( !x ) return -1;
	if ( !p ) return 1; // wildcard
	if (( data->pivot ) && p==data->pivot->name ) {
		if ( *p=='%' && ( p[1]=='@' || !is_separator(p[1]) ) )
			return !!lookupIfThere( data->pivot->value, x );
		else	return ( x==data->pivot->value ); }
	return bm_match( data->ctx, data->db, p, x, data->db ); }

static char *
tag( char *p, BMQueryData *data ) {
	CNInstance *x = x_sub( data->instance, data->stack.exponent, data->base );
	return (( x )? bm_tag( data->ctx, p+2, x ) : NULL ); }

static inline CNInstance *
x_sub( CNInstance *x, listItem *exponent, listItem *base ) {
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=base; i=i->next )
		addItem( &xpn, i->ptr );
	while (( x ) && (xpn)) {
		int exp = pop_item( &xpn );
		x = CNSUB( x, exp&1 ); }
	if ( !x ) freeListItem( &xpn );
	return x; }

//===========================================================================
//	bm_query
//===========================================================================
typedef BMQTake XPTraverseCB( CNInstance *, char *, BMQueryData * );

static int pivot_query( int, char *, BMQueryData *, XPTraverseCB *, void * );
static CNInstance * query_assignment( int, char *, BMQueryData * );
static inline CNInstance * dotnext( CNDB *, listItem ** );
static XPTraverseCB bm_verify;

CNInstance *
bm_query( int type, char *expression, BMContext *ctx,
	  BMQueryCB user_CB, void *user_data )
/*
	find the first term other than '.' or '?' in expression which is not
	negated. if found then test each entity in term.exponent against
	expression. otherwise test every entity in CNDB against expression.
	if no user callback is provided, returns first entity that passes.
	otherwise, invokes callback for each entity that passes, and returns
	first entity for which user callback returns BMQ_DONE.
*/ {
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY: %s\n", expression );
#endif
	CNDB *db = BMContextDB( ctx );
	BMQueryData data;
	memset( &data, 0, sizeof(BMQueryData));
	data.type = type;
	data.ctx = ctx;
	data.db = db;
	data.user_CB = user_CB;
	data.user_data = user_data;
	if ( *expression==':' )
		return query_assignment( type, expression, &data );

	int privy = data.privy = ( type==BM_RELEASED ? 1 : 0 );
	if ( !pivot_query( privy, expression, &data, bm_verify, NULL ) ) {
		CNInstance *e;
		listItem *s = NULL;
		switch ( type ) {
		case BM_CONDITION:
			for ( e=DBFirst(db,&s); e!=NULL; e=DBNext(db,e,&s) )
				if ( bm_verify( e, expression, &data )==BMQ_DONE ) {
					freeListItem( &s );
					return e; }
			e = BMContextSelf( ctx );
			if ( !db_private( 0, e, db ) ) {
				listItem *i = newItem( e );
				for ( addItem(&s,i); e!=NULL; e=dotnext(db,&s) )
					if ( bm_verify( e, expression, &data )==BMQ_DONE ) {
						freeListItem( &s );
						break; }
				freeItem( i );
				return e; }
			return NULL;
		default:
			for ( e=DBLog(1,privy,db,&s); e!=NULL; e=DBLog(0,privy,db,&s) )
				if ( xp_verify( e, expression, &data ) ) {
					freeListItem( &s );
					return e; }
			return NULL; } }
	return data.instance; }

static BMQTake
bm_verify( CNInstance *e, char *expression, BMQueryData *data ) {
	CNDB *db;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		if ( db_manifested(e,data->db) && xp_verify(e,expression,data) ) {
			data->instance = e;
			return BMQ_DONE; }
		break;
	case BM_RELEASED:
		if ( db_deprecated(e,data->db) && xp_verify(e,expression,data) ) {
			data->instance = e;
			return BMQ_DONE; }
		break;
	default:
		if ( xp_verify(e,expression,data) && ( !data->user_CB ||
		     data->user_CB( e, data->ctx, data->user_data )==BMQ_DONE ) ) {
			data->instance = e;
			return BMQ_DONE; } }
	return BMQ_CONTINUE; }

static inline CNInstance *
dotnext( CNDB *db, listItem **stack ) {
	if ( !*stack ) return NULL;
	listItem *i = (*stack)->ptr;
	CNInstance *e = i->ptr;
	for ( i=e->as_sub[ 0 ]; i!=NULL; i=i->next ) {
		e = i->ptr;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e; } }
	while (( i = popListItem( stack ) ))
		if (( i->next )) {
			i = i->next;
			e = i->ptr;
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e; } }
	return NULL; }

//---------------------------------------------------------------------------
//	pivot_query
//---------------------------------------------------------------------------
static CNInstance * xp_traverse( char *, BMQueryData *, XPTraverseCB *, void * );

static int
pivot_query( int privy, char *expression, BMQueryData *data, XPTraverseCB *CB, void *user_data ) {
	data->instance = NULL;
	char *p = bm_locate_pivot( expression, &data->exponent );
	if ( !p ) return 0;
	else if ( !strncmp( p, "%(", 2 ) ) {
		// %(list,?:...) or %(list,...) or %((?,...):list)
		Pair *list = popListItem( &data->exponent );
		// list_p = list->name;
		// mark_p = list->value;
		listItem *xpn = NULL;
		p += strncmp(p+2,"(?",2) ? 2 : 10;
		p = bm_locate_pivot( p, &xpn );
		if ( p && strncmp(p,"%(",2) )
			data->list = newPair( list, xpn );
		else {
			freePair( list );
			freeListItem( &xpn );
			freeListItem( &data->exponent );
			return 0; } }

	CNInstance *e = bm_lookup( data->ctx, p, data->db, privy );
	if (( e )) {
		data->pivot = newPair( p, e );
		if ( !xp_traverse( expression, data, CB, user_data ) )
			data->instance = NULL;
		freePair( data->pivot );
		data->pivot = NULL; }

	data->privy = privy;
	freeListItem( &data->exponent );
	if (( data->list )) {
		freePair( data->list->name );
		freeListItem((listItem **) &data->list->value );
		freePair( data->list );
		data->list = NULL; }

	return 1; }

//---------------------------------------------------------------------------
//	xp_traverse
//---------------------------------------------------------------------------
static CNInstance *
xp_traverse( char *expression, BMQueryData *data, XPTraverseCB *traverse_CB, void *user_data )
/*
	Traverses data->pivot's exponent [ resp., if data->list is set,
		xpn.as_sub[0]^n.sub[1].exponent (n>=1), etc. ]
		invoking traverse_CB on every match
	returns current match on the callback's BMQ_DONE, and NULL otherwise
	Assumptions
	. when set, we have data->list:[ list:[ list_p, mark_p ], xpn ] where
	  expression is in either form
			_%(list,?:...)_
			 ^      ^---------------- mark_p	lm==1
			  ----------------------- list_p
		or
			_%(list,...)_
			 ^      ^---------------- mark_p	lm==0
			  ----------------------- list_p
		or
			_%((?,...):list)
			 ^  ^-------------------- mark_p	lm==3
			  ----------------------- list_p
	. user_data only specified for query_assignment, in which case
	  we can overwrite data->user_data with the passed user_data
	. pivot->value is not deprecated - and neither are its subs
*/ {
	CNDB *db = data->db;
	int privy = data->privy;
	Pair *pivot = data->pivot;
	char *p = pivot->name;

	// %| no longer authorized in query expressions
	int pv = ( *p=='%' ?( p[1]=='@' || !is_separator(p[1]) ): 0 );
	
	listItem *i, *j;
	CNInstance *e, *f;
	if ( pv ) {
		i = pivot->value;
		e = i->ptr; }
	else {
		e = pivot->value;
		i = newItem( e ); }
	listItem *exponent = data->exponent;
	if (( user_data ))
		data->user_data = user_data;
#ifdef DEBUG
fprintf( stderr, "XP_TRAVERSE: privy=%d, pivot=", privy );
db_outputf( stderr, db, "%_, exponent=", e );
xpn_out( stderr, exponent );
fprintf( stderr, "\n" );
#endif
	CNInstance *success = NULL;
	listItem *trail = NULL;
	if ( !data->list ) {
		listItem *stack = NULL;
		for ( ; ; ) {
PUSH_stack:		PUSH( stack, exponent, POP_stack )
			if ( !lookupIfThere( trail, e ) ) { // ward off doublons
				addIfNotThere( &trail, e );
				if ( traverse_CB( e, expression, data )==BMQ_DONE ) {
					success = e;
					break; } }
POP_stack:		POP( stack, exponent, PUSH_stack )
			break; }
		POP_ALL( stack ) }
	else {
		Pair *list = data->list->name;
		listItem *xpn = data->list->value;
		char *mark_p = list->value;
		int lm = !strncmp(mark_p+1,",...",4) ? 3 : ( *mark_p=='?' );
		listItem *stack[ 3 ] = { NULL, NULL, NULL };
		enum { XPN_id, LIST_id, EXP_id };
		for ( ; ; ) {
PUSH_xpn:		PUSH( stack[ XPN_id ], xpn, POP_xpn )
PUSH_list:		LUSH( stack[ LIST_id ], lm, POP_list )
			if ( lm==1 ) { // ward off doublons Here
				if ( !lookupIfThere( trail, e ) ) {
					addIfNotThere( &trail, e );
					e = e->sub[ 1 ]; }
				else goto POP_list; }
PUSH_exp:		PUSH( stack[ EXP_id ], exponent, POP_exp )
			if ( lm==1 ) { // Here we do NOT ward off doublons
				if ( traverse_CB( e, expression, data )==BMQ_DONE ) {
					success = e;
					break; } }
			else if ( !lookupIfThere( trail, e ) ) { // ward off doublons
				addIfNotThere( &trail, e );
				if ( traverse_CB( e, expression, data )==BMQ_DONE ) {
					success = e;
					break; } }
POP_exp:		POP( stack[ EXP_id ], exponent, PUSH_exp )
POP_list:		LOP( stack[ LIST_id ], lm, PUSH_list, stack[ XPN_id ] )
			POP_XPi( stack[ XPN_id ], xpn );
POP_xpn:		POP( stack[ XPN_id ], xpn, PUSH_xpn )
			break; }
		POP_ALL( stack[ XPN_id ] )
		POP_ALL( stack[ EXP_id ] )
		freeListItem( &stack[ LIST_id ] ); }
#ifdef DEBUG
	fprintf( stderr, "XP_TRAVERSE: end, success=%d\n", !!success );
#endif
	freeListItem( &trail );
	if ( !pv )
		freeItem( i );
	return success; }

//---------------------------------------------------------------------------
//	xp_verify	- also invoked by eeno_query.c: bm_eeno_scan()
//---------------------------------------------------------------------------
static CNInstance * op_set( BMVerifyOp, BMQueryData *, CNInstance *, char **, int );
typedef struct {
	listItem *flags;
	listItem *list_exp;
	listItem *list_i;
	listItem *mark_exp;
	listItem *sub;
	listItem *as_sub;
	listItem *i;
	listItem *p;
	} XPVerifyStack;

static inline listItem * pop_as_sub( XPVerifyStack *, listItem *, listItem ** );
static inline char * prune( char *, int );
int
xp_verify( CNInstance *x, char *expression, BMQueryData *data )
/*
	Assumption: expression is not ternarized
	invokes query_traversal on each [sub-]expression, ie. on each
	expression term starting with '*' or '%' - note that *var is same
	as %((*,var),?)
*/ {
	CNDB *db = data->db;
	int privy = data->privy;
	char *p = expression;
	if ( !strncmp( p, "?:", 2 ) )
		p += 2;
#ifdef DEBUG
fprintf( stderr, "xp_verify: %s / ", p );
db_outputf( stderr, db, "candidate=%_ ........{\n", x );
#endif
	XPVerifyStack stack;
	memset( &stack, 0, sizeof(stack) );
	listItem *exponent = NULL;
	listItem *list_i = NULL;
	listItem *mark_exp = NULL;
	listItem *i = newItem( x ), *j;
	int list_exp = 0;
	int success = 0;
	int flags = FIRST;
	BMVerifyOp op = BM_INIT;

	BMTraverseData traverse_data;
	traverse_data.user_data = data;
	traverse_data.stack = &data->stack.flags;
	traverse_data.done = 0;
	for ( ; ; ) {
		x = i->ptr;
		if (( exponent )) {
			union { void *ptr; int value; } exp;
			exp.ptr = exponent->ptr;
			if ( exp.value & SUB ) {
				for ( j=x->as_sub[ exp.value&1 ]; j!=NULL; j=j->next )
					if ( !db_private( privy, j->ptr, db ) )
						break;
				if (( j )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = j; continue; }
				else x = NULL; }
			else {
				x = CNSUB( x, exp.value&1 );
				if (( x )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = newItem( x );
					continue; } } }

		if ((x) && ( x=op_set( op, data, x, &p, success ) )) {
			if ( op==BM_BGN )
				switch ( list_exp ) {
				case 2: case 3: // start past opening '%(' of %(list,...)
					p++; break;
				case 6: case 7: // start past opening '%((?,...):' of %((?,...):list)
					p+=9; break; }
			//----------------------------------------------------------

				p = query_traversal( p, &traverse_data, flags );

			//----------------------------------------------------------
			if (( data->mark_exp )) {
				flags = traverse_data.flags;
				listItem *base = data->base;
				addItem( &data->stack.base, base );
				addItem( &data->stack.scope, data->stack.flags );
				if (( x=x_sub( x, data->stack.exponent, base ) )) {
#ifdef DEBUG
					fprintf( stderr, "xp_verify: starting SUB, at '%s'\n", p );
#endif
					// backup context
					add_item( &stack.flags, flags );
					add_item( &stack.list_exp, list_exp );
					addItem( &stack.mark_exp, mark_exp );
					addItem( &stack.sub, stack.as_sub );
					addItem( &stack.i, i );
					addItem( &stack.p, p );
					// setup new sub context
					f_clr( NEGATED )
					list_exp = data->list_exp;
					// cases %(list,...) or %((?,...):list)
					if ( list_exp==2 || list_exp==6 )
						freeListItem( &data->mark_exp );
					mark_exp = data->mark_exp;
					stack.as_sub = NULL;
					i = newItem( x );
					exponent = mark_exp;
					// traverse sub-expression
					op = BM_BGN;
					success = 0;
					continue; }
				else {
					// move on past sub-expression
					if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
					p = prune( p, success );
					exponent = NULL;
					op = BM_END;
					continue; } }
			else {
#ifdef DEBUG
				fprintf( stderr, "xp_verify: returning %d, at '%s'\n", data->success, p );
#endif
				success = data->success; } }
		else success = 0;

		if (( mark_exp ) || list_exp ) {
			switch ( list_exp ) {
			case 2: case 3:	// finish past closing ',...)' of %(list,...)
				p += 5; break;
			case 6: case 7: // finish past closing ')' of %((?,...):list)
				p++; break; }
			if ( success ) {
				// restore context & move on past sub-expression
				LFLUSH( list_exp, i, list_i, stack.list_i )
				// pop stack
				popListItem( &stack.p );
				i = pop_as_sub( &stack, i, &mark_exp );
				mark_exp = popListItem( &stack.mark_exp );
				list_exp = pop_item( &stack.list_exp );
				flags = pop_item( &stack.flags );
				// move on - p is already informed
				if is_f( NEGATED ) { success = 0; f_clr( NEGATED ) }
				exponent = NULL;
				op = BM_END; }
			else {
				if ( list_exp ) {
					CND_LXj( list_exp, x, i, list_i, stack.list_i ) {
						i = j;
						p = stack.p->ptr;
						op = BM_BGN;
						success = 0;
						continue; } }
				for ( ; ; ) {
					if (( i->next )) {
						i = i->next;
						if ( !db_private( privy, i->ptr, db ) ) {
							p = stack.p->ptr;
							op = BM_BGN;
							success = 0;
							break; } }
					else if (( stack.as_sub )) {
						exponent = popListItem( &stack.as_sub );
						if (!( cast_i(exponent->ptr) & SUB )) freeItem( i );
						i = popListItem( &stack.as_sub ); }
					else {
						// restore context & move on past sub-expression
						char *start_p = popListItem( &stack.p );
						i = pop_as_sub( &stack, i, &mark_exp );
						mark_exp = popListItem( &stack.mark_exp );
						list_exp = pop_item( &stack.list_exp );
						flags = pop_item( &stack.flags );
						// move on - p is already informed unless x==NULL
						if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
						if ( x==NULL ) { p = prune( start_p, success ); }
						exponent = NULL;
						op = BM_END;
						break; } } } }
		else break; }
	freeItem( i );
#ifdef DEBUG
	if ((data->stack.flags) || (data->stack.exponent)) {
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on exponent\n" );
		exit( -1 ); }
	freeListItem( &data->stack.flags );
	freeListItem( &data->stack.exponent );
	if ((data->stack.scope) || (data->stack.base)) {
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on scope\n" );
		exit( -1 ); }
#endif
#ifdef DEBUG
	fprintf( stderr, "xp_verify:........} success=%d\n", success );
#endif
	return success; }

static inline listItem *
pop_as_sub( XPVerifyStack *stack, listItem *i, listItem **mark_exp )
/*
	free as_sub exponent stack and associated items
	(if these were reallocated) starting from i
*/ {
	listItem **as_sub = &stack->as_sub;
	while (( *as_sub )) {
		listItem *exponent = popListItem( as_sub );
		if (!( cast_i(exponent->ptr) & SUB )) freeItem( i );
		i = popListItem( as_sub ); }
	freeItem( i );
	stack->as_sub = popListItem( &stack->sub );
	// free mark_exp exponent stack
	freeListItem( mark_exp );
	// return previously stacked i
	return popListItem( &stack->i ); }

static inline char *
prune( char *p, int success ) {
	p = p_prune(( success ? PRUNE_FILTER : PRUNE_LEVEL ), p+1 );
	if ( *p==':' ) p++;
	return p; }

//---------------------------------------------------------------------------
//	op_set
//---------------------------------------------------------------------------
static CNInstance *
op_set( BMVerifyOp op, BMQueryData *data, CNInstance *x, char **q, int success ) {
	char *p = *q;
	switch ( op ) {
	case BM_BGN:
		if ( *p++=='*' ) {
			// take x.sub[0].sub[1] if x.sub[0].sub[0]==star
			CNInstance *y = CNSUB( x, 0 );
			if ( y && DBStarMatch( y->sub[0] ) )
				x = y->sub[ 1 ];
			else {
				data->success = 0;
				x = NULL; break; } }
		// no break
	default:
#ifdef DEBUG
		fprintf( stderr, "xp_verify: " );
		switch ( op ) {
		case BM_INIT: fprintf( stderr, "BM_INIT success=%d ", success ); break;
		case BM_BGN: fprintf( stderr, "BM_BGN success=%d ", success ); break;
		case BM_END: fprintf( stderr, "BM_END success=%d ", success ); break; }
		fprintf( stderr, "'%s'\n", p );
#endif
		/* we cannot use exponent to track scope, as no exponent is
		   pushed in case of "single" expressions - e.g. %(%?:(.,?))
		*/
		if ( op==BM_END ) {
			data->base = popListItem( &data->stack.base );
			popListItem( &data->stack.scope ); // done with that one
			data->OOS = ((data->stack.scope) ? data->stack.scope->ptr : NULL ); }
		else {
			data->base = data->stack.exponent;
			data->OOS = data->stack.flags; }
		data->op = op;
		data->instance = x;
		data->list_exp = 0;
		data->mark_exp = NULL;
		data->success = success; }
	*q = p;
	return x; }

//===========================================================================
//	query_assignment
//===========================================================================
static XPTraverseCB
	verify_unassigned, verify_variable, verify_value;

static CNInstance *
query_assignment( int type, char *expression, BMQueryData *data ) {
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	CNInstance *star = db_lookup( 0, "*", db );
	if ( !star ) return NULL;
	data->star = star;

	expression++; // skip leading ':'
	if ( !strcmp(expression,"~.") ) {
		CNInstance *e = cn_instance( star, BMContextSelf(ctx), 0 );
		return (( e ) && !db_private(0,e,db) && !assignment(e,db)) ?
			type==BM_CONDITION || db_manifested(e,db) ? e : NULL :
			NULL; }

	char *value = p_prune( PRUNE_FILTER, expression );
	if ( *value++=='\0' ) { // moving past ',' aka. ':' if there is
		// Here we have :expression, which is equivalent to :%%:expression
		CNInstance *e = cn_instance( star, BMContextSelf(ctx), 0 ), *f;
		return (( e ) && (f=assignment(e,db)) && xp_verify(f->sub[1],expression,data)) ?
			type==BM_CONDITION || db_manifested(f,db) ? f : NULL :
			NULL; }

	if ( !strncmp( value, "~.", 2 ) ) {
		if ( !pivot_query( 0, expression, data, verify_unassigned, NULL ) ) {
			switch ( type ) {
			case BM_CONDITION: ;
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr;
					if (( assignment(e,db) )) continue;
					if ( xp_verify( e->sub[1], expression, data ) )
						return e; } // return (*,.)
				return NULL;
			default: ;
				listItem *s = NULL;
				for ( CNInstance *e=DBLog(1,0,db,&s); e!=NULL; e=DBLog(0,0,db,&s) ) {
					if ( e->sub[0]!=star || (assignment(e,db)) )
						continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						freeListItem( &s );
						return e; } } // return (*,.)
				return NULL; } } }
	else {
		if ( !pivot_query( 0, expression, data, verify_value, value ) &&
		     !pivot_query( 0, value, data, verify_variable, expression ) ) {
			switch ( type ) {
			case BM_CONDITION: ;
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr, *f;
					if ( db_private(0,e,db) || !(f=assignment(e,db)) )
						continue;
					if ( xp_verify( e->sub[1], expression, data ) &&
					     xp_verify( f->sub[1], value, data ) )
						return f; } // return ((*,.),.)
				return NULL;
			default: ;
				listItem *s = NULL;
				for ( CNInstance *e=DBLog(1,0,db,&s); e!=NULL; e=DBLog(0,0,db,&s) ) {
					CNInstance *f = CNSUB( e, 0 );
					if ( !f || f->sub[0]!=star ) continue;
					if ( xp_verify( f->sub[1], expression, data ) &&
					     xp_verify( e->sub[1], value, data ) ) {
						freeListItem( &s );
						return e; } } // return ((*,.),.)
				return NULL; } } }
	return data->instance; }

static BMQTake
verify_unassigned( CNInstance *e, char *variable, BMQueryData *data ) {
	if ( !xp_verify( e, variable, data ) )
		return BMQ_CONTINUE;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			if ( !db_manifested(f,db) ) continue;
			if ( assignment(f,db) ) continue;
			data->instance = f; // return (*,e)
			return BMQ_DONE; }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			if ( assignment(f,db) ) continue;
			data->instance = f; // return (*,e)
			return BMQ_DONE; } }

	return BMQ_CONTINUE; }

static BMQTake
verify_value( CNInstance *e, char *variable, BMQueryData *data ) {
	if ( !xp_verify( e, variable, data ) )
		return BMQ_CONTINUE;
	char *value = data->user_data;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if ( !g || !db_manifested(g,db) ) continue;
			if ( xp_verify( g->sub[1], value, data ) ) {
				data->instance = g; // return ((*,e),.)
				return BMQ_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if ( !g ) continue;
			if ( xp_verify( g->sub[1], value, data ) ) {
				data->instance = g; // return ((*,e),.)
				return BMQ_DONE; } } }
	return BMQ_CONTINUE; }

static BMQTake
verify_variable( CNInstance *e, char *value, BMQueryData *data )
{
	if ( !xp_verify( e, value, data ) )
		return BMQ_CONTINUE;
	char *variable = data->user_data;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			if ( g->sub[0]->sub[0]!=star || !db_manifested(g,db) )
				continue;
			// Assumption: cannot be manifested AND private
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BMQ_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			if ( g->sub[0]->sub[0]!=star || db_private(0,g,db) )
				continue;
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BMQ_DONE; } } }
	return BMQ_CONTINUE; }

