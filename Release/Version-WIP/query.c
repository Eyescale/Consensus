#include <stdio.h>
#include <stdlib.h>

#include "query_private.h"
#include "string_util.h"
#include "locate.h"

// #define DEBUG

#ifdef UNIFIED
#define ESUB(e,ndx) ( e->sub[0] ? e->sub[ndx] : NULL )
#else
#define ESUB(e,ndx) e->sub[ndx]
#endif

static CNInstance *xp_traverse( char *, BMQueryData * );
static int bm_verify( CNInstance *e, char *, BMQueryData *data );
static int xp_verify( CNInstance *, char *, BMQueryData * );

//===========================================================================
//	bm_query
//===========================================================================
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
	if ( !expression || !(*expression)) return NULL;
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY: %s\n", expression );
#endif

	BMQueryData data;
	memset( &data, 0, sizeof(BMQueryData));
	data.ctx = ctx;
	data.user_CB = user_CB;
	data.user_data = user_data;

	CNDB *db = BMContextDB( ctx );
	CNInstance *success = NULL, *e;
	switch ( type ) {
	case BM_CONDITION: ;
		listItem *exponent = NULL;
		char *p = bm_locate_pivot( expression, &exponent );
		if (( p )) {
			e = bm_lookup( 0, p, ctx );
			if ( !e ) {
				freeListItem( &exponent );
				break;
			}
			data.exponent = exponent;
			data.privy = !strncmp( p, "%!", 2 ) ? 2 : 0;
			data.star = db_lookup( data.privy, "*", db );
			data.pivot = newPair( p, e );
			success = xp_traverse( expression, &data );
			freePair( data.pivot );
			freeListItem( &exponent );
			break;
		}
		listItem *s = NULL;
		for ( e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) ) {
			if ( bm_verify( e, expression, &data )==BM_DONE ) {
				freeListItem( &s );
				success = e;
				break;
			}
		}
		break;
	case BM_RELEASED:
		data.privy = 1;
		// no break
	case BM_INSTANTIATED:
		data.star = db_lookup( data.privy, "*", db );
		s = NULL;
		for ( e = db_log( 1, data.privy, db, &s ); e!=NULL;
		      e = db_log( 0, data.privy, db, &s ) ) {
			if ( xp_verify( e, expression, &data ) ) {
				freeListItem( &s );
				success = e;
				break;
			}
		}
		break;
	}
#ifdef DEBUG
	fprintf( stderr, "BM_QUERY: success=%d\n", !!success );
#endif
	return success;
}

//===========================================================================
//	xp_traverse
//===========================================================================
static CNInstance *
xp_traverse( char *expression, BMQueryData *data )
/*
	Traverses data->pivot's exponent invoking verify on every match
	returns current match on the callback's BM_DONE, and NULL otherwise
	Assumption: pivot->value is not deprecated - and neither are its subs
*/
{
	CNDB *db = BMContextDB( data->ctx );
	int privy = data->privy;
	Pair *pivot = data->pivot;
	CNInstance *e;
	listItem *i, *j;
	if ( !strncmp( pivot->name, "%!", 2 ) ) {
		i = pivot->value;
		e = i->ptr; }
	else {
		e = pivot->value;
		i = newItem( e );
	}
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
				e = ESUB( e, exp&1 );
				if (( e )) {
					addItem ( &stack, i );
					addItem( &stack, exponent );
					exponent = exponent->next;
					i = newItem( e );
					continue;
				}
			}
		}
		if ( e == NULL )
			; // failed e->sub
		else if ( exponent == NULL ) {
			if (( lookupIfThere( trail, e ) ))
				; // ward off doublons
			else {
				addIfNotThere( &trail, e );
				if ( bm_verify( e, expression, data )==BM_DONE ) {
					success = e;
					goto RETURN;
				}
			}
		}
		else {
			for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next )
				if ( !db_private( privy, j->ptr, db ) )
					break;
			if (( j )) {
				addItem( &stack, i );
				addItem( &stack, exponent );
				exponent = exponent->next;
				i = j; continue;
			}
		}
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				if ( !db_private( privy, i->ptr, db ) )
					break;
			}
			else if (( stack )) {
				exponent = popListItem( &stack );
				exp = (int) exponent->ptr;
				if ( exp & 2 ) freeItem( i );
				i = popListItem( &stack );
			}
			else goto RETURN;
		}
	}
