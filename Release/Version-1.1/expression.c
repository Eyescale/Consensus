#include <stdio.h>
#include <stdlib.h>

#include "database.h"
#include "expression.h"
#include "string_util.h"
#include "traversal.h"
#include "util.h"

// #define DEBUG

//===========================================================================
//	bm_lookup
//===========================================================================
CNInstance *
bm_lookup( int privy, char *p, BMContext *ctx )
{
	char *term = p_extract( p );
	CNInstance *e = db_lookup( privy, term, ctx->db );
	free( term );
	return e;
}

//===========================================================================
//	bm_verify
//===========================================================================
static int bm_match( CNInstance *x, char *p, listItem *exponent, listItem *base, BMTraverseData * );

int
bm_verify( CNInstance **x, char **position, BMTraverseData *data )
/*
	invoked by xp_verify on each [sub-]expression, i.e.
	on each expression term starting with '*' or '%'
	Note that *variable is same as %((*,variable),?)
*/
{
	char *p = *position;
	listItem *base;
	int scope, OOS;
	int success, not = 0;

	switch ( data->op ) {
	case BM_INIT:
		success = 0;
		base = NULL;
		scope = 1;
		OOS = 0;
		break;
	case BM_BGN:
		/* take x.sub[0].sub[1] if x.sub[0].sub[0]==star
		   Note that star may be null (not instantiated)
		*/
		if ( *p++ == '*' ) {
			CNInstance *y = (*x)->sub[ 0 ];
			if (( y == NULL ) || ( y->sub[ 0 ] == NULL ) ||
			    ( y->sub[ 0 ] != data->star )) {
				 *x = NULL; return 0;
			}
			else *x = y->sub[ 1 ];
		}
		success = 0;
		base = data->stack.exponent;
		scope = (int) data->stack.scope->ptr;
		OOS = scope - 1;
		break;
	case BM_END:;
		success = data->success;
		if ((int) popListItem( &data->stack.neg ))
			success = !success;
		base = popListItem( &data->stack.base );
		scope = (int) data->stack.scope->ptr;
		char *start_p = popListItem( &data->stack.p );
		if ( *p==')' &&  p!=p_prune( PRUNE_DEFAULT, start_p ) )
			scope++; // e.g. %( ... ) or *( ... )
		popListItem( &data->stack.scope );
		OOS = ((data->stack.scope) ? (int)data->stack.scope->ptr : 0 );
		break;
	}
	
	union { int value; void *ptr; } icast;
	listItem **exponent = &data->stack.exponent;
	listItem *mark_exp = NULL;

	int done = 0;
	while ( *p && !(mark_exp) && !done ) {
		// fprintf( stderr, "scanning: '%c'\n", *p );
		switch ( *p ) {
		case '~':
			not = !not; p++;
			break;
		case '*':
		case '%':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				success = bm_match( *x, p, *exponent, base, data );
				if ( success < 0 ) { success = 0; not = 0; }
				else if ( not ) { success = !success; not = 0; }
				p++;
			}
			else if ( bm_match( *x, NULL, *exponent, base, data ) < 0 ) {
				success = not; not = 0;
				p = p_prune( PRUNE_DEFAULT, p+1 );
			}
			else if ( *p == '*' )
				xpn_add( &mark_exp, SUB, 1 );
			else {
				p_locate( p+1, "?", &mark_exp );
				if ( mark_exp == NULL ) p++;
			}
			break;
		case '(':
			scope++;
			icast.value = data->couple;
			addItem( &data->stack.couple, icast.ptr );
			if ( p_single( p ) ) data->couple = 0;
			else {
				data->couple = 1;
				xpn_add( exponent, AS_SUB, 0 );
			}
			icast.value = not;
			addItem( &data->stack.not, icast.ptr );
			not = 0; p++;
			break;
		case ':':
			if (( data->op == BM_BGN ) && ( scope==OOS+1 ))
				{ done = 1; break; }
			if ( success ) { p++; }
			else p = p_prune( PRUNE_DEFAULT, p+1 );
			break;
		case ',':
			if ( scope <= OOS+1 ) { done = 1; break; }
			popListItem( exponent );
			xpn_add( exponent, AS_SUB, 1 );
			if ( success ) { p++; }
			else p = p_prune( PRUNE_DEFAULT, p+1 );
			break;
		case ')':
			scope--;
			if ( scope <= OOS ) { done = 1; break; }
			if ( data->couple ) {
				popListItem( exponent );
			}
			data->couple = (int) popListItem( &data->stack.couple );
			if (( data->op == BM_BGN ) && ( scope==OOS+1 ))
			    { done = 1; break; }
			not = (int) popListItem( &data->stack.not );
			if ( not ) { success = !success; not = 0; }
			p++;
			break;
		case '.':
		case '?':
			if ( not ) { success = 0; not = 0; }
			else if ( data->empty ) success = 0;
#ifdef TRIM
			else if ( wildcard_opt( p, data->btree ) ) success = 1;
#endif
			else success = ( bm_match( *x, NULL, *exponent, base, data ) > 0 );
			p++;
			break;
		default:
			success = bm_match( *x, p, *exponent, base, data );
			if ( success < 0 ) { success = 0; not = 0; }
			else if ( not ) { success = !success; not = 0; }
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		}
	}
	if (( data->mark_exp = mark_exp )) {
		listItem *sub_exp = NULL;
		for ( listItem *i=*exponent; i!=base; i=i->next )
			addItem( &sub_exp, i->ptr );
		data->sub_exp = sub_exp;
		icast.value = scope;
		addItem( &data->stack.scope, icast.ptr );
		icast.value = not;
		addItem( &data->stack.neg, icast.ptr );
		addItem( &data->stack.base, base );
		addItem( &data->stack.p, p );
	}
	*position = p;
