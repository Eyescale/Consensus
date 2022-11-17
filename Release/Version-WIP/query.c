#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate_param.h"
#include "locate_pivot.h"
#include "proxy.h"
#include "query.h"
#include "eenov.h"

// #define DEBUG

//===========================================================================
//	bm_query
//===========================================================================
typedef int XPTraverseCB( CNInstance *, char *, BMQueryData * );
static CNInstance * query_assignment( BMQueryType, char *, BMQueryData * );
static CNInstance * xp_traverse( char *, BMQueryData *, XPTraverseCB * );
static int xp_verify( CNInstance *, char *, BMQueryData * );

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
	int privy = ( type==BM_RELEASED ? 1 : 0 );
	CNInstance *success = NULL, *e;
	listItem *exponent = NULL;
	char *p = bm_locate_pivot( expression, &exponent );
	if (( p )) {
		e = bm_lookup( privy, p, ctx, db );
		if ( !e ) {
			freeListItem( &exponent );
			return NULL; }
		data.exponent = exponent;
		data.privy = strncmp( p, "%|", 2 ) ? privy : 2;
		data.pivot = newPair( p, e );
		success = xp_traverse( expression, &data, bm_verify );
		freePair( data.pivot );
		freeListItem( &exponent ); }
	else {
		listItem *s = NULL;
		switch ( type ) {
		case BM_CONDITION:
			for ( e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
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
					break; }
			break; } }
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
			BM_DONE : BM_CONTINUE; }
}

