#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_pivot.h"
#include "locate_mark.h"
#include "locate_emark.h"
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
static CNInstance * query_base( CNInstance *, CNDB *, char *, void * );
static CNInstance * query_assignment( int, char *, BMQueryData * );

CNInstance *
bm_query( int type, char *expression, BMContext *ctx, BMQueryCB user_CB, void *user_data )
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
	if ( !strncmp( expression, "?:", 2 ) )
		expression += 2;
	int privy = type==BM_RELEASED;
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
			return e=query_base( e, db, expression, &data ) ? e :
				db_arena_query( db, expression, query_base, &data );
		default:
			for ( e=DBLog(1,privy,db,&s); e!=NULL; e=DBLog(0,privy,db,&s) )
				if ( xp_verify( e, expression, &data ) ) {
					freeListItem( &s );
					return e; }
			return NULL; } }
	return data.instance; }

static BMQTake
verify_candidate( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( data->type==BM_INSTANTIATED ?
		db_manifested(e,data->db) && xp_verify(e,expression,data) :
		data->type==BM_RELEASED ?
			db_deprecated(e,data->db) && xp_verify(e,expression,data) :
			xp_verify( e, expression, data ) && ( !data->user_CB ||
				data->user_CB( e, data->ctx, data->user_data )==BMQ_DONE ))
		return ( data->instance=e, BMQ_DONE );
	else return BMQ_CONTINUE; }

//---------------------------------------------------------------------------
//	query_base
//---------------------------------------------------------------------------
static inline CNInstance * query_next( CNDB *, listItem ** );

static CNInstance *
query_base( CNInstance *e, CNDB *db, char *expression, void *data ) {
	listItem *s = NULL;
	listItem *i = newItem( e );
	for ( addItem(&s,i); e!=NULL; e=query_next(db,&s) )
		if ( verify_candidate( e, expression, data )==BMQ_DONE ) {
			freeListItem( &s );
			break; }
	freeItem( i );
	return e; }

