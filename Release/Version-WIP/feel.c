#include <stdio.h>
#include <stdlib.h>

#include "feel_private.h"
#include "string_util.h"
#include "traverse.h"
#include "locate.h"
#include "feel.h"

// #define DEBUG

#ifdef UNIFIED
#define ESUB(e,ndx) ( e->sub[0] ? e->sub[ndx] : NULL )
#else
#define ESUB(e,ndx) e->sub[ndx]
#endif

//===========================================================================
//	bm_feel
//===========================================================================
static int xp_init( BMScanData *, char *, BMContext * );
static int xp_verify( CNInstance *, BMScanData * );

CNInstance *
bm_feel( char *expression, BMContext *ctx, BMLogType type )
{
	if ( type == BM_CONDITION )
		return bm_scan( expression, ctx, NULL, NULL );

	BMScanData data;
	if ( !xp_init( &data, expression, ctx ) )
		return NULL;

	CNDB *db = data.db;
	int privy = (type==BM_RELEASED) ? 1 : 0;
	data.privy = privy;
	data.empty = db_is_empty( privy, db );
	data.star = db_lookup( privy, "*", db );

	CNInstance *success = NULL;
	listItem *s = NULL;
	for ( CNInstance *e=db_log(1,privy,db,&s); e!=NULL; e=db_log(0,privy,db,&s) ) {
		if ( xp_verify( e, &data ) ) {
			freeListItem( &s );
			success = e;
			break;
		}
	}
	return success;
}
static int
xp_init( BMScanData *data, char *expression, BMContext *ctx )
{
	if ( !expression || !*expression ) return 0;
	memset( data, 0, sizeof(BMScanData));
	data->expression = expression;
	data->ctx = ctx;
	data->db = BMContextDB(ctx);
	return 1;
}

//===========================================================================
//	bm_scan
//===========================================================================
static CNInstance *xp_traverse( BMScanData * );
static int scan_CB( CNInstance *e, BMScanData *data );

CNInstance *
bm_scan( char *expression, BMContext *ctx, BMScanCB user_CB, void *user_data )
/*
	find the first term other than '.' or '?' in expression which is not
	negated. if found then test each entity in term.exponent against
	expression. otherwise test every entity in CNDB against expression.
	if no user callback is provided, returns first entity that passes.
	otherwise, invokes callback for each entity that passes, and returns
	first entity for which user callback returns BM_DONE.
*/
{
#ifdef DEBUG
	fprintf( stderr, "BM_SCAN: %s\n", expression );
#endif
	BMScanData data;
	if ( !xp_init( &data, expression, ctx ) )
		return NULL;

	CNDB *db = data.db;
	data.user_CB = user_CB;
	data.user_data = user_data;
	listItem *exponent = NULL;
	int privy = 0;
	char *p = bm_locate_pivot( expression, &exponent );
	if (( p )) {
		CNInstance *x = bm_lookup( 0, p, ctx );
		if (( x )) {
			data.pivot = newPair( p, x );
			data.exponent = exponent;
		}
		else {
			freeListItem( &exponent );
			return NULL;
		}
		privy = !strncmp( p, "%!", 2 ) ? 2 : 0;
	}
	else freeListItem( &exponent ); // better safe than sorry
	data.privy = privy;
	data.empty = db_is_empty( privy, db );
	data.star = db_lookup( privy, "*", db );

	CNInstance *success = NULL;
	if (( data.pivot )) {
		success = xp_traverse( &data );
		freePair( data.pivot );
		freeListItem( &data.exponent );
	}
	else {
		listItem *s = NULL;
		for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) ) {
			if ( scan_CB( e, &data ) == BM_DONE ) {
				freeListItem( &s );
				success = e;
				break;
			}
		}
	}
#ifdef DEBUG
	fprintf( stderr, "BM_SCAN: success=%d\n", !!success );