#ifdef DEBUG
	if (( *mark_exp )) fprintf( stderr, "bm_verify: starting SUB, at %s\n", p );
	else fprintf( stderr, "bm_verify: returning %d, at %s\n", success, p );
#endif
	return success;
}

static int
bm_match( CNInstance *x, char *p, listItem *exponent, listItem *base, BMTraverseData *data )
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
	if ( *p == '*' ) return ( y == data->star );
	if (( data->pivot ) && ( p == data->pivot->name ))
		return ( y == data->pivot->value );

	return ( y == bm_lookup( data->privy, p, data->ctx ));
}

//===========================================================================
//	bm_substantiate
//===========================================================================
static int bm_void( char *, BMContext * );
static CNInstance *bm_register( char *, BMContext * );

void
bm_substantiate( char *expression, BMContext *ctx )
/*
	Substantiates expression by verifying and/or instantiating all
	relationships involved. Note that, reassignment excepted, only
	new or previously deprecated relationship instances will be
	manifested.
*/
{
	if ( bm_void( expression, ctx ) ) {
#ifdef DEBUG
		fprintf( stderr, "bm_substantiate: %s - void\n", expression );
#endif
		return;
	}
#ifdef DEBUG
	fprintf( stderr, "bm_substantiate: %s {\n", expression );
#endif

	union { int value; void *ptr; } icast;
	struct {
		listItem *results;
		listItem *ndx;
	} stack = { NULL, NULL };
	listItem *sub[ 2 ] = { NULL, NULL }, *instances;

	CNInstance *e;
	CNDB *db = ctx->db;
	int scope=1, ndx=0;
	char *p = expression;
	while ( *p && scope ) {
		if ( p_filtered( p )  ) {
			// bm_void made sure we do have results
			sub[ ndx ] = bm_fetch( p, ctx );
			p = p_prune( PRUNE_DEFAULT, p );
			continue;
		}
		switch ( *p ) {
		case '*':
		case '%':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				e = bm_register( p, ctx );
				sub[ ndx ] = newItem( e );
				p++; break;
			}
			// no break
		case '~':
			// bm_void made sure we do have results
			sub[ ndx ] = bm_fetch( p, ctx );
			p = p_prune( PRUNE_DEFAULT, p );
			break;
		case '(':
			scope++;
			icast.value = ndx;
			addItem( &stack.ndx, icast.ptr );
			if ( ndx ) {
				addItem( &stack.results, sub[ 0 ] );
				ndx = 0; sub[ 0 ] = NULL;
			}
			p++; break;
		case ':': // should not happen
			break;
		case ',':
			if ( scope == 1 ) { scope=0; break; }
			ndx = 1;
			p++; break;
		case ')':
			scope--;
			if ( !scope ) break;
			switch ( ndx ) {
			case 0:
				instances = sub[ 0 ];
				break;
			case 1:
				instances = db_couple( sub, db );
				freeListItem( &sub[ 0 ] );
				freeListItem( &sub[ 1 ] );
				break;
			}
			ndx = (int) popListItem( &stack.ndx );
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '?':
		case '.':
			sub[ ndx ] = newItem( NULL );
			p++; break;
		default:
			e = bm_register( p, ctx );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
#ifdef DEBUG
	if (( sub[ 0 ] )) dbg_out( "bm_substantiate: } first=", sub[0]->ptr, "\n", db );
	else fprintf( stderr, "bm_substantiate: } no result\n" );
#endif
	freeListItem( &sub[ 0 ] );
}

static int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	CNDB *db = ctx->db;
	int scope=1, empty=db_is_empty( db );
	char *p = expression;
	while ( *p && scope ) {
		if ( p_filtered( p ) ) {
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_DEFAULT, p );
			continue;
		}
		switch ( *p ) {
		case '*':
		case '%':
			if ( !p[1] || strmatch( ":,)", p[1] ) )
				{ p++; break; }
			// no break
		case '~':
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_DEFAULT, p );
			break;
		case '(':
			scope++;
			p++; break;
		case ':':
		case ',':
			if ( scope == 1 ) { scope=0; break; }
			p++; break;
		case ')':
			scope--;
			p++; break;
		case '?':
		case '.':
			if ( empty ) return 1;
			p++; break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	return 0;
}

