#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_pivot.h"
#include "locate_mark.h"
#include "proxy.h"
#include "query.h"
#include "query_private.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_query
//===========================================================================
static CNInstance * query_assignment( BMQueryType, char *, BMQueryData * );

CNInstance *
bm_query( BMQueryType type, char *expression, BMContext *ctx,
	  BMQueryCB user_CB, void *user_data )
/*
	find the first term other than '.' or '?' in expression which is not
	negated. if found then test each entity in term.exponent against
	expression. otherwise test every entity in CNDB against expression.
	if no user callback is provided, returns first entity that passes.
	otherwise, invokes callback for each entity that passes, and returns
	first entity for which user callback returns BM_DONE.
*/
{
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
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY: %s\n", expression );
#endif
	int privy = data.privy = ( type==BM_RELEASED ? 1 : 0 );
	CNInstance *success = NULL, *e;
	IFN_PIVOT( privy, expression, (&data), bm_verify, NULL ) {
		listItem *s = NULL;
		switch ( type ) {
		case BM_CONDITION:
			for ( e=DBFirst(db,&s); e!=NULL; e=DBNext(db,e,&s) )
				if ( bm_verify( e, expression, &data )==BM_DONE ) {
					freeListItem( &s );
					success = e;
					break; }
			break;
		default:
			for ( e=db_log(1,privy,db,&s); e!=NULL; e=db_log(0,privy,db,&s) )
				if ( xp_verify( e, expression, &data ) ) {
					freeListItem( &s );
					success = e;
					break; } } }
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY: success=%d\n", !!success );
#endif
	return success;
}
int
bm_verify( CNInstance *e, char *expression, BMQueryData *data )
{
	CNDB *db;
	switch ( data->type ) {
	case BM_INSTANTIATED:
		return ( db_manifested(e,data->db) && xp_verify(e,expression,data)) ?
			BM_DONE : BM_CONTINUE;
	case BM_RELEASED:
		return ( db_deprecated(e,data->db) && xp_verify(e,expression,data)) ?
			BM_DONE : BM_CONTINUE;
	default:
		return xp_verify( e, expression, data ) ?
			(data->user_CB) ?
				data->user_CB( e, data->ctx, data->user_data ) :
				BM_DONE :
			BM_CONTINUE; }
}

//---------------------------------------------------------------------------
//	pivot_check	- IFN_PIVOT internal
//---------------------------------------------------------------------------
static inline char *
pivot_check( char *p, listItem **exponent, listItem **xpn, BMQueryData *data )
{
	if ((p) && *p=='%' ) {
		switch ( p[1] ) {
		case '|':
			data->privy = 2;
			return p;
		case '(': ; // %(list,?:...)
			Pair *list = popListItem( exponent );
			char *list_p = list->name;
			p = bm_locate_pivot( list_p+2, xpn );
			if ((p) && strncmp(p,"%(",2))
				data->list = newPair( list, *xpn );
			else {
				freeListItem( xpn );
				freePair( list );
				p = NULL; }
			break; } }
	return p;
}

