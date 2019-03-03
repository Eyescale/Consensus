#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG
// #undef TRIM

static int xp_traverse( int privy, CNInstance *, listItem *xpn, CNDB *, DBTraverseCB, void * );

//===========================================================================
//	db_traverse
//===========================================================================
typedef struct {
	char *expression;
	DBTraverseCB *user_CB;
	void *user_data;
} DBTraverseData;
static DBTraverseCB verify_CB;

int
db_traverse( char *expression, CNDB *db, DBTraverseCB user_CB, void *user_data )
/*
	find the first term other than '.' or '?' in expression which is not
	negated. if found then test each entity in term.exponent against
	expression. otherwise test every entity in CNDB against expression.
	if a user callback is provided, invokes callback for each entity
	that passes. Otherwise returns 1 on first entity that passes.
*/
{
#ifdef DEBUG
	fprintf( stderr, "DB_TRAVERSE: %s\n", expression );
#endif
	DBTraverseData data;
	listItem *exponent = NULL;
	data.expression = expression;
	data.user_CB = user_CB;
	data.user_data = user_data;
	char *pivot = p_locate( expression, "", &exponent );
	if (( pivot )) {
		CNInstance *x = p_lookup( 0, pivot, db );
		return xp_traverse( 0, x, exponent, db, verify_CB, &data );
	}
	else {
		listItem *s = NULL;
		for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) ) {
			if ( verify_CB( e, db, &data ) == DB_DONE ) {
				freeListItem( &s );
				return 1;
			}
		}
		return 0;
	}
}
static int
verify_CB( CNInstance *e, CNDB *db, void *user_data )
{
	DBTraverseData *data = user_data;
	if ( db_verify( 0, e, data->expression, db ) ) {
		return data->user_CB ?
			data->user_CB( e, db, data->user_data ) :
			DB_DONE;
	}
	return DB_CONTINUE;
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
xp_traverse( int privy, CNInstance *x, listItem *exponent, CNDB *db, DBTraverseCB user_CB, void *user_data )
/*
	Traverses x.exponent in db invoking callback on every match
	If user_CB is NULL, then xp_traverse returns 1 on first match, and 0 otherwise
	Otherwise xp_traverse returns 1 on the callback's DB_DONE, and 0 otherwise
	Assumption: x is not deprecated - therefore neither are its subs
*/
{
#ifdef DEBUG
fprintf( stderr, "XP_TRAVERSE: privy=%d, pivot=", privy );
cn_out( stderr, x, db );
fprintf( stderr, ", exponent=" );
output_exponent( stderr, exponent );
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
			else if (( user_CB )) {
				addIfNotThere( &trail, x );
				if ( user_CB( x, db, user_data ) == DB_DONE ) {
					success = 1;
					goto RETURN;
				}
			}
			else {
				success = 1;
				goto RETURN;
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
				freeItem( i );
				return 0;
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
output_exponent( stderr, *exponent );
fprintf( stderr, " -> [ half:" );
output_exponent( stderr, rd->half );
fprintf( stderr, ", next:" );
output_exponent( stderr, rd->next );
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

//===========================================================================
//	xp_verify
//===========================================================================
int
xp_verify( int privy, CNInstance *x, char *expression, CNDB *db, VerifyData *data )
{
#ifdef DEBUG
	fprintf( stderr, "XP_VERIFY: %s / ", expression );
	dbg_out( "candidate=", x, "\n", db );
#endif
	union { int value; void *ptr; } icast;
	struct {
		listItem *mark_exp;
		listItem *sub;
		listItem *as_sub;
		listItem *p;
		listItem *i;
	} stack = { NULL, NULL, NULL, NULL, NULL };
	listItem *exponent = NULL,
		*mark_exp,
		*sub_pos = NULL,
		*i = newItem( x ),
		*j;
	int op = SUB_NONE;
	char *p = expression;
	int success = 0;
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
				else { x = NULL; success = 0; }
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
				else success = 0;
			}
		}
		if (( x )) {
			// backup context, initial
			addItem( &stack.mark_exp, mark_exp );
			mark_exp = NULL;
			success = db_verify_sub( op, success, &x, &p, &mark_exp, &sub_pos, data );
			if (( mark_exp )) {
				// backup context, final
				addItem( &stack.sub, stack.as_sub );
				addItem( &stack.i, i );
				addItem( &stack.p, p );
				// setup new sub context
				stack.as_sub = NULL;
				exponent = mark_exp;
				while (( sub_pos )) {
					int exp = (int) popListItem( &sub_pos );
					x = x->sub[ exp & 1 ];
				}
				i = newItem( x );
				op = SUB_START;
				continue;
			}
			else mark_exp = popListItem( &stack.mark_exp );
		}
		if (( mark_exp )) {
			if ( success ) {
				// move on past sub-expression
				while (( stack.as_sub )) {
					exponent = popListItem( &stack.as_sub );
					int exp = (int) exponent->ptr;
					if (!( exp & 2 )) freeItem( i );
					i = popListItem( &stack.as_sub );
				}
				exponent = NULL;
				freeItem( i );
				freeListItem( &mark_exp );
				mark_exp = popListItem( &stack.mark_exp );
				stack.as_sub = popListItem( &stack.sub );
				i = popListItem( &stack.i );
				popListItem( &stack.p );
				// p is already at sub-expression closure
				op = SUB_FINISH;
				continue;
			}
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					if ( !db_private( privy, i->ptr, db ) ) {
						p = stack.p->ptr;
						op = SUB_START;
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
					exponent = NULL;
					freeItem( i );
					freeListItem( &mark_exp );
					mark_exp = popListItem( &stack.mark_exp );
					stack.as_sub = popListItem( &stack.sub );
					i = popListItem( &stack.i );
					// p must be at sub-expression closure
					if ( x == NULL ) p = p_prune( PRUNE_DEFAULT, stack.p->ptr );
					popListItem( &stack.p );
					op = SUB_FINISH;
					break;
				}
			}
		}
		else { freeItem( i ); return success; }
	}
}