static inline CNInstance *
query_next( CNDB *db, listItem **stack ) {
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
static inline int cleanup( listItem *, listItem *, Pair *, int  );
static inline int is_multi( char *pv );

static int
pivot_query( int privy, char *expression, BMQueryData *data, XPTraverseCB *cb, void *user_data )
/*
	Traverses bm_locate_pivot's exponent [ resp., if data->list is set,
		xpn.as_sub[0]^n.sub[1].exponent (n>=1), etc. ]
		invoking verify_CB on every match
	returns current match on the callback's BMQ_DONE, and NULL otherwise

	Assumptions
	. when set, we have data->list:[ list:[ list_p, mark_p ], xpn ]
	  and expression as described in above pivot_query()
	. user_data only specified for query_assignment, in which case
	  we can overwrite data->user_data with the passed user_data
	. pivot->value is not deprecated - and neither are its subs

	bm_locate_pivot returns list:[ list_p, mark_p ] as first exponent
	element when expression is in either form:

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
	listItem *exponent = NULL;
	char *p = bm_locate_pivot( expression, &exponent );
	if ( !p ) return 0;
	Pair *list = NULL;
	listItem *xpn;
	int lm;
	if ( !strncmp( p, "%(", 2 ) ) {
		list = popListItem( &exponent );
		lm = LMID((char *) list->value );
		// %(list,?:...) or %(list,...) or %((?,...):list)
		p += ( lm==3 ? 10 : 2 );
		p = ( xpn=NULL, bm_locate_pivot( p, &xpn ) );
		if ( !p || !strncmp(p,"%(",2) )
			return cleanup( exponent, xpn, list, 0 ); }
	CNDB *db = data->db;
	void *pve = bm_lookup( privy, p, data->ctx, db );
	if ( !pve ) return cleanup( exponent, xpn, list, 1 );
#ifdef DEBUG
db_outputf( db, stderr, "PIVOT_QUERY bgn: privy=%d, pivot=%_, exponent=%^\n", privy, pve, exponent );
#endif
	if (( user_data )) data->user_data = user_data;
	Pair *pivot = data->pivot = newPair( p, pve );
	data->instance = NULL;
	listItem *i, *j, *pvi;
	CNInstance *e, *f;
	if ( is_multi(p) ) { i=pve; e=i->ptr; pvi=NULL; }
	else { e=pve; i=newItem(e); pvi=i; }
	CNInstance *success = NULL;
	listItem *trail = NULL;
	if ( !list ) {
		listItem *stack = NULL;
		for ( ; ; ) {
PUSH_stack:		PUSH( stack, exponent, POP_stack )
			if (( addIfNotThere( &trail, e ) ) // ward off doublons
			    && ( cb( e, expression, data )==BMQ_DONE )) {
				success = e; break; }
POP_stack:		POP( stack, exponent, PUSH_stack )
			break; }
		POP_ALL( stack ) }
	else {
		struct { listItem *exp, *xpn, *list; } stack = { NULL, NULL, NULL };
		listItem *xp = exponent;
		for ( ; ; ) {
PUSH_xpn:		PUSH( stack.xpn, xpn, POP_xpn )
PUSH_list:		LUSH( stack.list, lm, POP_list )
			if ( lm&1 ) { // ward off doublons Here
				if ( !addIfNotThere( &trail, e ) )
					goto POP_list;
				if ( lm==1 ) e = e->sub[ 1 ]; }
PUSH_exp:		PUSH( stack.exp, xp, POP_exp )
			if (( lm&1 || ( addIfNotThere( &trail, e ) )) &&
			    ( cb( e, expression, data )==BMQ_DONE )) {
				success = e; break; }
POP_exp:		POP( stack.exp, xp, PUSH_exp )
POP_list:		LOP( stack.list, lm, PUSH_list )
			if ( !stack.xpn ) break;
			POP_XPi( stack.xpn, xpn );
POP_xpn:		POP( stack.xpn, xpn, PUSH_xpn )
			break; }
		POP_ALL( stack.exp )
		freeListItem( &stack.list );
		POP_ALL( stack.xpn ) }
#ifdef DEBUG
	db_outputf( db, stderr, "PIVOT_QUERY end: returning '%_'\n", success );
#endif
	freeListItem( &trail );
	if (( pvi )) freeItem( pvi );
	if ( !success ) data->instance = NULL;
	if ( *p=='/' ) // regex results
		freeListItem((listItem **) &pivot->value );
	freePair( pivot );
	data->pivot = NULL;
	return cleanup( exponent, xpn, list, 1 ); }

static inline int
cleanup( listItem *exponent, listItem *xpn, Pair *list, int done ) {
	freeListItem( &exponent );
	if (( list )) {
		freeListItem( &xpn );
		freePair( list ); }
	return done; }

static inline int is_multi( char *pv ) {
	switch ( *pv ) {
	case '/': return 1; // regex
	case '%': return ( pv[1]=='@' || !is_separator(pv[1]) );
	default: return 0; } }

//---------------------------------------------------------------------------
//	xp_verify	- also invoked by eeno_query.c: bm_eeno_scan()
//---------------------------------------------------------------------------
static inline void op_set( int, BMQueryData *, CNInstance *, char *, int );
static inline void push_mark_sel( char *, listItem **, listItem ** );
static inline void pop_mark_sel( listItem **, listItem ** );
static inline CNInstance * star_sub( CNInstance *, CNDB *, int exp );
static inline CNInstance * x_sub( CNInstance *, listItem *, listItem * );
static inline char * prune( char *, int );
typedef struct {
	listItem *as_sub;
	listItem *list_i;
	listItem *heap;
	} XPVerifyStack;
static inline void
	pop_as_sub( XPVerifyStack *, listItem *, listItem ** );

int
xp_verify( CNInstance *x, char *expression, BMQueryData *data )
/*
	Assumption: expression is not ternarized
	invokes query_traversal on each [sub-]expression, ie. on each
	expression term starting with '*' or '%' - note that *var is same
	as %((*,var),?)
*/ {
	int privy = data->type==BM_RELEASED;
	CNDB *db = data->db;
	char *p = expression, *start_p;
#ifdef DEBUG
db_outputf( db, stderr, "xp_verify: %$ / candidate=%_ ........{\n", p, x );
#endif
	XPVerifyStack stack;
	memset( &stack, 0, sizeof(stack) );

	int op = INIT_OP;
	listItem *exponent = NULL;
	listItem *list_i = NULL;
	listItem *mark_exp = NULL;
	listItem *mark_sel = NULL;
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
			else if (( x=star_sub( x, db, exp.value ) )) {
				addItem( &stack.as_sub, i );
				addItem( &stack.as_sub, exponent );
				exponent = exponent->next;
				i = newItem( x );
				continue; } }
		if (( x )) {
			if ( op==BGN_OP ) {
				if ( *p++=='*' ) {
					if (!( x=assignment_fetch( x ) )) {
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
				push_mark_sel( p, &data->mark_sel, &mark_sel );
				flags = traversal.flags;
				listItem *base = data->base;
				addItem( &data->stack.base, base );
				addItem( &data->stack.scope, data->stack.flags );
				if (( x=x_sub( x, data->stack.exponent, base ) )) {
					DBG_OP_SUB_EXPR( p )
					// backup context - aka. push stack
					PUSH_STACK( p, i, mark_exp, list_expr, flags )
					// setup new sub context
					f_clr( NEGATED )
					list_expr = BM_LIST;
					// cases %(list,...) or %((?,...):list)
					if ( list_expr==2 || list_expr==6 )
						freeListItem( &data->mark_exp );
					mark_exp = data->mark_exp;
					stack.as_sub = NULL;
					i = newItem( x );
					exponent = mark_exp;
					// traverse sub-expression
					op = BGN_OP;
					success = 0;
					continue; }
				else {
					freeListItem( &data->mark_exp );
					pop_mark_sel( &data->mark_sel, &mark_sel );
					// move on past sub-expression
					if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
					p = prune( p, success );
					exponent = NULL;
					op = END_OP;
					continue; } }
			else {
				success = BM_SUCCESS;
				DBG_OP_RETURN( p, success ) } }
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
				op = END_OP; }
			else {
				if ( list_expr ) {
					if_LDIG( list_expr, x, i, list_i, stack.list_i ) {
						i = j;
						p = stack.heap->ptr;
						op = BGN_OP;
						continue; } }
				for ( ; ; ) {
					if (( i->next )) {
						i = i->next;
						if ( !db_private( privy, i->ptr, db ) ) {
							p = stack.heap->ptr;
							op = BGN_OP;
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
						if ( x==NULL ) p = prune( start_p, success );
						exponent = NULL;
						op = END_OP;
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
	fprintf( stderr, "xp_verify:........} success=%d\n", !!success );
#endif
	freeItem( i );
	return success; }

static inline void
op_set( int op, BMQueryData *data, CNInstance *x, char *p, int success )
/*
	Note: we cannot use exponent to track scope, as no exponent is
	pushed in case of "single" expressions - e.g. %(%?:(.,?))
*/ {
	DBG_OP_SET( data->db, x, p, success );
	switch ( op ) {
	case BGN_OP:
	case INIT_OP:
		data->base = data->stack.exponent;
		data->OOS = data->stack.flags;
		break;
	default:
		OP_END( data ) }
	data->op = success ? (op|SUCCESS_OP) : op;
	data->instance = x;
	data->mark_exp = NULL; }

static inline void
push_mark_sel( char *p, listItem **mark_sel, listItem **backup ) {
	if (( *mark_sel ) && strncmp(p,"%!",2)) {
		addItem( backup, *mark_sel );
		*mark_sel = NULL; }
	else if (( *backup )) addItem( backup, NULL ); }

static inline void
pop_mark_sel( listItem **mark_sel, listItem **backup ) {
	if ( *backup ) *mark_sel = popListItem( backup );
	else freeListItem( mark_sel ); }

static inline void
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
	freeListItem( mark_exp ); }

static inline CNInstance *
star_sub( CNInstance *x, CNDB *db, int exp ) {
#ifdef DEBUG
	if (exp &STAR_SUB) db_outputf( db, stderr, "xp_verify: *** star_sub: %_\n", x );
#endif
	return ( exp & STAR_SUB ) ?
		!CNSUB(x,0) || !cnStarMatch( CNSUB(x->sub[0],0) ) ?
		NULL : CNSUB(x,1) : CNSUB(x,exp); }

static inline CNInstance *
x_sub( CNInstance *x, listItem *exponent, listItem *base ) {
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=base; i=i->next )
		addItem( &xpn, i->ptr );
	while (( x ) && (xpn)) {
		int exp = pop_item(&xpn) & 1;
		x = CNSUB( x, exp ); }
	if ( !x ) freeListItem( &xpn );
	return x; }

static inline char *
prune( char *p, int success ) {
	p = p_prune(( success ? PRUNE_FILTER : PRUNE_LEVEL ), p+1 );
	return ( p[0]==':' ? p+1 : p ); }

//===========================================================================
//	query_traversal
//===========================================================================
static int match( char *, BMQueryData * );
static char *tag( char *, BMQueryData * );

BMTraverseCBSwitch( query_traversal )
case_( tag_CB )
	if ( BM_BGN && BM_OOS )
		_return( 1 )
	else if ( !BM_SUCCESS )
		_prune( BMCB_LEVEL, p+1 )
	else if ( p[2]=='.' ) // special case: |^.
		*(int *)data->user_data = (p[3]=='~');
	else if (( p=tag(p,data) ))
		_continue( p )
	_break
case_( bgn_selection_CB )
	bm_locate_emark( p+3, &data->mark_exp );
	bm_locate_mark( SUB_EXPR, p+3, &data->mark_sel );
	for ( listItem *i=data->mark_sel; i!=NULL; i=i->next )
		i->ptr = cast_ptr( cast_i(i->ptr) & 1 );
	_return( 1 )
case_( end_selection_CB )
	if ( BM_OOS ) _return( 1 )
	if ( BM_END && (data->stack.scope) && data->stack.flags->next==data->OOS )
		traversal->done = 1; // after popping
	_break
case_( filter_CB )
	if ( BM_BGN && BM_OOS )
		_return( 1 )
	else if ( !BM_SUCCESS )
		_prune( BMCB_LEVEL, p+1 )
	_break
case_( match_CB )
	switch ( match( p, data ) ) {
	case -1: clr_SUCCESS; break;
	case  0: if is_f( NEGATED ) set_SUCCESS else clr_SUCCESS; break;
	case  1: if is_f( NEGATED ) clr_SUCCESS else set_SUCCESS; break; }
	_break
case_( dot_identifier_CB )
	xpn_add( &data->stack.exponent, AS_SUB, 0 );
	switch ( match( p, data ) ) {
	case -1: clr_SUCCESS; break;
	case  0: if is_f( NEGATED ) set_SUCCESS else clr_SUCCESS; break;
	case  1: if ( p[1]=='?' ) { // special case: .? (cannot be negated)
			set_SUCCESS;
			break; }
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		switch ( match( p+1, data ) ) {
		case -1: clr_SUCCESS; break;
		case  0: if is_f( NEGATED ) set_SUCCESS else clr_SUCCESS; break;
		default: if is_f( NEGATED ) clr_SUCCESS else set_SUCCESS; break; } }
	popListItem( &data->stack.exponent );
	_break
case_( dereference_CB )
	xpn_add( &data->mark_exp, SUB, 1 );
	_return( 1 )
case_( sub_expression_CB )
	listItem **mark_exp = &data->mark_exp;
	char *mark = bm_locate_mark( SUB_EXPR|STARRED, p+1, mark_exp );
	if ( is_f(SEL_EXPR) && !is_f(LEVEL) ) {
		for ( listItem *i=data->mark_sel; i!=NULL; i=i->next )
			addItem( mark_exp, i->ptr ); }
	if (( mark )) {
		if ( !strncmp( mark, "...", 3 ) ) // %(list,...)
			data->op |= 2;
		else if ( !strncmp( mark+1, ":...", 4 ) ) // %(list,?:...)
			data->op |= 4;
		else if ( !strncmp( mark+1, ",...", 4 ) ) // %((?,...):list)
			data->op |= 6;
		_return( 1 ) }
	else if (( *mark_exp ))
		_return( 1 )
	_break
case_( dot_expression_CB )
	listItem **stack = &data->stack.exponent;
	xpn_add( stack, AS_SUB, 0 );
	switch ( match( p, data ) ) {
	case 0: if is_f( NEGATED ) {
			set_SUCCESS; popListItem( stack );
			_prune( BMCB_FILTER, p+1 ) }
		// no break
	case -1:
		clr_SUCCESS; popListItem( stack );
		_prune( BMCB_LEVEL, p+1 )
	default: xpn_set( *stack, AS_SUB, 1 ); }
	_break
case_( open_CB )
	if is_f_next( ASSIGN ) {
		listItem **stack = &data->stack.exponent;
		xpn_add( stack, AS_SUB, 0 );
		switch ( match( p, data ) ) {
		case 0: if is_f( NEGATED ) {
				set_SUCCESS; popListItem( stack );
			 	_prune( BMCB_FILTER, p ) }
			// no break
		case -1:
			clr_SUCCESS; popListItem( stack );
			_prune( BMCB_LEVEL, p )
		default: xpn_set( *stack, AS_SUB, 1 ); } }
	else if is_f_next( COUPLE ) {
		listItem **stack = &data->stack.exponent;
		xpn_add( stack, AS_SUB, 0 ); }
	_break
case_( comma_CB )
	if ( BM_OOS ) _return( 1 )
	if ( !BM_SUCCESS ) _prune( BMCB_LEVEL, p+1 )
	xpn_set( data->stack.exponent, AS_SUB, 1 );
	_break
case_( close_CB )
	if ( BM_OOS ) _return( 1 )
	if is_f( COUPLE ) popListItem( &data->stack.exponent );
	else if is_f( ASSIGN ) popListItem( &data->stack.exponent );
	if is_f( DOT ) popListItem( &data->stack.exponent );
	if is_f_next( NEGATED ) { if BM_SUCCESS clr_SUCCESS else set_SUCCESS }
	if ( BM_END && (data->stack.scope) && data->stack.flags->next==data->OOS )
		traversal->done = 1; // after popping
	_break
case_( wildcard_CB )
	if is_f( NEGATED ) clr_SUCCESS
	else if ( !uneq( data->stack.exponent, 1 ) ) {
		set_SUCCESS } // wildcard is any or as_sub[1]
	else switch ( match( NULL, data ) ) {
		case -1: // no break
		case  0: clr_SUCCESS; break;
		case  1: set_SUCCESS; break; }
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
		return is_multi( p ) ?
			!!lookupIfThere( data->pivot->value, x ) :
			x==data->pivot->value; }
	return bm_match( data->ctx, data->db, p, x, data->db ); }

static char *
tag( char *p, BMQueryData *data ) {
	CNInstance *x = x_sub( data->instance, data->stack.exponent, data->base );
	if ( !x ) return NULL;
	p += 2;
	Pair *entry = BMTag( data->ctx, p );
	char *q = p_prune( PRUNE_IDENTIFIER, p );
	switch ( *q ) {
	case '~':
		if (( entry )) {
			listItem **tag = (listItem **) &entry->value;
			removeIfThere( tag, x ); }
		q++; break;
	default:
		if ( !entry ) {
			bm_tag( data->ctx, p, newItem(x) ); }
		else {
			listItem **tag = (listItem **) &entry->value;
			addIfNotThere( tag, x ); } }
	return q; }

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
		if ( pivot_query( 0, expression, data, verify_unassigned, star ) )
			return data->instance;
		else if ( type==BM_CONDITION ) {
			for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
				CNInstance *e = i->ptr;
				if (( assignment(e,db) )) continue;
				if ( xp_verify( e->sub[1], expression, data ) )
					return e; } } // return (*,.)
		else {
			listItem *s = NULL;
			for ( CNInstance *e=DBLog(1,0,db,&s); e!=NULL; e=DBLog(0,0,db,&s) ) {
				if ( e->sub[0]!=star || (assignment(e,db)) )
					continue;
				if ( xp_verify( e->sub[1], expression, data ) ) {
					freeListItem( &s );
					return e; } } } } // return (*,.)
	else {
		void *cbd[ 2 ] = { star, NULL };
		if (( cbd[1]=value, pivot_query( 0, expression, data, verify_value, &cbd )) ||
		    ( cbd[1]=expression, pivot_query( 0, value, data, verify_variable, &cbd )))
			return data->instance;
		else if ( type==BM_CONDITION ) {
			for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
				CNInstance *e = i->ptr, *f;
				if ( db_private(0,e,db) || !(f=assignment(e,db)) )
					continue;
				if ( xp_verify( e->sub[1], expression, data ) &&
				     xp_verify( f->sub[1], value, data ) )
					return f; } } // return ((*,.),.)
		else {
			listItem *s = NULL;
			for ( CNInstance *e=DBLog(1,0,db,&s); e!=NULL; e=DBLog(0,0,db,&s) ) {
				CNInstance *f = CNSUB( e, 0 );
				if ( !f || f->sub[0]!=star ) continue;
				if ( xp_verify( f->sub[1], expression, data ) &&
				     xp_verify( e->sub[1], value, data ) ) {
					freeListItem( &s );
					return e; } } } } // return ((*,.),.)
	return NULL; }