//---------------------------------------------------------------------------
//	xp_traverse	- IFN_PIVOT internal
//---------------------------------------------------------------------------
static CNInstance *
xp_traverse( char *expression, BMQueryData *data, XPTraverseCB *traverse_CB, void *user_data )
/*
	Traverses data->pivot's exponent invoking traverse_CB on every match
	returns current match on the callback's BM_DONE, and NULL otherwise
	Assumptions
	. user_data only specified for query_assignment, in which case
	  we can overwrite data->user_data with the passed user_data
	. pivot->value is not deprecated - and neither are its subs
	. _POP set to POP_LAST
*/
{
	CNDB *db = data->db;
	int privy = data->privy;
	Pair *pivot = data->pivot;
	CNInstance *e;
	listItem *i, *j;
	if ( !strncmp( pivot->name, "%|", 2 ) ) {
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
	listItem *stack = NULL;
	for ( ; ; ) {
PUSH_exp:	PUSH( stack, exponent, POP_exp )
Execute:	if ( !lookupIfThere( trail, e ) ) { // ward off doublons
			addIfNotThere( &trail, e );
			if ( traverse_CB( e, expression, data )==BM_DONE ) {
				success = e;
				goto RETURN; } }
POP_exp:	POP( stack, exponent, PUSH_exp, NULL ) }
RETURN:
	freeListItem( &trail );
	FLUSH( stack )
	if ( strncmp( pivot->name, "%|", 2 ) )
		freeItem( i );
	return success;
}

//---------------------------------------------------------------------------
//	xp_traverse_list	- IFN_PIVOT internal
//---------------------------------------------------------------------------
#undef _POP
#define _POP( NEXT ) POP_( NEXT )
static XPTraverseCB list_verify;

static CNInstance *
xp_traverse_list( char *expression, BMQueryData *data, XPTraverseCB *traverse_CB, void *user_data )
/*
	Traverses data->pivot's xpn.as_sub[0]^n.sub[1].exponent (n>=1)
		invoking traverse_CB on each match.
	returns current match on the callback's BM_DONE, and NULL otherwise
	Assumptions
	. we have data->list:[ list:[ list_p, next_p ], xpn ] where
		expression is in the form  ...%(list,?:...)...
			list_p ---------------^            ^
			next_p ----------------------------
	. user_data only specified for query_assignment, in which case
	  we can overwrite data->user_data with the passed user_data
	. pivot->value is not deprecated - and neither are its subs
	. _POP set to POP_( NEXT )
*/
{
	CNInstance *success = NULL;
	Pair *list = data->list->name;
	char *list_p = list->name;
	char *next_p = list->value;
	listItem *xpn = data->list->value;
	listItem *trail = NULL;
	char *clamped = NULL;
	int hacked = 0;

	/* Special cases: expression is in either form
			%(list,?:...):_
			%(list,?:...)
			_:%(list,?:...):_
			_:%(list,?:...)
	*/
	if ( list_p==expression ) {
		switch ( *next_p ) {
		case ':': // we have expression=="%(list,?:...):_"
			expression = next_p + 1;
			break;
		default:  // we have expression=="%(list,?:...)"
			data->exponent = xpn;
			data->list->name = traverse_CB;
			data->list->value = &trail;
			success = xp_traverse( list_p+2, data, list_verify, user_data );
			data->exponent = NULL;
			goto DONE; } }
	else if ( list_p[-1]==':' ) {
		switch ( *next_p ) {
		case ':': // we have expression=="_:%(list,?:...):_"
			clamped = expression;
			CNString *s = newString();
			s_append( expression, list_p );
			s_add( next_p+1 );
			expression = StringFinish( s, 0 );
			StringReset( s, CNStringMode );
			freeString( s );
			break;
		default:  // we have expression=="_:%(list,?:...)"
			list_p[-1]='\0'; hacked=1; } }

	/* General case: expression is in form "_%(list,?:...)_"
	*/
	CNDB *db = data->db;
	int privy = data->privy;
	Pair *pivot = data->pivot;
	CNInstance *e, *nil = db->nil;
	listItem *i, *j;
	if ( !strncmp( pivot->name, "%|", 2 ) ) {
		i = pivot->value;
		e = i->ptr; }
	else {
		e = pivot->value;
		i = newItem( e ); }
	listItem *exponent = data->exponent;
	if (( user_data ))
		data->user_data = user_data;

	listItem *stack[ 3 ] = { NULL, NULL, NULL };
	enum { XPN_id, LIST_id, EXP_id };
	for ( ; ; ) {
PUSH_xpn:	PUSH( stack[ XPN_id ], xpn, POP_xpn )
PUSH_list: 	PUSH_LIST( stack[ LIST_id ], POP_list )
PUSH_exp:	PUSH( stack[ EXP_id ], exponent, POP_exp )
Execute:	if ( !lookupIfThere( trail, e ) ) { // ward off doublons
			addIfNotThere( &trail, e );
			if ( traverse_CB( e, expression, data )==BM_DONE ) {
				success = e;
				goto RETURN; } }
POP_exp:	POP( stack[ EXP_id ], exponent, PUSH_exp, LIST_id )
POP_list:	POP_LIST( stack[ LIST_id ], PUSH_list, XPN_id )
#undef _POP
#define _POP( NEXT ) POP_LAST
POP_xpn:	POP( stack[ XPN_id ], xpn, PUSH_list, NULL ) }
RETURN:
	freeListItem( &stack[ LIST_id ] );
	FLUSH( stack[ EXP_id ] )
	FLUSH( stack[ XPN_id ] )
	if ( strncmp( pivot->name, "%|", 2 ) )
		freeItem( i );
	if (( clamped )) { free( expression ); expression=clamped; }
	else if ( hacked ) list_p[-1] = ':';
DONE:
	freeListItem( &trail );
	freePair( list );
	freeListItem( &xpn );
	freePair( data->list );
	data->list = NULL;
	return success;
}

static int
list_verify( CNInstance *e, char *expression, BMQueryData *data )
/*
	invoke bm_verify on all { e.as_sub[0]^n.sub[1] / n>=1 }
	Assumption: _POP set to POP_LAST
*/
{
	XPTraverseCB *traverse_CB = data->list->name;
	listItem **trail = data->list->value;
	CNInstance *nil = data->db->nil;
	listItem *i = newItem( e ), *j;
	int success = BM_CONTINUE;
	listItem *stack = NULL;
	for ( ; ; ) {
PUSH_list:	PUSH_LIST( stack, POP_list );
Execute:	if ( !lookupIfThere( *trail, e ) ) { // ward off doublons
			addIfNotThere( trail, e );
			if ( traverse_CB(e,expression,data)==BM_DONE ) {
				success = BM_DONE;
				goto RETURN; } }
POP_list:	POP_LIST( stack, PUSH_list, NULL ); }
RETURN:
	freeListItem( &stack );
	freeItem( i );
	return success;
}

//===========================================================================
//	xp_verify
//===========================================================================
#include "verify_traversal.h"

typedef enum { BM_INIT, BM_BGN, BM_END } BMVerifyOp;
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
static inline CNInstance * xsub( CNInstance *, listItem *, listItem * );
static inline char * prune( char *, int );

int
xp_verify( CNInstance *x, char *expression, BMQueryData *data )
/*
	Assumption: expression is not ternarized
	invokes verify_traversal on each [sub-]expression, ie. on each
	expression term starting with '*' or '%' - note that *var is same
	as %((*,var),?)
*/
{
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
			int exp = (int) exponent->ptr;
			if ( exp & SUB ) {
				for ( j=x->as_sub[ exp&1 ]; j!=NULL; j=j->next )
					if ( !db_private( privy, j->ptr, db ) )
						break;
				if (( j )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = j; continue; }
				else x = NULL; }
			else {
				x = CNSUB( x, exp&1 );
				if (( x )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = newItem( x );
					continue; } } }

		if ((x) && ( x = op_set( op, data, x, &p, success ) )) {
			//----------------------------------------------------------

				p = verify_traversal( p, &traverse_data, flags );

			//----------------------------------------------------------
			if (( data->mark_exp )) {
				flags = traverse_data.flags;
				listItem *base = data->base;
				addItem( &data->stack.base, base );
				addItem( &data->stack.scope, data->stack.flags );
				if (( x = xsub( x, data->stack.exponent, base ) )) {
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

		if (( mark_exp )) {
			if ( success ) {
				// restore context & move on past sub-expression
				if ( list_exp==2 ) {
					freeItem( i ); i = list_i;
					list_i = popListItem( &stack.list_i ); }
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
				if ( list_exp ) { // %( list, ?:... )
					CNInstance *y = (x) ? CNSUB(x,0) : NULL;
					if ( !y ) {
						if ( list_exp==2 ) {
							list_exp = 1;
							freeItem( i ); i = list_i;
							list_i = popListItem( &stack.list_i ); } }
					else {
						if ( list_exp==1 ) {
							list_exp = 2;
							addItem( &stack.list_i, list_i );
							list_i = i; i = newItem( y ); }
						else {
							i->ptr = y; }
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
						int exp = (int) exponent->ptr;
						if (!( exp & SUB )) freeItem( i );
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
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on exponent %d %d\n",
			(int)data->stack.flags, (int)data->stack.exponent );
		exit( -1 ); }
	freeListItem( &data->stack.flags );
	freeListItem( &data->stack.exponent );
	if ((data->stack.scope) || (data->stack.base)) {
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on scope\n" );
		exit( -1 ); }
	fprintf( stderr, "xp_verify:........} success=%d\n", success );
#endif
	return success;
}

static inline CNInstance *
xsub( CNInstance *x, listItem *xpn, listItem *base )
{
	listItem *sub_exp = NULL;
	for ( listItem *i=xpn; i!=base; i=i->next )
		addItem( &sub_exp, i->ptr );
	while (( x ) && (sub_exp)) {
		int exp = pop_item( &sub_exp );
		x = CNSUB( x, exp&1 ); }
	freeListItem( &sub_exp );
	return x;
}
static inline char *
prune( char *p, int success )
{
	p = p_prune(( success ? PRUNE_FILTER : PRUNE_TERM ), p+1 );
	if ( *p==':' ) p++;
	return p;
}
static inline listItem *
pop_as_sub( XPVerifyStack *stack, listItem *i, listItem **mark_exp )
/*
	free as_sub exponent stack and associated items
	(if these were reallocated) starting from i
*/
{
	listItem **as_sub = &stack->as_sub;
	while (( *as_sub )) {
		listItem *xpn = popListItem( as_sub );
		int exp = (int) xpn->ptr;
		if (!( exp & SUB )) freeItem( i );
		i = popListItem( as_sub ); }
	freeItem( i );
	stack->as_sub = popListItem( &stack->sub );
	// free mark_exp exponent stack
	freeListItem( mark_exp );
	// return previously stacked i
	return popListItem( &stack->i );
}

//---------------------------------------------------------------------------
//	op_set
//---------------------------------------------------------------------------
static CNInstance *
op_set( BMVerifyOp op, BMQueryData *data, CNInstance *x, char **q, int success )
{
	char *p = *q;
	switch ( op ) {
	case BM_BGN:
		if ( *p++=='*' ) {
			// take x.sub[0].sub[1] if x.sub[0].sub[0]==star
			CNInstance *y = CNSUB( x, 0 );
			if ( y && DBStarMatch( y->sub[0], data->db ) )
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
	return x;
}

//---------------------------------------------------------------------------
//	verify_traversal
//---------------------------------------------------------------------------
static int match( CNInstance *, char *, listItem *, BMQueryData * );

BMTraverseCBSwitch( verify_traversal )
case_( match_CB )
	switch ( match( data->instance, p, data->base, data ) ) {
	case -1: data->success = 0; break;
	case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
	case  1: data->success = is_f( NEGATED ) ? 0 : 1; break; }
	_break
case_( dot_identifier_CB )
	xpn_add( &data->stack.exponent, AS_SUB, 0 );
	switch ( match( data->instance, p, data->base, data ) ) {
	case -1: data->success = 0; break;
	case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
	case  1:
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		switch ( match( data->instance, p+1, data->base, data ) ) {
		case -1: data->success = 0; break;
		case  0: data->success = is_f( NEGATED ) ? 1 : 0; break;
		default: data->success = is_f( NEGATED ) ? 0 : 1; break; } }
	popListItem( &data->stack.exponent );
	_break
case_( dereference_CB )
	xpn_add( &data->mark_exp, SUB, 1 );
	_return( 1 )
case_( sub_expression_CB )
	p = bm_locate_mark( p+1, &data->mark_exp );
	if (( data->mark_exp )) {
		if ( !strncmp( p+1, ":...", 4 ) )
			data->list_exp = 1;
		_return( 1 ) }
	_break
case_( dot_expression_CB )
	listItem **stack = &data->stack.exponent;
	xpn_add( stack, AS_SUB, 0 );
	switch ( match( data->instance, p, data->base, data ) ) {
	case 0: if is_f( NEGATED ) {
			data->success=1; popListItem( stack );
			_prune( BM_PRUNE_FILTER ) }
		// no break
	case -1:
		data->success=0; popListItem( stack );
		_prune( BM_PRUNE_TERM )
	default:
		xpn_set( *stack, AS_SUB, 1 );
		_break }
case_( open_CB )
	if ( f_next & COUPLE )
		xpn_add( &data->stack.exponent, AS_SUB, 0 );
	_break
case_( filter_CB )
	if ( data->op==BM_BGN && data->stack.flags==data->OOS )
		_return( 1 )
	else if ( !data->success )
		_prune( BM_PRUNE_TERM )
	else _break
case_( decouple_CB )
	if ( data->stack.flags==data->OOS )
		_return( 1 )
	else if ( !data->success )
		_prune( BM_PRUNE_TERM )
	else {
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		_break }
case_( close_CB )
	if ( data->stack.flags==data->OOS )
		_return( 1 )
	if is_f( COUPLE ) popListItem( &data->stack.exponent );
	if is_f( DOT ) popListItem( &data->stack.exponent );
	if ( f_next & NEGATED ) data->success = !data->success;
	if ( data->op==BM_END && data->stack.flags==data->OOS && (data->stack.scope))
		traverse_data->done = 1; // after popping
	_break
case_( wildcard_CB )
	if is_f( NEGATED ) data->success = 0;
	else if ( !data->stack.exponent || (int)data->stack.exponent->ptr==1 )
		data->success = 1; // wildcard is any or as_sub[1]
	else switch ( match( data->instance, NULL, data->base, data ) ) {
		case -1: // no break
		case  0: data->success = 0; break;
		case  1: data->success = 1; break; }
	_break
BMTraverseCBEnd

//---------------------------------------------------------------------------
//	match
//---------------------------------------------------------------------------
static int
match( CNInstance *x, char *p, listItem *base, BMQueryData *data )
/*
	tests x.sub[kn]...sub[k0] where exponent=as_sub[k0]...as_sub[kn]
	is either NULL or the exponent of p as expression term
*/
{
	CNEntity *that;
	listItem *xpn = NULL;
	for ( listItem *i=data->stack.exponent; i!=base; i=i->next )
		addItem( &xpn, i->ptr );
	CNInstance *y = x;
	while (( y ) && (xpn)) {
		int exp = pop_item( &xpn );
		y = CNSUB( y, exp&1 ); }
	if ( y == NULL ) {
		freeListItem( &xpn );
		return -1; }
	else if ( p == NULL ) // wildcard
		return 1;

	// name-value-based testing: note that %| MUST come first
	BMContext *ctx = data->ctx;
	if ( !strncmp( p, "%|", 2 ) ) {
		listItem *v = bm_context_lookup( ctx, "|" );
		return !!lookupItem( v, y ); }
	else if (( data->pivot ) && p==data->pivot->name )
		return ( y==data->pivot->value );
	else switch ( *p ) {
		case '.':
			return ( p[1]=='.' ?
				( y==BMContextParent(ctx) ) :
				( y==BMContextPerso(ctx) ) );
		case '*':
			return DBStarMatch( y, data->db );
		case '%':
			switch ( p[1] ) {
			case '?': return ( y==bm_context_lookup( ctx, "?" ) );
			case '!': return ( y==bm_context_lookup( ctx, "!" ) );
			case '%': return ( y==BMContextSelf(ctx) );
			case '<': return eenov_match( ctx, p, data->db, y ); }
			return ( !y->sub[0] && *DBIdentifier(y,data->db)=='%' );
		default:
			if ( is_separator(*p) ) break;
			CNInstance *found = bm_context_lookup( ctx, p );
			if (( found )) return ( y==found ); }

	// not found in data->ctx
	if ( !y->sub[0] ) {
		char *identifier = DBIdentifier( y, data->db );
		char_s q;
		switch ( *p ) {
		case '/': return !strcomp( p, identifier, 2 );
		case '\'': return charscan(p+1,&q) && !strcomp( q.s, identifier, 1 );
		default: return !strcomp( p, identifier, 1 ); } }

	return 0; // not a base entity
}

//===========================================================================
//	query_assignment
//===========================================================================
#undef _XP_TRAVERSE // IFN_PIVOT internal
#define _XP_TRAVERSE XP_TRAVERSE_ASSIGNMENT

static XPTraverseCB
	bm_verify_unassigned, bm_verify_variable, bm_verify_value;

static CNInstance *
query_assignment( BMQueryType type, char *expression, BMQueryData *data )
{
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	CNInstance *star = db_lookup( 0, "*", db );
	if ( !star ) return NULL;
	data->star = star;
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	CNInstance *success = NULL, *e;
	if ( !strncmp( value, "~.", 2 ) ) {
		IFN_PIVOT( 0, expression, data, bm_verify_unassigned, NULL ) {
			switch ( type ) {
			case BM_CONDITION: ;
				for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
					CNInstance *e = i->ptr;
					if (( assignment(e,db) )) continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						success = e; // return (*,.)
						break; } }
				break;
			default: ;
				listItem *s = NULL;
				for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
					if ( e->sub[0]!=star || (assignment(e,db)) )
						continue;
					if ( xp_verify( e->sub[1], expression, data ) ) {
						freeListItem( &s );
						success = e; // return (*,.)
						break; } } } } }
	else {
		IFN_PIVOT( 0, expression, data, bm_verify_value, value ) {
			IFN_PIVOT( 0, value, data, bm_verify_variable, expression ) {
				switch ( type ) {
				case BM_CONDITION: ;
					for ( listItem *i=star->as_sub[0]; i!=NULL; i=i->next ) {
						CNInstance *e = i->ptr, *f;
						if ( db_private(0,e,db) || !(f=assignment(e,db)) )
							continue;
						if ( xp_verify( e->sub[1], expression, data ) &&
						     xp_verify( f->sub[1], value, data ) ) {
							success = f;	// return ((*,.),.)
							break; } }
					break;
				default: ;
					listItem *s = NULL;
					for ( e=db_log(1,0,db,&s); e!=NULL; e=db_log(0,0,db,&s) ) {
						CNInstance *f = CNSUB( e, 0 );
						if ( !f || f->sub[0]!=star ) continue;
						if ( xp_verify( f->sub[1], expression, data ) &&
						     xp_verify( e->sub[1], value, data ) ) {
							freeListItem( &s );
							success = e; // return ((*,.),.)
							break; } } } } } }
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY_ASSIGNMENT: success=%d\n", !!success );
#endif
	return success;
}
static int
bm_verify_unassigned( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) )
		return BM_CONTINUE;
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
			return BM_DONE; }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			if ( assignment(f,db) ) continue;
			data->instance = f; // return (*,e)
			return BM_DONE; } }

	return BM_CONTINUE;
}
static int
bm_verify_value( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) )
		return BM_CONTINUE;
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
				return BM_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *f = i->ptr; // f:(.,e)
			if ( f->sub[0]!=star ) continue;
			CNInstance *g = assignment(f,db);
			if ( !g ) continue;
			if ( xp_verify( g->sub[1], value, data ) ) {
				data->instance = g; // return ((*,e),.)
				return BM_DONE; } } }
	return BM_CONTINUE;
}
static int
bm_verify_variable( CNInstance *e, char *value, BMQueryData *data )
{
	if ( !xp_verify( e, value, data ) )
		return BM_CONTINUE;
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
				return BM_DONE; } }
		break;
	default:
		for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
			CNInstance *g = i->ptr; // g:(.,e)
			if ( g->sub[0]->sub[0]!=star || db_private(0,g,db) )
				continue;
			CNInstance *f = g->sub[ 0 ]; // g:(f:(*,.),e)
			if ( xp_verify( f->sub[1], variable, data ) ) {
				data->instance = g; // return ((*,.),e)
				return BM_DONE; } } }
	return BM_CONTINUE;
}