RETURN:
	freeListItem( &trail );
	while (( stack )) {
		exponent = popListItem( &stack );
		exp = (int) exponent->ptr;
		if ( exp & 2 ) freeItem( i );
		i = popListItem( &stack );
	}
	if ( strncmp( pivot->name, "%!", 2 ) )
		freeItem( i );
	return success;
}

//===========================================================================
//	bm_verify
//===========================================================================
static int
bm_verify( CNInstance *e, char *expression, BMQueryData *data )
{
	return xp_verify( e, expression, data ) ?
		( !data->user_CB ) ? BM_DONE :
			data->user_CB( e, data->ctx, data->user_data ) :
		BM_CONTINUE;
}

//===========================================================================
//	xp_verify
//===========================================================================
static int verify( int op, CNInstance *, char **, BMQueryData * );
typedef struct {
	listItem *mark_exp;
	listItem *flags;
	listItem *sub;
	listItem *as_sub;
	listItem *p;
	listItem *i;
} XPVerifyStack;
static char *pop_stack( XPVerifyStack *, listItem **i, listItem **mark, int *not );

static int
xp_verify( CNInstance *x, char *expression, BMQueryData *data )
{
	CNDB *db = BMContextDB( data->ctx );
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
	int	op = BM_INIT,
		success = 0,
		flags = FIRST;
	for ( ; ; ) {
		x = i->ptr;
		if (( exponent )) {
			int exp = (int) exponent->ptr;
			if ( exp & 2 ) {
				for ( j = x->as_sub[ exp & 1 ]; j!=NULL; j=j->next )
					if ( !db_private( privy, j->ptr, db ) ) break;
				if (( j )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = j; continue;
				}
				else x = NULL;
			}
			else {
				x = ESUB( x, exp&1 );
				if (( x )) {
					addItem( &stack.as_sub, i );
					addItem( &stack.as_sub, exponent );
					exponent = exponent->next;
					i = newItem( x );
					continue;
				}
			}
		}
		if (( x )) {
			data->sub_exp = NULL;
			data->mark_exp = NULL;
			data->success = success;
			data->flags = flags;
			switch ( op ) {
			case BM_BGN:
				/* take x.sub[0].sub[1] if x.sub[0].sub[0]==star
				   Note that star may be null (not instantiated)
				*/
				if ( *p++ == '*' ) {
					CNInstance *y = x->sub[ 0 ];
					if ((y) && (y->sub[0]) && (y->sub[0]==data->star))
						x = y->sub[ 1 ];
					else { data->success = 0; x = NULL; break; }
				}
				// no break
			default:
				verify( op, x, &p, data );
			}
			if (( data->mark_exp )) {
				listItem **sub_exp = &data->sub_exp;
				while (( x ) && (*sub_exp)) {
					int exp = pop_item( sub_exp );
					x = ESUB( x, exp&1 );
				}
				flags = data->flags;
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
					exponent = mark_exp;
					op = BM_BGN;
					success = 0;
					continue;
				}
				else {
					freeListItem( sub_exp );
					success = 0;
				}
			}
			else success = data->success;
		}
		else success = 0;

		if (( mark_exp )) {
			if ( success ) {
				// move on past sub-expression
				pop_stack( &stack, &i, &mark_exp, &flags );
				if is_f( NEGATED ) { success = 0; f_clr( NEGATED ) }
				exponent = NULL;
				op = BM_END;
				continue;
			}
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					if ( !db_private( privy, i->ptr, db ) ) {
						p = stack.p->ptr;
						op = BM_BGN;
						success = 0;
						break;
					}
				}
				else if (( stack.as_sub )) {
					exponent = popListItem( &stack.as_sub );
					int exp = (int) exponent->ptr;
					if (!( exp & 2 )) freeItem( i );
					i = popListItem( &stack.as_sub );
				}
				else {
					// move on past sub-expression
					char *start_p = pop_stack( &stack, &i, &mark_exp, &flags );
					if is_f( NEGATED ) { success = 1; f_clr( NEGATED ) }
					if ( x == NULL ) {
						p = success ?
							p_prune( PRUNE_FILTER, start_p+1 ) :
							p_prune( PRUNE_TERM, start_p+1 );
						if ( *p==':' ) p++;
					}
					exponent = NULL;
					op = BM_END;
					break;
				}
			}
		}
		else break;
	}
	freeItem( i );