static CNInstance *
bm_register( char *p, BMContext *ctx )
{
	CNInstance *e = bm_lookup( 0, p, ctx );
	if ( e == NULL ) {
		char *term = p_extract( p );
		e = db_register( term, ctx->db ); 
		free( term );
	}
	return e;
}

//===========================================================================
//	bm_fetch
//===========================================================================
static BMTraverseCB fetch_CB;

listItem *
bm_fetch( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_traverse( expression, ctx, fetch_CB, &results );
	return results;
}
static int
fetch_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_release
//===========================================================================
static BMTraverseCB release_CB;

void
bm_release( char *expression, BMContext *ctx )
{
#ifdef DEBUG
fprintf( stderr, "bm_release: %s {\n", expression );
#endif
	bm_traverse( expression, ctx, release_CB, NULL );
#ifdef DEBUG
fprintf( stderr, "bm_release: }\n" );
#endif
}
static int
release_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	db_deprecate( e, ctx->db );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_outputf
//===========================================================================
void
bm_outputf( char *format, char *expression, BMContext *ctx )
{
	int escaped = 0;
	for ( char *p=format; *p; p++ ) {
		switch ( *p ) {
		case '\\':
			switch ( escaped ) {
			case 0:
			case 2:
				escaped++;
				break;
			case 1:
			case 3:
				putchar( *p );
				escaped--;
				break;
			}
			break;
		case '\"':
			switch ( escaped ) {
			case 0 :
				escaped = 2;
				break;
			case 2:
				return;
			case 1:
			case 3:
				putchar( *p );
				escaped--;
				break;
			}
			break;
		case '%':
			p++;
			switch ( *p ) {
			case '_':
				if ( *expression ) {
					bm_output( expression, ctx );
					expression = p_prune( PRUNE_DEFAULT, expression );
				}
			}
			break;
		default:
			switch ( escaped ) {
			case 0:	// should not happen
				return;
			case 2:
				putchar( *p );
				break;
			case 1:
			case 3:
				switch ( *p ) {
				case 't': putchar( '\t' ); break;
				case 'n': putchar( '\n' ); break;
				default: putchar( *p ); break;
				}
				escaped--;
				break;
			}
			break;
		}
	}
}

//===========================================================================
//	bm_output
//===========================================================================
static BMTraverseCB output_CB;
typedef struct {
	int first;
	CNInstance *last;
} OutputData;

void
bm_output( char *expression, BMContext *ctx )
/*
	outputs expression's results
	note that we rely here on bm_traverse to eliminate doublons
*/
{
	OutputData data;
	data.first = 1;
	data.last = NULL;
	bm_traverse( expression, ctx, output_CB, &data );
	if ( data.first )
		cn_out( stdout, data.last, ctx->db );
	else {
		printf( ", " );
		cn_out( stdout, data.last, ctx->db );
		printf( " }" );
	}
}
static int
output_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OutputData *data = user_data;
	if (( data->last )) {
		if ( data->first ) {
			printf( "{ " );
			data->first = 0;
		}
		else printf( ", " );
		cn_out( stdout, data->last, ctx->db );
	}
	data->last = e;
	return BM_CONTINUE;
}

