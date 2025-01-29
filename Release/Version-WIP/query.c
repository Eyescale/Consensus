#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_pivot.h"
#include "locate_mark.h"
#include "eenov.h"
#include "errout.h"

// #define DEBUG

#include "query.h"
#include "query_private.h"
#include "query_traversal.h"

//===========================================================================
//	bm_query
//===========================================================================
static XPTraverseCB verify_candidate;

static int pivot_query( int, char *, BMQueryData *, XPTraverseCB *, void * );
static CNInstance * query_assignment( int, char *, BMQueryData * );
static inline CNInstance * dotnext( CNDB *, listItem ** );

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
	if ( !pivot_query( privy, expression, &data, verify_candidate, NULL ) ) {
		CNInstance *e;
		listItem *s = NULL;
		switch ( type ) {
		case BM_CONDITION:
			for ( e=DBFirst(db,&s); e!=NULL; e=DBNext(db,e,&s) )
				if ( verify_candidate( e, expression, &data )==BMQ_DONE ) {
					freeListItem( &s );
					return e; }
			e = BMContextSelf( ctx );
			if ( !db_private( 0, e, db ) ) {
				listItem *i = newItem( e );
				for ( addItem(&s,i); e!=NULL; e=dotnext(db,&s) )
					if ( verify_candidate( e, expression, &data )==BMQ_DONE ) {
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
verify_candidate( CNInstance *e, char *expression, BMQueryData *data ) {
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
#define LMID(mark_p) ( (mark_p)[0]=='?' ? (mark_p)[1]==':' ? 1 : 3 : 0 )
static CNInstance * xp_traverse( char *, BMQueryData *, XPTraverseCB *, void * );

static int
pivot_query( int privy, char *expression, BMQueryData *data, XPTraverseCB *cb, void *user_data )
/*
	bm_locate_pivot will return with list:[ list_p, mark_p ] informed as the
	first element of data->exponent, when expression is in either form:

		_%(list,...)_				lm	list_expr
		 ^      ^---------------- mark_p	0	   2|3
		  ----------------------- list_p
	or
		_%(list,?:...)_
		 ^      ^---------------- mark_p	1	   4|5
		  ----------------------- list_p
	or
		_%((?,...):list)
		 ^  ^-------------------- mark_p	3	   6|7
		  ----------------------- list_p

	Where
		. lm is used in xp_traverse() when list is used as pivot
		. list_expr is used in xp_verify() when candidate is verified

	ie. two different usages, which may not even refer to the same list
	at the same time in the overall expression
*/ {
	data->instance = NULL;
	char *p = bm_locate_pivot( expression, &data->exponent );
	if ( !p ) return 0;
	else if ( !strncmp( p, "%(", 2 ) ) {
		// %(list,?:...) or %(list,...) or %((?,...):list)
		Pair *list = popListItem( &data->exponent );
		p += ( LMID((char *) list->value )==3 ? 10 : 2 );
		listItem *xpn = NULL;
		p = bm_locate_pivot( p, &xpn );
		if ( p && strncmp(p,"%(",2) )
			data->list = newPair( list, xpn );
		else {
			freePair( list );
			freeListItem( &xpn );
			freeListItem( &data->exponent );
			return 0; } }

	CNInstance *e = bm_lookup( privy, p, data->ctx, data->db );
	if (( e )) {
		data->pivot = newPair( p, e );
		if ( !xp_traverse( expression, data, cb, user_data ) )
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
xp_traverse( char *expression, BMQueryData *data, XPTraverseCB *verify_CB, void *user_data )
/*
	Traverses data->pivot's exponent [ resp., if data->list is set,
		xpn.as_sub[0]^n.sub[1].exponent (n>=1), etc. ]
		invoking verify_CB on every match
	returns current match on the callback's BMQ_DONE, and NULL otherwise
	Assumptions
	. when set, we have data->list:[ list:[ list_p, mark_p ], xpn ]
	  and expression as described in above pivot_query()
	. user_data only specified for query_assignment, in which case
	  we can overwrite data->user_data with the passed user_data
	. pivot->value is not deprecated - and neither are its subs
*/ {
	CNDB *db = data->db;
	int privy = data->privy;
	Pair *pivot = data->pivot;
	char *p = pivot->name;

	// %| no longer authorized in query expressions
	int pv = (( *p=='%' )&&( p[1]=='@' || !is_separator(p[1]) ));

	listItem *i, *j, *pvi;
	CNInstance *e, *f;
	if ( pv ) {
		i = pivot->value;
		e = i->ptr;
		pvi = NULL; }
	else {
		e = pivot->value;
		i = newItem( e );
		pvi = i; }
	listItem *exponent = data->exponent;
	if (( user_data ))
		data->user_data = user_data;
#ifdef DEBUG
db_outputf( db, stderr, "XP_TRAVERSE bgn: privy=%d, pivot=%_, exponent=%^\n", privy, e, exponent );
#endif
	CNInstance *success = NULL;
	listItem *trail = NULL;
	if ( !data->list ) {
		listItem *stack = NULL;
		for ( ; ; ) {
PUSH_stack:		PUSH( stack, exponent, POP_stack )
			if (( addIfNotThere( &trail, e ) ) // ward off doublons
			    && ( verify_CB( e, expression, data )==BMQ_DONE )) {
				success = e; break; }
POP_stack:		POP( stack, exponent, PUSH_stack )
			break; }
		POP_ALL( stack ) }
	else {
		Pair *list = data->list->name;
		int lm = LMID((char *) list->value );
		listItem *xpn = data->list->value;
		struct {
			listItem *exp, *xpn, *list;
			} stack = { NULL, NULL, NULL };
		for ( ; ; ) {
PUSH_xpn:		PUSH( stack.xpn, xpn, POP_xpn )
PUSH_list:		LUSH( stack.list, lm, POP_list )
			if ( lm!=2 ) { // ward off doublons Here
				if ( !addIfNotThere( &trail, e ) )
					goto POP_list;
				if ( lm==1 ) e = e->sub[ 1 ]; }
PUSH_exp:		PUSH( stack.exp, exponent, POP_exp )
			if (( lm!=2 || ( addIfNotThere( &trail, e ) )) &&
			    ( verify_CB( e, expression, data )==BMQ_DONE )) {
				success = e; break; }
POP_exp:		POP( stack.exp, exponent, PUSH_exp )
POP_list:		LOP( stack.list, lm, PUSH_list )
			if ( !stack.xpn ) break;
			POP_XPi( stack.xpn, xpn );
POP_xpn:		POP( stack.xpn, xpn, PUSH_xpn )
			break; }
		POP_ALL( stack.exp )
		freeListItem( &stack.list );
		POP_ALL( stack.xpn ) }
#ifdef DEBUG
	db_outputf( db, stderr, "XP_TRAVERSE end: returning %_\n", success );
#endif
	freeListItem( &trail );
	if (( pvi )) freeItem( pvi );
	return success; }

//---------------------------------------------------------------------------
//	xp_verify	- also invoked by eeno_query.c: bm_eeno_scan()
//---------------------------------------------------------------------------
static inline void op_set( BMVerifyOp, BMQueryData *, CNInstance *, char *, int );
static inline CNInstance * x_sub( CNInstance *, listItem *, listItem * );
static inline char * prune( char *, int );
typedef struct {
	listItem *flags;
	listItem *list_expr;
	listItem *list_i;
	listItem *mark_exp;
	listItem *sub;
	listItem *as_sub;
	listItem *i;
	listItem *p;
	} XPVerifyStack;
static inline listItem *
	pop_as_sub( XPVerifyStack *, listItem *, listItem ** );

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
	char *p = expression, *start_p;
	if ( !strncmp( p, "?:", 2 ) )
		p += 2;
#ifdef DEBUG
db_outputf( db, stderr, "xp_verify: %$ / candidate=%_ ........{\n", p, x );
#endif
	XPVerifyStack stack;
	memset( &stack, 0, sizeof(stack) );

	BMVerifyOp op = BM_INIT;
	listItem *exponent = NULL;
	listItem *list_i = NULL;
	listItem *mark_exp = NULL;
	listItem *i = newItem( x ), *j;
	int list_expr = 0;
	int flags = FIRST;
	int success = 0;

	BMTraverseData traversal;
	traversal.user_data = data;
	traversal.stack = &data->stack.flags;
	traversal.done = 0;
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
			else { // Assumption: star case only
				CNInstance *y = CNSUB( x, 0 );
				if ( y && cnStarMatch( y->sub[0] ) ) {
					x = CNSUB( x, exp.value&1 );
					if (( x )) {
						addItem( &stack.as_sub, i );
						addItem( &stack.as_sub, exponent );
						exponent = exponent->next;
						i = newItem( x );
						continue; } } } }

		if (( x )) {
			if ( op==BM_BGN ) {
				if ( *p++=='*' ) {
					if (!( x=assignment_fetch( x, data ) )) {
						success = 0;
						goto POST_OP; } }
				else switch ( list_expr ) {
					case 2: case 3: // start past opening '%(' of %(list,...)
						p++; break;
					case 6: case 7: // start past opening '%((?,...):' of %((?,...):list)
						p+=9; break; } }
			op_set( op, data, x, p, success );
			//----------------------------------------------------------

				p = query_traversal( p, &traversal, flags );

			//----------------------------------------------------------
			if (( data->mark_exp )) { // including new data->list_expr
				flags = traversal.flags;
				listItem *base = data->base;
				addItem( &data->stack.base, base );
				addItem( &data->stack.scope, data->stack.flags );
				if (( x=x_sub( x, data->stack.exponent, base ) )) {
					DBG_OP_SUB_EXPR( p )
					// backup context - aka. push stack
					add_item( &stack.flags, flags );
					add_item( &stack.list_expr, list_expr );
					addItem( &stack.mark_exp, mark_exp );
					addItem( &stack.sub, stack.as_sub );
					addItem( &stack.i, i );
					addItem( &stack.p, p );
					// setup new sub context
					f_clr( NEGATED )
					list_expr = data->list_expr;
					// cases %(list,...) or %((?,...):list)
					if ( list_expr==2 || list_expr==6 )
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
					freeListItem( &data->mark_exp );
					// move on past sub-expression
					if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
					p = prune( p, success );
					exponent = NULL;
					op = BM_END;
					continue; } }
			else {
				DBG_OP_RETURN( p, data->success )
				success = data->success; } }
		else success = 0;
POST_OP:
		if (( mark_exp ) || list_expr ) {
			switch ( list_expr ) {
			case 2: case 3:	// finish past closing ',...)' of %(list,...)
				p += 5; break;
			case 6: case 7: // finish past closing ')' of %((?,...):list)
				p++; break; }
			if ( success ) {
				// restore context & move on past sub-expression
				LFLUSH( list_expr, i, list_i, stack.list_i );
				do POP_STACK( start_p, i, mark_exp, list_expr, flags )
				while is_DOUBLE_STARRED( start_p, expression, data );
				// move on - p is already informed
				if is_f( NEGATED ) { success = 0; f_clr( NEGATED ) }
				exponent = NULL;
				op = BM_END; }
			else {
				if ( list_expr ) {
					if_LDIG( list_expr, x, i, list_i, stack.list_i ) {
						i = j;
						p = stack.p->ptr;
						op = BM_BGN;
						continue; } }
				for ( ; ; ) {
					if (( i->next )) {
						i = i->next;
						if ( !db_private( privy, i->ptr, db ) ) {
							p = stack.p->ptr;
							op = BM_BGN;
							break; } }
					else if (( stack.as_sub )) {
						exponent = popListItem( &stack.as_sub );
						if (!( cast_i(exponent->ptr) & SUB )) freeItem( i );
						i = popListItem( &stack.as_sub ); }
					else {
						// restore context & move on past sub-expression
						POP_STACK( start_p, i, mark_exp, list_expr, flags )
						// move on - p is already informed unless x==NULL
						if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
						if ( x==NULL ) { p = prune( start_p, success ); }
						exponent = NULL;
						op = BM_END;
						break; } } } }
		else break; }
#ifdef DEBUG
	if ((data->stack.flags) || (data->stack.exponent)) {
		errout( QueryExponentMemoryLeak );
		exit( -1 ); }
	freeListItem( &data->stack.flags );
	freeListItem( &data->stack.exponent );
	if ((data->stack.scope) || (data->stack.base)) {
		errout( QueryScopeMemoryLeak );
		exit( -1 ); }
	fprintf( stderr, "xp_verify:........} success=%d\n", success );
#endif
	freeItem( i );
	return success; }

static inline void
op_set( BMVerifyOp op, BMQueryData *data, CNInstance *x, char *p, int success )
/*
	Note: we cannot use exponent to track scope, as no exponent is
	pushed in case of "single" expressions - e.g. %(%?:(.,?))
*/ {
	DBG_OP_SET( data->db, x, p, success );
	switch ( op ) {
	case BM_BGN:
	case BM_INIT:
		data->base = data->stack.exponent;
		data->OOS = data->stack.flags;
		break;
	default:
		OP_END( data ) }
	data->op = op;
	data->instance = x;
	data->list_expr = 0;
	data->mark_exp = NULL;
	data->success = success; }

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

static inline char *
prune( char *p, int success ) {
	p = p_prune(( success ? PRUNE_FILTER : PRUNE_LEVEL ), p+1 );
	return ( p[0]==':' ? p[1]=='|' ? p+2 : p+1 : p ); }

//===========================================================================
//	query_traversal
//===========================================================================
static int match( char *, BMQueryData * );
static char *tag( char *, BMQueryData * );

BMTraverseCBSwitch( query_traversal )
case_( tag_CB )
	if ( !data->success )
		_prune( BM_PRUNE_LEVEL, p+1 )
	else if ( p[2]=='.' ) // special case: |^.
		*(int *)data->user_data = (p[3]=='~');
	else if (( p=tag(p,data) ))
		_continue( p )
	_break
case_( filter_CB )
	if ( data->op==BM_BGN && data->stack.flags==data->OOS )
		_return( 1 )
	else if ( !data->success )
		_prune( BM_PRUNE_LEVEL, p+1 )
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
	if ( p[1]=='^' )
		switch_over( match_CB, p, p )
	else {
		xpn_add( &data->mark_exp, SUB, 1 );
		_return( 1 ) }
	_break
case_( sub_expression_CB )
	char *mark = bm_locate_mark( p+1, &data->mark_exp );
	if (( data->mark_exp )) {
		if ( !strncmp( mark, "...", 3 ) )
			data->list_expr = 2;
		else if ( !strncmp( mark+1, ":...", 4 ) )
			data->list_expr = 4;
		else if ( !strncmp( mark+1, ",...", 4 ) )
			data->list_expr = 6;
		_return( 1 ) }
	_break
case_( dot_expression_CB )
	listItem **stack = &data->stack.exponent;
	xpn_add( stack, AS_SUB, 0 );
	switch ( match( p, data ) ) {
	case 0: if is_f( NEGATED ) {
			data->success=1; popListItem( stack );
			_prune( BM_PRUNE_FILTER, p+1 ) }
		// no break
	case -1:
		data->success=0; popListItem( stack );
		_prune( BM_PRUNE_LEVEL, p+1 )
	default: xpn_set( *stack, AS_SUB, 1 ); }
	_break
case_( open_CB )
	if is_f_next( ASSIGN ) {
		listItem **stack = &data->stack.exponent;
		xpn_add( stack, AS_SUB, 0 );
		switch ( match( p, data ) ) {
		case 0: if is_f( NEGATED ) {
				data->success=1; popListItem( stack );
			 	_prune( BM_PRUNE_FILTER, p ) }
			// no break
		case -1:
			data->success=0; popListItem( stack );
			_prune( BM_PRUNE_LEVEL, p )
		default: xpn_set( *stack, AS_SUB, 1 ); } }
	else if is_f_next( COUPLE ) {
		listItem **stack = &data->stack.exponent;
		xpn_add( stack, AS_SUB, 0 ); }
	_break
case_( comma_CB )
	if ( data->stack.flags==data->OOS ) {
		_return( 1 ) }
	if ( !data->success ) {
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
	if is_f_next( NEGATED ) data->success = !data->success;
	if ( data->op==BM_END && (data->stack.scope) && data->stack.flags->next==data->OOS )
		traversal->done = 1; // after popping
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
	if ( !x ) return NULL;
	else {
		p += 2;
		Pair *entry = BMTag( data->ctx, p );
		char *q = p_prune( PRUNE_IDENTIFIER, p );
		if (( entry )) {
			listItem **tag = (listItem **) &entry->value;
			switch ( *q ) {
			case '~':
				removeIfThere( tag, x );
				q++; break;
			default:
				addIfNotThere( tag, x ); } }
		else switch ( *q ) {
			case '~':
				q++; break;
			default:
				bm_tag( data->ctx, p, newItem(x) ); }
		return q; } }


//---------------------------------------------------------------------------
//	query_assignment
//---------------------------------------------------------------------------
static XPTraverseCB verify_unassigned, verify_variable, verify_value;

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
verify_unassigned( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( !xp_verify( e, expression, data ) )
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
verify_value( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( !xp_verify( e, expression, data ) )
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
verify_variable( CNInstance *e, char *expression, BMQueryData *data )
{
	if ( !xp_verify( e, expression, data ) )
		return BMQ_CONTINUE;
	char *variable = data->user_data;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			// Assumption: cannot be manifested AND private
			if ( f->sub[0]!=star || !db_manifested(g,db) )
				continue;
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BMQ_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			if ( f->sub[0]!=star || db_private(0,g,db) )
				continue;
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BMQ_DONE; } } }
	return BMQ_CONTINUE; }

//===========================================================================
//	bm_query_assignee
//===========================================================================
static XPTraverseCB verify_assignee;

CNInstance *
bm_query_assignee( int type, char *expression, BMContext *ctx )
/*
	Assumption: expression not "~."
*/ {
	if ( !type ) {
		// bm_case() usage
		CNInstance *e = BMVal( ctx, "*^^" );
		if ( !e ) return NULL;
		BMQueryData data;
		memset( &data, 0, sizeof(data) );
		data.ctx = ctx;
		data.db = BMContextDB( ctx );
		return xp_verify( e, expression, &data ) ? e : NULL; }
	else {
		// bm_switch() usage
		CNDB *db = BMContextDB( ctx );
		BMQueryData data;
		memset( &data, 0, sizeof(data) );
		data.ctx = ctx;
		data.db = db;
		CNInstance *star = db_lookup( 0, "*", db );
		if ( !star ) return NULL;
		data.star = star;
		data.type = type;
		expression++; // skip leading ':'
		if ( !pivot_query( 0, expression, &data, verify_assignee, NULL ) ) {
			switch ( type ) {
			case BM_CONDITION: ;
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr, *f;
					if ( db_private(0,e,db) ) continue;
					if ( xp_verify( e->sub[1], expression, &data ) ) {
						// return ((*,.),.) or (*,.)
						return (( f=assignment( e, db ) )) ? f : e; } }
				return NULL;
			default: ;
				listItem *s = NULL;
				CNInstance *fallback = NULL;
				for ( CNInstance *e=DBLog(1,0,db,&s); e!=NULL; e=DBLog(0,0,db,&s) ) {
					CNInstance *f = CNSUB( e, 0 );
					if ( f==star ) fallback = e;
					if ( !f || f->sub[0]!=star ) continue;
					if ( xp_verify( f->sub[ 1 ], expression, &data ) ) {
						freeListItem( &s );
						return e; } } // return ((*,.),.)
				return fallback; } } // return (*,.)
		return data.instance; } }

static BMQTake
verify_assignee( CNInstance *e, char *expression, BMQueryData *data )
{
	if ( !xp_verify( e, expression, data ) )
		return BMQ_CONTINUE;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if (( g ) && db_manifested(g,db) ) {
				data->instance = g; // return ((*,e),.)
				return BMQ_DONE; }
			else if ( !g && db_manifested(f,db) ) {
				data->instance = f; // return (*,e)
				return BMQ_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr, *g; // f:(.,e)
			if ( f->sub[0]==star ) {
				// return ((*,.),.) or (*,.)
				data->instance = (( g=assignment(f,db) )) ? g: f;
				return BMQ_DONE; } } }
	return BMQ_CONTINUE; }