#endif
	return success;
}
static int
scan_CB( CNInstance *e, BMScanData *data )
{
	if ( xp_verify( e, data ) ) {
		if ( !data->user_CB ) return BM_DONE;
		return data->user_CB( e, data->ctx, data->user_data );
	}
	return BM_CONTINUE;
}

//===========================================================================
//	xp_traverse
//===========================================================================
static CNInstance *
xp_traverse( BMScanData *data )
/*
	Traverses data->pivot's exponent invoking scan_CB on every match
	returns current match on the callback's BM_DONE, and NULL otherwise
	Assumption: x is not deprecated - therefore neither are its subs
*/
{
	CNDB *db = data->db;
	int privy = data->privy;
	Pair *pivot = data->pivot;
	CNInstance *x;
	listItem *i, *j;
	if ( !strncmp( pivot->name, "%!", 2 ) ) {
		i = pivot->value;
		x = i->ptr;
	}
	else {
		x = pivot->value;
		i = newItem( x );
	}
	listItem *exponent = data->exponent;
	CNInstance *success = NULL;
#ifdef DEBUG
fprintf( stderr, "XP_TRAVERSE: privy=%d, pivot=", privy );
db_output( stderr, "", x, db );
fprintf( stderr, ", exponent=" );
xpn_out( stderr, exponent );
fprintf( stderr, "\n" );
#endif
	int exp;
	listItem *trail = NULL;
	listItem *stack = NULL;
	for ( ; ; ) {
		x = i->ptr;
		if (( exponent )) {
			exp = (int) exponent->ptr;
			if ( exp & 2 ) {
				x = ESUB( x, exp&1 );
				if (( x )) {
					addItem ( &stack, i );
					addItem( &stack, exponent );
					exponent = exponent->next;
					i = newItem( x );
					continue;
				}
			}
		}
		if ( x == NULL )
			; // failed x->sub
		else if ( exponent == NULL ) {
			if (( lookupIfThere( trail, x ) ))
				; // ward off doublons
			else {
				addIfNotThere( &trail, x );
				if ( scan_CB( x, data ) == BM_DONE ) {
					success = x;
					goto RETURN;
				}
			}
		}
		else {
			for ( j=x->as_sub[ exp & 1 ]; j!=NULL; j=j->next )
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
//	xp_verify
//===========================================================================
static int bm_verify( int op, CNInstance *, char **, BMScanData * );
typedef struct {
	listItem *mark_exp;
	listItem *not;
	listItem *sub;
	listItem *as_sub;
	listItem *p;
	listItem *i;
} XPVerifyStack;
static char *pop_stack( XPVerifyStack *, listItem **i, listItem **mark, int *not );

static int
xp_verify( CNInstance *x, BMScanData *data )
{
	CNDB *db = data->db;
	int privy = data->privy;
	char *p = data->expression;
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
		not = 0;
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
			data->mark_exp = NULL;
			data->success = success;
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
				bm_verify( op, x, &p, data );
			}
			if (( data->mark_exp )) {
				listItem **sub_exp = &data->sub_exp;
				while (( x ) && (*sub_exp)) {
					int exp = pop_item( sub_exp );
					x = ESUB( x, exp&1 );
				}
				not = data->flags & NEGATED;
				// backup context
				addItem( &stack.mark_exp, mark_exp );
				add_item( &stack.not, not );
				addItem( &stack.sub, stack.as_sub );
				addItem( &stack.i, i );
				addItem( &stack.p, p );
				// setup new sub context
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
				pop_stack( &stack, &i, &mark_exp, &not );
				if ( not ) success = 0;
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
					char *start_p = pop_stack( &stack, &i, &mark_exp, &not );
					if ( not ) success = 1;
					if ( x == NULL ) {
						p = p_prune( success?PRUNE_FILTER:PRUNE_TERM, start_p+1 );
						if ( *p==':' ) p++;
					}
					exponent = NULL;
					op = BM_END;
					break;
				}
			}
		}
		else goto RETURN;
	}
RETURN:
	freeItem( i );