//---------------------------------------------------------------------------
//	xp_traverse
//---------------------------------------------------------------------------
static CNInstance *
xp_traverse( char *expression, BMQueryData *data, XPTraverseCB *traverse_CB )
/*
	Traverses data->pivot's exponent invoking traverse_CB on every match
	returns current match on the callback's BM_DONE, and NULL otherwise
	Assumption: pivot->value is not deprecated - and neither are its subs
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
	CNInstance *success = NULL;
#ifdef DEBUG
fprintf( stderr, "XP_TRAVERSE: privy=%d, pivot=", privy );
db_output( stderr, "", e, db );
fprintf( stderr, ", exponent=" );
xpn_out( stderr, exponent );
fprintf( stderr, "\n" );
#endif
	int exp;
	listItem *trail = NULL;
	listItem *stack = NULL;
	for ( ; ; ) {
		e = i->ptr;
		if (( exponent )) {
			exp = (int) exponent->ptr;
			if ( exp & 2 ) {
				e = CNSUB( e, exp&1 );
				if (( e )) {
					addItem ( &stack, i );
					addItem( &stack, exponent );
					exponent = exponent->next;
					i = newItem( e );
					continue; } } }
		if ( e == NULL )
			; // failed e->sub
		else if ( exponent == NULL ) {
			if (( lookupIfThere( trail, e ) ))
				; // ward off doublons
			else {
				addIfNotThere( &trail, e );
				if ( traverse_CB( e, expression, data )==BM_DONE ) {
					success = e;
					goto RETURN; } } }
		else {
			for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next )
				if ( !db_private( privy, j->ptr, db ) )
					break;
			if (( j )) {
				addItem( &stack, i );
				addItem( &stack, exponent );
				exponent = exponent->next;
				i = j; continue; } }
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				if ( !db_private( privy, i->ptr, db ) )
					break; }
			else if (( stack )) {
				exponent = popListItem( &stack );
				exp = (int) exponent->ptr;
				if ( exp & 2 ) freeItem( i );
				i = popListItem( &stack ); }
			else goto RETURN; } }
RETURN:
	freeListItem( &trail );
	while (( stack )) {
		exponent = popListItem( &stack );
		exp = (int) exponent->ptr;
		if ( exp & 2 ) freeItem( i );
		i = popListItem( &stack ); }
	if ( strncmp( pivot->name, "%|", 2 ) )
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
	listItem *mark_exp;
	listItem *flags;
	listItem *sub;
	listItem *as_sub;
	listItem *p;
	listItem *i;
} XPVerifyStack;
static char * pop_stack( XPVerifyStack *, listItem **i, listItem **mark, int *not );

static int
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
fprintf( stderr, "xp_verify: %s / candidate=", p );
db_output( stderr, "", x, db );
fprintf( stderr, " ........{\n" );
#endif
	XPVerifyStack stack;
	memset( &stack, 0, sizeof(stack) );
	listItem *exponent = NULL,
		*mark_exp = NULL,
		*i = newItem( x ), *j;
	int success=0, flags=FIRST;
	BMVerifyOp op = BM_INIT;

	BMTraverseData traverse_data;
	traverse_data.user_data = data;
	traverse_data.stack = &data->stack.flags;
	traverse_data.done = 0;
	for ( ; ; ) {
		x = i->ptr;
		if (( exponent )) {
			int exp = (int) exponent->ptr;
			if ( exp&2 ) {
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
		if (( x = op_set( op, data, x, &p, success ) )) {
			//----------------------------------------------------------

				p = verify_traversal( p, &traverse_data, flags );

			//----------------------------------------------------------
			if (( data->mark_exp )) {
				listItem *base = data->base;
				addItem( &data->stack.base, base );
				addItem( &data->stack.scope, data->stack.flags );
				listItem *sub_exp = NULL;
				for ( listItem *i=data->stack.exponent; i!=base; i=i->next )
					addItem( &sub_exp, i->ptr );
				while (( x ) && (sub_exp)) {
					int exp = pop_item( &sub_exp );
					x = CNSUB( x, exp&1 ); }
				flags = traverse_data.flags;
				// backup context
				addItem( &stack.mark_exp, mark_exp );
				add_item( &stack.flags, flags );
				addItem( &stack.sub, stack.as_sub );
				addItem( &stack.i, i );
				addItem( &stack.p, p );
				// setup new sub context
				f_clr( NEGATED )
				mark_exp = data->mark_exp;
				stack.as_sub = NULL;
				i = newItem( x );
				if (( x )) {
#ifdef DEBUG
					fprintf( stderr, "xp_verify: starting SUB, at '%s'\n", p );
#endif
					exponent = mark_exp;
					op = BM_BGN;
					success = 0;
					continue; }
				else {
					freeListItem( &sub_exp );
					success = 0; } }
			else {
#ifdef DEBUG
				fprintf( stderr, "xp_verify: returning %d, at '%s'\n", data->success, p );
#endif
				success = data->success; } }
		else success = 0;

		if (( mark_exp )) {
			if ( success ) {
				// move on past sub-expression
				pop_stack( &stack, &i, &mark_exp, &flags );
				if is_f( NEGATED ) { success = 0; f_clr( NEGATED ) }
				exponent = NULL;
				op = BM_END;
				continue; }
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
					if (!( exp & 2 )) freeItem( i );
					i = popListItem( &stack.as_sub ); }
				else {
					// move on past sub-expression
					char *start_p = pop_stack( &stack, &i, &mark_exp, &flags );
					if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
					if ( x == NULL ) {
						p = success ?
							p_prune( PRUNE_FILTER, start_p+1 ) :
							p_prune( PRUNE_TERM, start_p+1 );
						if ( *p==':' ) p++; }
					exponent = NULL;
					op = BM_END;
					break; }
			} }
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
	fprintf( stderr, "xp_verify:........} success=%d\n", success );
#endif
	return success;
}

static char *
pop_stack( XPVerifyStack *stack, listItem **i, listItem **mark_exp, int *flags )
{
	listItem *j = *i;
	while (( stack->as_sub )) {
		listItem *xpn = popListItem( &stack->as_sub );
		int exp = (int) xpn->ptr;
		if (!( exp & 2 )) freeItem( j );
		j = popListItem( &stack->as_sub ); }
	freeItem( j );
	freeListItem( mark_exp );
	stack->as_sub = popListItem( &stack->sub );
	*i = popListItem( &stack->i );
	*mark_exp = popListItem( &stack->mark_exp );
	*flags = pop_item( &stack->flags );
	return (char *) popListItem( &stack->p );
}

static CNInstance *
op_set( BMVerifyOp op, BMQueryData *data, CNInstance *x, char **q, int success )
{
	if ( !x ) return NULL;
	char *p = *q;
	switch ( op ) {
	case BM_BGN:
		if ( *p++=='*' ) {
			// take x.sub[0].sub[1] if x.sub[0].sub[0]==star
			CNInstance *y = CNSUB( x, 0 );
			if ( !y || y->sub[0]!=DBStar(data->db) ) {
				data->success = 0;
				x = NULL; break; }
			x = y->sub[ 1 ]; }
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
		   pushed in case of "single" expressions - e.g. %((.,?):%?)
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
	bm_locate_mark( p+1, &data->mark_exp );
	if (( data->mark_exp ))
		_return( 1 )
	_break
case_( dot_expression_CB )
	xpn_add( &data->stack.exponent, AS_SUB, 0 );
	switch ( match( data->instance, p, data->base, data ) ) {
	case -1: data->success = 0;
		_prune( BM_PRUNE_TERM )
	case  0: data->success = is_f( NEGATED ) ? 1 : 0;
		_prune( data->success ? BM_PRUNE_FILTER : BM_PRUNE_TERM )
	case  1: xpn_set( data->stack.exponent, AS_SUB, 1 ); }
	_break
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

	// name-value-based testing: note that %! MUST come first
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
				( y== BMContextPerso(ctx) ) );
		case '*':
			return ( y==DBStar(data->db) );
		case '%':
			switch ( p[1] ) {
			case '?': return ( y==bm_context_lookup( ctx, "?" ) );
			case '!': return ( y==bm_context_lookup( ctx, "!" ) );
			case '%': return ( y==BMContextSelf(ctx) );
			case '<': return eenov_match( ctx, p, data->db, y ); }
			return ( !y->sub[0] && *db_identifier(y,data->db)=='%' );
		default:
			if ( is_separator(*p) ) break;
			CNInstance *found = bm_context_lookup( ctx, p );
			if (( found )) return ( y==found ); }

	// not found in data->ctx
	if ( !y->sub[0] ) {
		char *identifier = db_identifier( y, data->db );
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
static inline CNInstance * assignment( CNInstance *e, CNDB *db );
static XPTraverseCB
	bm_verify_unassigned, bm_verify_variable, bm_verify_value;

static CNInstance *
query_assignment( BMQueryType type, char *expression, BMQueryData *data )
{
	BMContext *ctx = data->ctx;
	CNDB *db = data->db;
	CNInstance *star = DBStar( db );
	CNInstance *success = NULL, *e;
	expression++; // skip leading ':'
	char *value = p_prune( PRUNE_FILTER, expression ) + 1;
	if ( !strncmp( value, "~.", 2 ) ) {
		listItem *exponent = NULL;
		char *p = bm_locate_pivot( expression, &exponent );
		if (( p )) {
			e = bm_lookup( 0, p, ctx, db );
			if ( !e ) {
				freeListItem( &exponent );
				return NULL; }
			data->exponent = exponent;
			data->pivot = newPair( p, e );
			success = xp_traverse( expression, data, bm_verify_unassigned );
			if (( success )) success = data->instance; // ( *, success )
			freePair( data->pivot );
			freeListItem( &exponent ); }
		else {
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
						break; } }
				break;
			} } }
	else {
		listItem *exponent = NULL;
		char *p = bm_locate_pivot( expression, &exponent );
		if (( p )) {
			e = bm_lookup( 0, p, ctx, db );
			if ( !e ) {
				freeListItem( &exponent );
				return NULL; }
			data->exponent = exponent;
			data->pivot = newPair( p, e );
			data->user_data = value;
			success = xp_traverse( expression, data, bm_verify_value );
			if (( success )) success = data->instance; // ((*,success),.)
			freePair( data->pivot );
			freeListItem( &exponent ); }
		else {
			p = bm_locate_pivot( value, &exponent );
			if (( p )) {
				e = bm_lookup( 0, p, ctx, db );
				if ( !e ) {
					freeListItem( &exponent );
					return NULL; }
				data->exponent = exponent;
				data->pivot = newPair( p, e );
				data->user_data = expression;
				success = xp_traverse( value, data, bm_verify_variable );
				if (( success )) success = data->instance; // ((*,.),success)
				freePair( data->pivot );
				freeListItem( &exponent ); }
			else {
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
							break; } }
					break;
			} } } }
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
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = data->db;
	CNInstance *star = DBStar( db );
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]!=star ) continue;
		if ( manifest && !db_manifested(e,db) )
			continue;
		if ( assignment(e,db) ) continue;
		data->instance = e; // return (*,e)
		return BM_DONE; }
	return BM_CONTINUE;
}
static int
bm_verify_value( CNInstance *e, char *variable, BMQueryData *data )
{
	if ( !xp_verify( e, variable, data ) )
		return BM_CONTINUE;
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = data->db;
	CNInstance *star = DBStar( db );
	char *value = data->user_data;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		CNInstance *f = i->ptr, *g; // f:(.,e)
		if ( f->sub[0]!=star || !(g=assignment(f,db)) )
			continue;
		if ( manifest && !db_manifested(g,db) )
			continue;
		if ( xp_verify( g->sub[1], value, data ) ) {
			data->instance = g; // return ((*,e),.)
			return BM_DONE; } }
	return BM_CONTINUE;
}
static int
bm_verify_variable( CNInstance *e, char *value, BMQueryData *data )
{
	if ( !xp_verify( e, value, data ) )
		return BM_CONTINUE;
	int manifest = ( data->type==BM_INSTANTIATED );
	CNDB *db = data->db;
	CNInstance *star = DBStar( db );
	char *variable = data->user_data;
	for ( listItem *i=e->as_sub[1]; i!=NULL; i=i->next ) {
		e = i->ptr; // e:(.,e)
		if ( e->sub[0]->sub[0]!=star || db_private(0,e,db) )
			continue;
		if ( manifest && !db_manifested(e,db) )
			continue;
		CNInstance *f = e->sub[ 0 ]; // e:(f:(*,.),e)
		if ( xp_verify( f->sub[1], variable, data ) ) {
			data->instance = e; // return ((*,.),e)
			return BM_DONE; } }
	return BM_CONTINUE;
}
static inline CNInstance *
assignment( CNInstance *e, CNDB *db )
{
	for ( listItem *j=e->as_sub[0]; j!=NULL; j=j->next )
		if ( !db_private( 0, j->ptr, db ) )
			return j->ptr;
	return NULL;
}

