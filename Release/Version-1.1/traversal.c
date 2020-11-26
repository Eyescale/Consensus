#include <stdio.h>
#include <stdlib.h>

#include "expression.h"
#include "traversal.h"
#include "util.h"

// #define DEBUG
// #undef TRIM

//===========================================================================
//	bm_feel
//===========================================================================
static char *xp_init( BMTraverseData *, char *, BMContext *, int );
static void xp_release( BMTraverseData * );
static int xp_verify( CNInstance *, BMTraverseData * );

int
bm_feel( char *expression, BMContext *ctx, BMLogType type )
{
	int privy;
	switch ( type ) {
	case BM_CONDITION:
		return bm_traverse( expression, ctx, NULL, NULL );
	case BM_RELEASED:
		privy = 1;
		break;
	case BM_INSTANTIATED:
		privy = 0;
		break;
	}
	BMTraverseData data;
	expression = xp_init( &data, expression, ctx, privy );
	if ( expression == NULL ) return 0;

	int success = 0;
	CNDB *db = ctx->db;
	listItem *s = NULL;
	for ( CNInstance *e=db_log(1,privy,db,&s); e!=NULL; e=db_log(0,privy,db,&s) ) {
		if ( xp_verify( e, &data ) ) {
			if ( !strncmp(data.expression,"?:",2) )
				bm_register( data.ctx, "?", e );
			freeListItem( &s );
			success = 1;
			break;
		}
	}
	xp_release( &data );
	return success;
}

static char *
xp_init( BMTraverseData *data, char *p, BMContext *ctx, int privy )
{
	if (!((p) && (*p)) ) return NULL;
	memset( data, 0, sizeof(BMTraverseData));
	data->ctx = ctx;
	data->privy = privy;
	data->expression = p;
	CNDB *db = ctx->db;
	data->empty = db_is_empty( db );
	data->star = db_lookup( privy, "*", db );
#ifdef TRIM
	// used by wildcard_opt
	data->btree = btreefy( p );
#endif
	return strncmp( p, "?:", 2 ) ? p : p+2;
}

static void
xp_release( BMTraverseData *data )
{
	freePair( data->pivot );
	freeListItem( &data->exponent );
#ifdef TRIM
	freeBTree( data->btree );
#endif
}

//===========================================================================
//	bm_traverse
//===========================================================================
static int xp_traverse( BMTraverseData * );
static int traverse_CB( CNInstance *e, BMTraverseData *data );

int
bm_traverse( char *expression, BMContext *ctx, BMTraverseCB user_CB, void *user_data )
/*
	find the first term other than '.' or '?' in expression which is not
	negated. if found then test each entity in term.exponent against
	expression. otherwise test every entity in CNDB against expression.
	if a user callback is provided, invokes callback for each entity
	that passes. Otherwise returns 1 on first entity that passes.
*/
{
#ifdef DEBUG
	fprintf( stderr, "BM_TRAVERSE: %s\n", expression );
#endif
	BMTraverseData data;
	expression = xp_init( &data, expression, ctx, 0 );
	if (( expression )) {
		char *p = p_locate( expression, &data.exponent );
		if (( p )) {
			CNInstance *x = bm_lookup( 0, p, ctx );
			if (( x )) data.pivot = newPair( p, x );
			else {
				freeListItem( &data.exponent );
				return 0;
			}
		}
		data.user_CB = user_CB;
		data.user_data = user_data;
	}
	else return 0;

	int success = 0;
	if (( data.pivot ))
		success = xp_traverse( &data );
	else {
		CNDB *db = ctx->db;
		listItem *s = NULL;
		for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) ) {
			if ( traverse_CB( e, &data ) == BM_DONE ) {
				freeListItem( &s );
				success = 1;
				break;
			}
		}
	}
	xp_release( &data );
	return success;
}

static int
traverse_CB( CNInstance *e, BMTraverseData *data )
{
	if ( !xp_verify( e, data ) )
		return BM_CONTINUE;
	if ( data->user_CB )
		return data->user_CB( e, data->ctx, data->user_data );
	if ( !strncmp( data->expression, "?:", 2 ) )
		bm_register( data->ctx, "?", e );
	return BM_DONE;
}

//===========================================================================
//	xp_verify
//===========================================================================
typedef struct {
	listItem *mark_exp;
	listItem *not;
	listItem *sub;
	listItem *as_sub;
	listItem *p;
	listItem *i;
} XPVerifyStack;
static char *pop_stack( XPVerifyStack *stack, listItem **i, listItem **mark, int *not );