//===========================================================================
//	xp_match
//===========================================================================
int
xp_match( int privy, CNInstance *x, char *p, CNInstance *star,
	listItem *exponent, listItem *base, CNDB *db )
/*
	tests x.sub[kn]...sub[k0] where exponent=as_sub[k0]...as_sub[kn]
	is either NULL or the exponent of p as expression term
*/
{
	listItem *xpn = NULL;
	for ( listItem *i = exponent; i!=base; i=i->next ) {
		addItem( &xpn, i->ptr );
	}
	CNInstance *y = x;
	while (( y ) && (xpn)) {
		int exp = (int) popListItem( &xpn );
		y = y->sub[ exp & 1 ];
	}
	if ( y == NULL ) return -1;
	if ( p == NULL ) return 1;
	if ( *p == '*' ) return ( y == star );
	return ( y == p_lookup( privy, p, db ));
}

//===========================================================================
//	db_is_empty
//===========================================================================
int
db_is_empty( CNDB *db )
{
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		if ( !db_private( 0, entry->value, db ) )
			return 0;
	}
	return 1;
}

//===========================================================================
//	db_first, db_next
//===========================================================================
CNInstance *
db_first( CNDB *db, listItem **stack )
{
	for ( listItem *i=db->index->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		CNInstance *e = entry->value;
		if ( !db_private( 0, e, db ) ) {
			addItem( stack, i );
			return e;
		}
	}
	return NULL;
}
CNInstance *
db_next( CNDB *db, CNInstance *e, listItem **stack )
{
	if ( e == NULL ) return NULL;
	if (( e->as_sub[ 0 ] )) {
		for ( listItem *i=e->as_sub[0]; i!=NULL; i=i->next ) {
			e = i->ptr;
			if ( e == NULL ) {
				fprintf( stderr, "BINGO!!!!\n" );
				exit( 0 );
			}
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e;
			}
		}
	}
	listItem *i = popListItem( stack );
	for ( ; ; ) {
		if (( i->next )) {
			i = i->next;
			if (( *stack ))
				e = i->ptr;
			else {
				// e in db->index
				Pair *entry = i->ptr;
				e = entry->value;
			}
			if ( !db_private( 0, e, db ) ) {
				addItem( stack, i );
				return e;
			}
		}
		else if (( *stack )) {
			i = popListItem( stack );
		}
		else return NULL;
	}
}

//===========================================================================
//	xp_compare	- unused
//===========================================================================
static listItem *xp_div( listItem *exp1, listItem *exp2 );
static DBTraverseCB compare_CB;
typedef struct {
	int cmp;
	CNInstance *e;
} CompareData;
int
xp_compare( int privy, CNInstance *x, listItem *exp1, CNInstance *e, listItem *exp2, CNDB *db )
/*
	tests e.exp2 in x.exp1 - for which we test e in x.exp1.inv(exp2)
	note: x null means 'any', in which case xp_compare simply verifies
	that e.exp2.inv(exp1) exists.
*/
{
	CompareData data;
	data.cmp = 1;
	if (( x )) {
		data.e = e;
		if (( exp2 )){
			listItem *exponent = xp_div( exp1, exp2 );
			xp_traverse( privy, x, exponent, db, compare_CB, &data );
			freeListItem( &exponent );
		}
		else xp_traverse( privy, x, exp1, db, compare_CB, &data );
	}
	else {
		// when callback is null, xp_traverse returns 1 on first match
		if (( exp1 )){
			listItem *exponent = xp_div( exp2, exp1 );
			data.cmp = xp_traverse( privy, e, exponent, db, NULL, NULL );
			freeListItem( &exponent );
		}
		else data.cmp = xp_traverse( privy, e, exp2, db, NULL, NULL );
	}
	return data.cmp;
}
static int
compare_CB( CNInstance *e, CNDB *db, void *user_data )
{
	CompareData *data = user_data;
	if ( e == data->e ) {
		data->cmp = 0;
		return DB_DONE;
	}
	return DB_CONTINUE;
}
static listItem * xp_inv( listItem *exponent );
static listItem *
xp_div( listItem *exp1, listItem *exp2 )
{
	listItem *result = NULL;
	listItem *inverse = xp_inv( exp2 );
	for ( listItem *i=exp1; i!=NULL; i=i->next )
		addItem( &result, i->ptr );
	for ( listItem *i=inverse; i!=NULL; i=i->next )
		addItem( &result, i->ptr );
	reorderListItem( &result );
	freeListItem( &inverse );
	return result;
}
static listItem *
xp_inv( listItem *exponent )
{
	union { int value; void *ptr; } icast;

	listItem *inverse = NULL;
	for ( listItem *i=exponent; i!=NULL; i=i->next ) {
		int exp = (int) i->ptr;
		int xpn = ((exp&2)? 0 : 2 ) + (exp&1);
		icast.value = xpn;
		addItem( &inverse, icast.ptr );
	}
	return inverse;
}

//===========================================================================
//	utilities
//===========================================================================
void
push_exponent( int as_sub, int position, listItem **stack )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( stack, icast.ptr );
}
void
output_exponent( FILE *stream, listItem *exponent )
{
	while ( exponent ) {
		fprintf( stream, "%d", (int)exponent->ptr );
		exponent=exponent->next;
	}
}