#ifdef DEBUG
	if ((data->stack.flags) || (data->stack.exponent)) {
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on exponent\n" );
		exit( -1 );
	}
	freeListItem( &data->stack.flags );
	freeListItem( &data->stack.exponent );
	if ((data->stack.scope) || (data->stack.base)) {
		fprintf( stderr, ">>>>> B%%: Error: xp_verify: memory leak on scope\n" );
		exit( -1 );
	}
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
		j = popListItem( &stack->as_sub );
	}
	freeItem( j );
	freeListItem( mark_exp );
	stack->as_sub = popListItem( &stack->sub );
	*i = popListItem( &stack->i );
	*mark_exp = popListItem( &stack->mark_exp );
	*flags = pop_item( &stack->flags );
	return (char *) popListItem( &stack->p );
}

//===========================================================================
//	verify
//===========================================================================
static int match( CNInstance *, char *, listItem *, BMQueryData * );
static BMTraverseCB
	match_CB, dot_identifier_CB, dereference_CB, dot_expression_CB,
	sub_expression_CB, open_CB, filter_CB, decouple_CB, close_CB, wildcard_CB;

static int
verify( int op, CNInstance *x, char **position, BMQueryData *data )
/*
	invoked by xp_verify on each [sub-]expression, i.e.
	on each expression term starting with '*' or '%'
	Note that *variable is same as %((*,variable),?)
	Assumption: expression is not ternarized
*/
{
#ifdef DEBUG
	fprintf( stderr, "verify: " );
	switch ( op ) {
	case BM_INIT: fprintf( stderr, "BM_INIT success=%d ", data->success ); break;
	case BM_BGN: fprintf( stderr, "BM_BGN success=%d ", data->success ); break;
	case BM_END: fprintf( stderr, "BM_END success=%d ", data->success ); break;
	}
	fprintf( stderr, "'%s'\n", *position );
#endif
	/* we cannot use exponent to track scope, as no exponent is
	   pushed in case of "single" expressions - e.g. %((.,?):%?)
	*/
	listItem *base, *OOS;
	if ( op==BM_END ) {
		base = popListItem( &data->stack.base );
		popListItem( &data->stack.scope ); // done with this one
		OOS = ((data->stack.scope) ? data->stack.scope->ptr : NULL );
	}
	else {
		base = data->stack.exponent;
		OOS = data->stack.flags;
	}
	data->op = op;
	data->instance = x;
	data->base = base;
	data->OOS = OOS;

	BMTraverseData traverse_data;
	memset( &traverse_data, 0, sizeof(traverse_data) );
	traverse_data.user_data = data;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data.table;
	table[ BMMarkRegisterCB ]	= match_CB;
	table[ BMStarCharacterCB ]	= match_CB;
	table[ BMModCharacterCB ]	= match_CB;
	table[ BMCharacterCB ]		= match_CB;
	table[ BMRegexCB ]		= match_CB;
	table[ BMIdentifierCB ]		= match_CB;
	table[ BMDotIdentifierCB ]	= dot_identifier_CB;
	table[ BMDereferenceCB ]	= dereference_CB;
	table[ BMSubExpressionCB ]	= sub_expression_CB;
	table[ BMDotExpressionCB ]	= dot_expression_CB;
	table[ BMOpenCB ]		= open_CB;
	table[ BMFilterCB ]		= filter_CB;
	table[ BMDecoupleCB ]		= decouple_CB;
	table[ BMCloseCB ]		= close_CB;
	table[ BMWildCardCB ]		= wildcard_CB;
	bm_traverse( *position, &traverse_data, &data->stack.flags, data->flags );

	*position = traverse_data.p;
	if (( data->mark_exp )) {
		data->flags = traverse_data.flags;
		listItem **sub_exp = &data->sub_exp;
		for ( listItem *i=data->stack.exponent; i!=base; i=i->next )
			addItem( sub_exp, i->ptr );
		addItem( &data->stack.base, base );
		addItem( &data->stack.scope, data->stack.flags );
	}
#ifdef DEBUG
	if (( data->mark_exp )) fprintf( stderr, "verify: starting SUB, at '%s'\n", *position );
	else fprintf( stderr, "verify: returning %d, at '%s'\n", data->success, *position );
#endif
	return data->success;
}

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
	_return( p )
case_( sub_expression_CB )
	bm_locate_mark( p+1, &data->mark_exp );
	if (( data->mark_exp )) _return( p )
	else _continue( p+1 ) // hand over to open_CB