#ifdef DEBUG
	if ((data->stack.flags) || (data->stack.exponent)) {
		fprintf( stderr, ">>>>> xp_verify: memory leak on exponent\n" );
		if (data->stack.flags) fprintf( stderr, "FLAGS\n" );
		if (data->stack.exponent) fprintf( stderr, "EXPONENT\n" );
		exit( -1 );
	}
	freeListItem( &data->stack.flags );
	freeListItem( &data->stack.exponent );
	if ((data->stack.scope) || (data->stack.base)) {
		fprintf( stderr, ">>>>> xp_verify: memory leak on scope\n" );
		exit( -1 );
	}
	freeListItem( &data->stack.scope );
	freeListItem( &data->stack.base );
#endif
#ifdef DEBUG
fprintf( stderr, "xp_verify:.......} success=%d\n", success );
#endif
	return success;
}

static char *
pop_stack( XPVerifyStack *stack, listItem **i, listItem **mark_exp, int *not )
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
	*not = pop_item( &stack->not );
	return (char *) popListItem( &stack->p );
}

//===========================================================================
//	bm_verify
//===========================================================================
static int bm_match( CNInstance *, char *, listItem *, listItem *, BMScanData * );

static int
bm_verify( int op, CNInstance *x, char **position, BMScanData *data )
/*
	invoked by xp_verify on each [sub-]expression, i.e.
	on each expression term starting with '*' or '%'
	Note that *variable is same as %((*,variable),?)
	Assumption: expression is not ternarized
*/
{
#ifdef DEBUG
	fprintf( stderr, "bm_verify: " );
	switch ( op ) {
	case BM_INIT: fprintf( stderr, "BM_INIT success=%d ", data->success ); break;
	case BM_BGN: fprintf( stderr, "BM_BGN success=%d ", data->success ); break;
	case BM_END: fprintf( stderr, "BM_END success=%d ", data->success ); break;
	}
	fprintf( stderr, "'%s'\n", *position );
#endif
	char *p = *position;
	listItem *base;
	int	level, OOS,
		success = data->success,
		flags = data->flags;
	f_clr( NEGATED )
	switch ( op ) {
	case BM_INIT:
		base = NULL;
		level = 0;
		OOS = 0;
		break;
	case BM_BGN:
		base = data->stack.exponent;
		level = (int) data->stack.scope->ptr;
		OOS = level;
		break;
	case BM_END:;
		base = popListItem( &data->stack.base );
		level = pop_item( &data->stack.scope );
		OOS = ((data->stack.scope) ? (int)data->stack.scope->ptr : 0 );
		break;
	}
	listItem **exponent = &data->stack.exponent;
	listItem *mark_exp = NULL;
	int done = 0;
	while ( *p && !(mark_exp) && !done ) {
		// fprintf( stderr, "scanning: '%s'\n", p );
		switch ( *p ) {
		case '~':
			if is_f( NEGATED ) { f_clr( NEGATED ); }
			else { f_set( NEGATED ); }
			p++; break;
		case '%':
			if ( strmatch( "?!", p[1] ) ) {
				success = bm_match( x, p, *exponent, base, data );
				if ( success < 0 ) { success = 0; f_clr( NEGATED ); }
				else if is_f( NEGATED ) { success = !success; f_clr( NEGATED ); }
				p+=2;
			}
			else if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				success = bm_match( x, p, *exponent, base, data );
				if ( success < 0 ) { success = 0; f_clr( NEGATED ); }
				else if is_f( NEGATED ) { success = !success; f_clr( NEGATED ); }
				p++;
			}
			else {
				bm_locate_mark( p+1, &mark_exp );
				if ( !mark_exp ) p++;
			}
			break;
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				success = bm_match( x, p, *exponent, base, data );
				if ( success < 0 ) { success = 0; f_clr( NEGATED ); }
				else if is_f( NEGATED ) { success = !success; f_clr( NEGATED ); }
				p++;
			}
			else { xpn_add( &mark_exp, SUB, 1 ); }
			break;
		case '(':
			level++;
			f_push( &data->stack.flags );
			f_reset( 0, 0 );
			if ( !p_single(p) ) {
				f_set( COUPLE );
				xpn_add( exponent, AS_SUB, 0 );
			}
			p++; break;
		case ':':
			if ( op==BM_BGN && level==OOS )
				{ done = 1; break; }
			p = success ? p+1 : p_prune( PRUNE_TERM, p+1 );
			break;
		case ',':
			if ( level==OOS ) { done = 1; break; }
			xpn_set( *exponent, AS_SUB, 1 );
			p = success ? p+1 : p_prune( PRUNE_TERM, p+1 );
			break;
		case ')':
			if ( level==OOS ) { done = 1; break; }
			level--;
			if is_f( COUPLE ) popListItem( exponent );
			f_pop( &data->stack.flags, 0 );
			if is_f( NEGATED ) { success = !success; f_clr( NEGATED ); }
			p++;
			if ( op==BM_END && level==OOS && (data->stack.scope))
				{ done = 1; break; }
			break;
		case '?':
			if ( p[1]==':' ) { p+=2; break; }
			// no break
		case '.':
			if is_f( NEGATED ) { success = 0; f_clr( NEGATED ); }
			else if ( data->empty ) success = 0;
			else if (( *exponent == NULL ) || ((int)(*exponent)->ptr == 1 ))
				success = 1; // wildcard is as_sub[1]
			else success = ( bm_match( x, NULL, *exponent, base, data ) > 0 );
			if ( *p++ == '.' ) p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		default:
			success = bm_match( x, p, *exponent, base, data );
			if ( success < 0 ) { success = 0; f_clr( NEGATED ); }
			else if is_f( NEGATED ) { success = !success; f_clr( NEGATED ); }
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		}
	}
	if (( data->mark_exp = mark_exp )) {
		data->flags = flags;
		listItem *sub_exp = NULL;
		for ( listItem *i=*exponent; i!=base; i=i->next )
			addItem( &sub_exp, i->ptr );
		data->sub_exp = sub_exp;
		add_item( &data->stack.scope, level );
		addItem( &data->stack.base, base );
	}
	*position = p;