static int
xp_verify( CNInstance *x, BMTraverseData *data )
{
	CNDB *db = data->ctx->db;
	int privy = data->privy;
	char *p = data->expression;
	if ( !strncmp( p, "?:", 2 ) )
		p += 2;
#ifdef DEBUG
fprintf( stderr, "xp_verify: %s / candidate=", p );
cn_out( stderr, x, db );
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
				x = x->sub[ exp & 1 ];
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
					else { x = NULL; data->success = 0; break; }
				}
				// no break
			default:
				bm_verify( op, x, &p, data );
			}
			if (( data->mark_exp )) {
				// backup context
				addItem( &stack.mark_exp, mark_exp );
				add_item( &stack.not, data->not );
				addItem( &stack.sub, stack.as_sub );
				addItem( &stack.i, i );
				addItem( &stack.p, p );
				// setup new sub context
				mark_exp = data->mark_exp;
				stack.as_sub = NULL;
				exponent = mark_exp;
				listItem **sub_exp = &data->sub_exp;
				while (( *sub_exp )) {
					int exp = (int) popListItem( sub_exp );
					x = x->sub[ exp & 1 ];
				}
				i = newItem( x );
				success = 0;
				op = BM_BGN;
				continue;
			}
			else success = data->success;
		}
		else success = 0;

		if (( mark_exp )) {
			if ( success ) {
				// move on past sub-expression
				pop_stack( &stack, &i, &mark_exp, &not );
				if ( not ) success = !success;
				exponent = NULL;
				op = BM_END;
				continue;
			}
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					if ( !db_private( privy, i->ptr, db ) ) {
						p = stack.p->ptr;
						success = 0;
						op = BM_BGN;
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
					if ( not ) success = !success;
					if ( x == NULL ) {
						if ( success ) {
							p = p_prune( PRUNE_FILTER, start_p );
							if ( *p == ':' ) p++;
						}
						else p = p_prune( PRUNE_TERM, start_p );
					}
					exponent = NULL;
					op = BM_END;
					break;
				}
			}
		}
		else { freeItem( i ); goto RETURN; }
	}

RETURN:
#ifdef DEBUG
	if ((data->stack.exponent) || (data->stack.couple) || (data->stack.not))
		fprintf( stderr, "xp_verify: memory leak on exponent\n" );
	if ((data->stack.scope) || (data->stack.base))
		fprintf( stderr, "xp_verify: memory leak on scope\n" );
	freeListItem( &data->stack.exponent );
	freeListItem( &data->stack.couple );
	freeListItem( &data->stack.scope );
	freeListItem( &data->stack.base );
	freeListItem( &data->stack.not );
fprintf( stderr, "xp_verify:.......} success=%d\n", success );
#endif
	return success;
}

static char *
pop_stack( XPVerifyStack *stack, listItem **item, listItem **mark_exp, int *not )
{
	listItem *i = *item;
	while (( stack->as_sub )) {
		listItem *xpn = popListItem( &stack->as_sub );
		int exp = (int) xpn->ptr;
		if (!( exp & 2 )) freeItem( i );
		i = popListItem( &stack->as_sub );
	}
	freeItem( i );
	stack->as_sub = popListItem( &stack->sub );
	*item = popListItem( &stack->i );
	freeListItem( mark_exp );
	*mark_exp = popListItem( &stack->mark_exp );
	*not = (int) popListItem( &stack->not );
	return (char *) popListItem( &stack->p );
}

//===========================================================================
//	xp_traverse
//===========================================================================
typedef struct {
	int active;
	listItem *half;
	listItem *next;
	struct {
		listItem *i;
		listItem *stack;
	} restore;
} TrimData;
static int trim_bgn( TrimData *, listItem **, listItem *, listItem ** );
static int trimmed( TrimData *, listItem **, listItem **, listItem ** );
static int trim_end( TrimData *, listItem **, listItem **, listItem ** );