case_( dot_expression_CB )
	xpn_add( &data->stack.exponent, AS_SUB, 0 );
	switch ( match( data->instance, p, data->base, data ) ) {
	case -1: data->success = 0;
		_continue( p_prune( PRUNE_TERM, p+1 ) )
	case  0: data->success = is_f( NEGATED ) ? 1 : 0;
		p = data->success ?
			p_prune( PRUNE_FILTER, p+1 ) :
			p_prune( PRUNE_TERM, p+1 );
		_continue( p )
	case  1: xpn_set( data->stack.exponent, AS_SUB, 1 ); }
	_post_( open_CB, p+1, DOT )
	_continue( p+2 )
case_( open_CB )
	f_push( &data->stack.flags )
	f_reset( LEVEL|FIRST, 0 )
	if ( !p_single(p) ) {
		f_set( COUPLE )
		xpn_add( &data->stack.exponent, AS_SUB, 0 );
	}
	_continue( p+1 )
case_( filter_CB )
	if ( data->op==BM_BGN && data->stack.flags==data->OOS )
		_return( p )
	else if ( !data->success )
		_continue( p_prune( PRUNE_TERM, p+1 ) )
	else _break
case_( decouple_CB )
	if ( data->stack.flags==data->OOS )
		_return( p )
	else if ( !data->success )
		_continue( p_prune( PRUNE_TERM, p+1 ) )
	else {
		xpn_set( data->stack.exponent, AS_SUB, 1 );
		_break }
case_( close_CB )
	if ( data->stack.flags==data->OOS )
		_return( p )
	if is_f( COUPLE ) popListItem( &data->stack.exponent );
	if is_f( DOT ) popListItem( &data->stack.exponent );
	f_pop( &data->stack.flags, 0 )
	f_set( INFORMED )
	if is_f( NEGATED ) { data->success = !data->success; f_clr( NEGATED ) }
	if ( data->op==BM_END && data->stack.flags==data->OOS && (data->stack.scope))
		_return( p+1 )
	else _continue( p+1 )
case_( wildcard_CB )
	if ( !strncmp( p, "?:", 2 ) ) _continue( p+2 )
	else if is_f( NEGATED ) data->success = 0;
	else if ( !data->stack.exponent || (int)data->stack.exponent->ptr==1 )
		data->success = 1; // wildcard is any or as_sub[1]
	else switch ( match( data->instance, NULL, data->base, data ) ) {
		case -1: // no break
		case  0: data->success = 0; break;
		case  1: data->success = 1; break; }
	_break
BMTraverseCBEnd

//===========================================================================
//	match
//===========================================================================
static int
match( CNInstance *x, char *p, listItem *base, BMQueryData *data )
/*
	tests x.sub[kn]...sub[k0] where exponent=as_sub[k0]...as_sub[kn]
	is either NULL or the exponent of p as expression term
*/
{
	listItem *xpn = NULL;
	for ( listItem *i=data->stack.exponent; i!=base; i=i->next )
		addItem( &xpn, i->ptr );
	CNInstance *y = x;
	while (( y ) && (xpn)) {
		int exp = pop_item( &xpn );
		y = ESUB( y, exp&1 ); }
	if ( y == NULL ) {
		freeListItem( &xpn );
		return -1; }
	else if ( p == NULL ) // wildcard
		return 1;

	// name-value-based testing: note that %! MUST come first
	if ( !strncmp( p, "%!", 2 ) ) {
		listItem *v = bm_context_lookup( data->ctx, "!" );
		return !!lookupItem( v, y ); }
	else if (( data->pivot ) && p==data->pivot->name )
		return ( y==data->pivot->value );
	else if ( *p=='.' )
		return ( y==bm_context_lookup( data->ctx, "." ) );
	else if ( *p=='*' )
		return ( y==data->star );
	else if ( !strncmp( p, "%?", 2 ) )
		return ( y==bm_context_lookup( data->ctx, "?" ) );
	else if ( !is_separator(*p) ) {
		CNInstance *found = bm_context_lookup( data->ctx, p );
		if (( found )) return ( y==found ); }

	// not found in data->ctx
	if ( !y->sub[0] ) {
		char *identifier = db_identifier( y, BMContextDB(data->ctx) );
		char_s q;
		switch ( *p ) {
		case '/': return !strcomp( p, identifier, 2 );
		case '\'': return charscan(p+1,&q) && !strcomp( q.s, identifier, 1 );
		default: return !strcomp( p, identifier, 1 );
		}
	}
	return 0; // not a base entity
}