#ifdef DEBUG
	if (( mark_exp )) fprintf( stderr, "bm_verify: starting SUB, at '%s'\n", p );
	else fprintf( stderr, "bm_verify: returning %d, at '%s'\n", success, p );
#endif
	data->success = success;
	return success;
}

static int
bm_match( CNInstance *x, char *p, listItem *exponent, listItem *base, BMScanData *data )
/*
	tests x.sub[kn]...sub[k0] where exponent=as_sub[k0]...as_sub[kn]
	is either NULL or the exponent of p as expression term
*/
{
	listItem *xpn = NULL;
	for ( listItem *i=exponent; i!=base; i=i->next )
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

	// name-based testing: note that %! MUST come first
	if ( !strncmp( p, "%!", 2 ) ) {
		listItem *v = bm_context_lookup( data->ctx, "!" );
		return !!lookupItem( v, y ); }
	else if (( data->pivot ) && p==data->pivot->name )
		return ( y == data->pivot->value );
	else if ( *p=='*' )
		return ( y == data->star );
	else if ( !strncmp( p, "%?", 2 ) )
		return ( y == bm_context_lookup( data->ctx, "?" ) );
	else if ( !is_separator(*p) ) {
		CNInstance *found = bm_context_lookup( data->ctx, p );
		if (( found )) return ( y == found ); }

	// not found in data->ctx
	if ( !y->sub[0] ) {
		char *identifier = db_identifier( y, data->db );
		char_s q;
		switch ( *p ) {
		case '/': return !strcomp( p, identifier, 2 );
		case '\'': return charscan(p+1,&q) && !strcomp( q.s, identifier, 1 );
		default: return !strcomp( p, identifier, 1 );
		}
	}
	return 0; // not a base entity
}