static BMQTake
verify_unassigned( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( !xp_verify( e, expression, data ) )
		return BMQ_CONTINUE;
	CNDB *db = data->db;
	CNInstance *star = data->user_data;
	if ( data->type==BM_INSTANTIATED ) {
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			if ( !db_manifested(f,db) ) continue;
			if ( assignment(f,db) ) continue;
			data->instance = f; // return (*,e)
			return BMQ_DONE; } }
	else {
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
	CNDB *db = data->db;
	void **cbd = data->user_data;
	CNInstance *star = cbd[ 0 ];
	char *value = cbd[ 1 ];
	if ( data->type==BM_INSTANTIATED ) {
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if ( !g || !db_manifested(g,db) ) continue;
			if ( xp_verify( g->sub[1], value, data ) ) {
				data->instance = g; // return ((*,e),.)
				return BMQ_DONE; } } }
	else {
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
verify_variable( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( !xp_verify( e, expression, data ) )
		return BMQ_CONTINUE;
	CNDB *db = data->db;
	void **cbd = data->user_data;
	CNInstance *star = cbd[ 0 ];
	char *variable = cbd[ 1 ];
	if ( data->type==BM_INSTANTIATED ) {
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			// Assumption: cannot be manifested AND private
			if ( f->sub[0]!=star || !db_manifested(g,db) )
				continue;
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BMQ_DONE; } } }
	else {
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
//	bm_query_assignee / bm_query_assigner
//===========================================================================
static XPTraverseCB verify_assignee;

CNInstance *
bm_query_assignee( int type, char *expression, BMContext *ctx )
/*
	Assumption: expression not "~."
*/ {
	// bm_switch() usage
	CNDB *db = BMContextDB( ctx );
	BMQueryData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = db;
	CNInstance *star = db_lookup( 0, "*", db );
	if ( !star ) return NULL;
	data.type = type;
	expression++; // skip leading ':'
	if ( pivot_query( 0, expression, &data, verify_assignee, star ) )
		return data.instance;
	else if ( type==BM_CONDITION ) {
		for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
			CNInstance *e = i->ptr, *f;
			if ( db_private(0,e,db) ) continue;
			if ( xp_verify( e->sub[1], expression, &data ) ) {
				// return ((*,.),.) or (*,.)
				return (( f=assignment( e, db ) )) ? f : e; } }
		return NULL; }
	else {
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

static BMQTake
verify_assignee( CNInstance *e, char *expression, BMQueryData *data ) {
	if ( !xp_verify( e, expression, data ) )
		return BMQ_CONTINUE;
	CNDB *db = data->db;
	CNInstance *star = data->user_data;
	if ( data->type==BM_INSTANTIATED ) {
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if (( g ) && db_manifested(g,db) ) {
				data->instance = g; // return ((*,e),.)
				return BMQ_DONE; }
			else if ( !g && db_manifested(f,db) ) {
				data->instance = f; // return (*,e)
				return BMQ_DONE; } } }
	else {
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr, *g; // f:(.,e)
			if ( f->sub[0]==star ) {
				// return ((*,.),.) or (*,.)
				data->instance = (( g=assignment(f,db) )) ? g: f;
				return BMQ_DONE; } } }
	return BMQ_CONTINUE; }

CNInstance *
bm_query_assigner( char *expression, BMContext *ctx )
/*
	Assumption: expression not "~."
*/ {
	// bm_case() usage
	CNInstance *e = BMContextRVV( ctx, "*^^" );
	if ( !e ) return NULL;
	BMQueryData data;
	memset( &data, 0, sizeof(data) );
	data.ctx = ctx;
	data.db = BMContextDB( ctx );
	return xp_verify( e, expression, &data ) ? e : NULL; }