int
xp_traverse( BMTraverseData *data )
/*
	Traverses data->pivot's exponent invoking traverse_CB on every match
	returns 1 on the callback's BM_DONE, and 0 otherwise
	Assumption: x is not deprecated - therefore neither are its subs
*/
{
	CNDB *db = data->ctx->db;
	int privy = data->privy;
	CNInstance *x = data->pivot->value;
	listItem *exponent = data->exponent;

#ifdef DEBUG
fprintf( stderr, "XP_TRAVERSE: privy=%d, pivot=", privy );
cn_out( stderr, x, db );
fprintf( stderr, ", exponent=" );
xpn_out( stderr, exponent );
fprintf( stderr, "\n" );
#endif
	if ( x == NULL ) return 0;

	int exp, success = 0;
	TrimData rd;
	rd.active = 0;
	listItem *trail = NULL;

	listItem *stack = NULL, *i = newItem( x ), *j;
	for ( ; ; ) {
		x = i->ptr;
		if (( exponent )) {
			exp = (int) exponent->ptr;
			if ( exp & 2 ) {
				CNInstance *sub = x->sub[ exp & 1 ];
				if (( sub )) {
					addItem ( &stack, i );
					addItem( &stack, exponent );
					exponent = exponent->next;
					i = newItem( sub );
					continue;
				}
				else x = NULL;
			}
		}
		if ( x == NULL )
			; // failed x->sub
		else if ( exponent == NULL ) {
#ifdef TRIM
			if ( trimmed( &rd, &exponent, &i, &stack ) )
				continue;
			else
#endif
			if (( lookupIfThere( trail, x )))
				; // ward off doublons
			else {
				addIfNotThere( &trail, x );
				if ( traverse_CB( x, data ) == BM_DONE ) {
					success = 1;
					goto RETURN;
				}
			}
		}
#ifdef TRIM
		else if ( trim_bgn( &rd, &exponent, i, &stack ) )
			continue;
#endif
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
#ifdef TRIM
			else if ( trim_end( &rd, &exponent, &i, &stack ) ) {
#else
			else {
#endif
				goto RETURN;
			}
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
	freeItem( i );
	return success;
}
static int reduceable( listItem *exponent, listItem **half, listItem **next );
static int
trim_bgn( TrimData *rd, listItem **exponent, listItem *i, listItem **stack )
/*
	Allows both better performances and to eliminate doublons, by
	bypassing as_sub[k0]...as_sub[kn].sub[kn]...sub[k0] in exponent.
	If found, sets rd->next to the subsequent exponent term, and
	allocates rd->half to hold the series of as_sub's - which
	should be tested, but only once for all, by the caller.
	Assumption: exponent is not NULL and first term is AS_SUB
*/
{
	if ( rd->active || !reduceable( *exponent, &rd->half, &rd->next ) )
		return 0;
#ifdef DEBUG
fprintf( stderr, "TRIMMING: exponent=" );
xpn_out( stderr, *exponent );
fprintf( stderr, " -> [ half:" );
xpn_out( stderr, rd->half );
fprintf( stderr, ", next:" );
xpn_out( stderr, rd->next );
fprintf( stderr, " ]\n" );
#endif
	rd->active = 1;
	rd->restore.i = i;
	rd->restore.stack = *stack;
	*exponent = rd->half;
	*stack = NULL;
	return 1;
}
static int
reduceable( listItem *exponent, listItem **half, listItem **next )
{
	// register the full series of consecutive as_sub's, if there are any
	listItem *stack = NULL;
	do {
		int exp = (int) exponent->ptr;
		if ( exp & 2 ) break;
		else {
			union { int value; void *ptr; } icast;
			icast.value = exp;
			addItem( &stack, icast.ptr );
			exponent = exponent->next;
		}
	} while (( exponent ));

	// see if these are followed (in reverse order) by matching sub's
	for ( listItem *i=stack; i!=NULL; i=i->next ) {
		if ( exponent == NULL ) {
			freeListItem( &stack );
			return 0;
		}
		int xpn = (int) i->ptr;
		int exp = (int) exponent->ptr;
		if ((!exp&2) || ((exp&1)!=(xpn&1)) ) {
			freeListItem( &stack );
			return 0;
		}
		exponent = exponent->next;
	}
	reorderListItem( &stack );
	*next = exponent;
	*half = stack;
	return 1;
}
static int
trimmed( TrimData *rd, listItem **exponent, listItem **i, listItem **stack )
/*
	x.half (x=i->ptr) successfully trimmed to x => restore context
*/
{
	if ( rd->active ) {
#ifdef DEBUG
fprintf( stderr, "trimmed: success\n" );
#endif
		rd->active = 0;
		*exponent = rd->next;
		freeListItem( &rd->half );
		*stack = rd->restore.stack;
		*i = rd->restore.i;
		return 1;
	}
	return 0;
}
static int
trim_end( TrimData *rd, listItem **exponent, listItem **i, listItem **stack )
/*
	x.half (x=i->ptr) failed => try i->next
*/
{
	if ( rd->active ) {
		*i = rd->restore.i;
		listItem *next_i = (*i)->next;
		if (( next_i )) {
			rd->restore.i = next_i;
			*exponent = rd->half;
		}
		else {
			rd->active = 0;
			freeListItem( &rd->half );
			*stack = rd->restore.stack;
		}
		return 0;
	}
	return 1;
}
