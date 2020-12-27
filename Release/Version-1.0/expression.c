#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG

//===========================================================================
//	db_verify
//===========================================================================
// #define DEBUG

int
db_verify( int privy, CNInstance *x, char *expression, CNDB *db )
{
#ifdef DEBUG
fprintf( stderr, "db_verify: %s / candidate=", expression );
cn_out( stderr, x, db );
fprintf( stderr, " ........{\n" );
#endif
	VerifyData data;
	data.stack.exponent = NULL;
	data.stack.couple = NULL;
	data.stack.scope = NULL;
	data.stack.base = NULL;
	data.stack.not = NULL;
	data.stack.neg = NULL;
	data.stack.p = NULL;
#ifdef TRIM
	// used by wildcard_opt
	data.btree = btreefy( expression );
#endif
	data.db = db;
	data.privy = privy;
	data.empty = db_is_empty( db );
	data.star = p_lookup( privy, "*", db );
	data.couple = 0;

	int success = xp_verify( privy, x, expression, db, &data );
#ifdef TRIM
	freeBTree( data.btree );
#endif
#ifdef DEBUG
fprintf( stderr, "db_verify:.......} success=%d\n", success );
#endif
	return success;
}

//===========================================================================
//	db_verify_sub
//===========================================================================
typedef struct {
	listItem *base;
	int scope, OOS;
	char *p;
} VerifyParams;
static int init_params( VerifyParams *, int, int *, CNInstance **, char *, VerifyData *);

int
db_verify_sub( int op, int success,
	CNInstance **x, char **position,
	listItem **mark_exp, listItem **mark_pos,
	VerifyData *data )
/*
	invoked by xp_verify on each [sub-]expression, i.e.
	on each expression term starting with '*' or '%'
	Note that *variable is same as %((*,variable),?)
*/
{
	VerifyParams params;
	if ( !init_params( &params, op, &success, x, *position, data ) )
		{ *x = NULL; return 0; }
#ifdef DEBUG
	fprintf( stderr, "db_verify_sub: " );
	switch ( op ) {
		case SUB_NONE: fprintf( stderr, "INIT %s / ", *position ); break;
		case SUB_START: fprintf( stderr, "START %s / ", *position + 1 ); break;
		case SUB_FINISH: fprintf( stderr, "FINISH %s / ", *position ); break;
	}
	fprintf( stderr, "success=%d", success );
	dbg_out( ", candidate=", *x, NULL, data->db );
	fprintf( stderr, ", exp=" );
	output_exponent( stderr, data->stack.exponent );
	fprintf( stderr, "\n" );
#endif
	listItem *base = params.base;
	int scope = params.scope;
	int OOS = params.OOS;
	char *p = params.p;
	int not = 0;

	union { int value; void *ptr; } icast;
	int privy = data->privy;
	CNDB *db = data->db;
	CNInstance *star = data->star;
	listItem **exponent = &data->stack.exponent;

	int out = 0;
	while ( *p && !(*mark_exp) && !out ) {
		// fprintf( stderr, "scanning: '%c'\n", *p );
		switch ( *p ) {
		case '~':
			not = !not; p++;
			break;
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				success = xp_match( privy, *x, p, star, *exponent, base, db );
				if ( success < 0 ) { success = 0; not = 0; }
				else if ( not ) { success = !success; not = 0; }
				p++; break;
			}
			// no break
		case '%':
			if ( xp_match( privy, *x, NULL, star, *exponent, base, db ) < 0 ) {
				success = not; not = 0;
				p = p_prune( PRUNE_TERM, p+1 );
			}
			else if ( *p == '*' )
				push_exponent( SUB, 1, mark_exp );
			else {
				p_locate( p+1, "?", mark_exp );
				if ( *mark_exp == NULL ) p++;
			}
			break;
		case '(':
			scope++;
			icast.value = data->couple;
			addItem( &data->stack.couple, icast.ptr );
			if ( p_single( p ) ) data->couple = 0;
			else {
				data->couple = 1;
				push_exponent( AS_SUB, 0, exponent );
			}
			icast.value = not;
			addItem( &data->stack.not, icast.ptr );
			not = 0; p++;
			break;
		case ':':
			if (( op == SUB_START ) && ( scope==OOS+1 ))
				{ out = 1; break; }
			if ( success ) { p++; }
			else p = p_prune( PRUNE_TERM, p+1 );
			break;
		case ',':
			if ( scope <= OOS+1 ) { out = 1; break; }
			popListItem( exponent );
			push_exponent( AS_SUB, 1, exponent );
			if ( success ) { p++; }
			else p = p_prune( PRUNE_TERM, p+1 );
			break;
		case ')':
			scope--;
			if ( scope <= OOS ) { out = 1; break; }
			if ( data->couple ) {
				popListItem( exponent );
			}
			data->couple = (int) popListItem( &data->stack.couple );
			if (( op == SUB_START ) && ( scope==OOS+1 ))
			    { out = 1; break; }
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
			else success = ( xp_match( privy, *x, NULL, star, *exponent, base, db ) > 0 );
			p++;
			break;
		default:
			success = xp_match( privy, *x, p, star, *exponent, base, db );
			if ( success < 0 ) { success = 0; not = 0; }
			else if ( not ) { success = !success; not = 0; }
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		}
	}
	if (( *mark_exp )) {
		for ( listItem *i=*exponent; i!=base; i=i->next )
			addItem( mark_pos, i->ptr );
		icast.value = scope;
		addItem( &data->stack.scope, icast.ptr );
		icast.value = not;
		addItem( &data->stack.neg, icast.ptr );
		addItem( &data->stack.base, base );
		addItem( &data->stack.p, p );
	}
	*position = p;
#ifdef DEBUG
	if (( *mark_exp )) fprintf( stderr, "db_verify_sub: starting SUB, at %s\n", p );
	else fprintf( stderr, "db_verify_sub: returning %d, at %s\n", success, p );
#endif
	return success;
}

static int
init_params( VerifyParams *params, int op, int *success, CNInstance **x, char *p, VerifyData *data )
{
	switch ( op ) {
	case SUB_NONE:
		*success = 0;
		params->p = p;
		params->base = NULL;
		params->scope = 1;
		params->OOS = 0;
		break;
	case SUB_START:
		// take x.sub[0].sub[1] if x.sub[0].sub[0]==star
		// Note that star may be null (not instantiated)
		if ( *p++ == '*' ) {
			CNInstance *y = (*x)->sub[ 0 ];
			if ( y == NULL ) return 0;
			if ( y->sub[ 0 ] == NULL ) return 0;
			if ( y->sub[ 0 ] != data->star ) return 0;
			*x = y->sub[ 1 ];
		}
		*success = 0;
		params->p = p;
		params->base = data->stack.exponent;
		params->scope = (int) data->stack.scope->ptr;
		params->OOS = params->scope - 1;
		break;
	case SUB_FINISH:;
		if ((int) popListItem( &data->stack.neg ))
			*success = !*success;
		params->p = p;
		params->base = popListItem( &data->stack.base );
		params->scope = (int) data->stack.scope->ptr;
		char *start_p = popListItem( &data->stack.p );
		if ( *p==')' &&  p!=p_prune( PRUNE_TERM, start_p ) )
			params->scope++; // e.g. %( ... ) or *( ... )
		popListItem( &data->stack.scope );
		params->OOS = ((data->stack.scope) ? (int)data->stack.scope->ptr : 0 );
	}
	return 1;
}
	
//===========================================================================
//	db_substantiate
//===========================================================================
static int db_void( char *, CNDB *db );
static CNInstance *p_register( char *p, CNDB *db );

void
db_substantiate( char *expression, CNDB *db )
/*
	Substantiates expression by verifying and/or instantiating all
	relationships involved. Note that, reassignment excepted, only
	new or previously deprecated relationship instances will be
	manifested.
*/
{
	if ( db_void( expression, db ) ) {
#ifdef DEBUG
		fprintf( stderr, "db_substantiate: %s - void\n", expression );
#endif
		return;
	}
#ifdef DEBUG
	fprintf( stderr, "db_substantiate: %s {\n", expression );
#endif

	union { int value; void *ptr; } icast;
	struct {
		listItem *results;
		listItem *ndx;
	} stack = { NULL, NULL };
	listItem *sub[ 2 ] = { NULL, NULL }, *instances;

	CNInstance *e;
	int scope=1, ndx=0;
	char *p = expression;
	while ( *p && scope ) {
		if ( p_filtered( p )  ) {
			// db_void made sure we do have results
			sub[ ndx ] = db_fetch( p, db );
			p = p_prune( PRUNE_TERM, p );
			continue;
		}
		switch ( *p ) {
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				e = p_register( p, db );
				sub[ ndx ] = newItem( e );
				p++; break;
			}
			// no break
		case '~':
		case '%':
			// db_void made sure we do have results
			sub[ ndx ] = db_fetch( p, db );
			p = p_prune( PRUNE_TERM, p );
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
			case 0: instances = sub[ 0 ]; break;
			case 1: instances = db_couple( sub, db );
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
			e = p_register( p, db );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
#ifdef DEBUG
	if (( sub[ 0 ] )) dbg_out( "db_substantiate: } first=", sub[0]->ptr, "\n", db );
	else fprintf( stderr, "db_substantiate: } no result\n" );
#endif
	freeListItem( &sub[ 0 ] );
}

static int
db_void( char *expression, CNDB *db )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is, 1 otherwise
*/
{
	// fprintf( stderr, "db_void: %s\n", expression );
	int scope=1, empty=db_is_empty( db );
	char *p = expression;
	while ( *p && scope ) {
		if ( p_filtered( p ) ) {
			if ( empty || !db_feel( p, DB_CONDITION, db ) )
				return 1;
			p = p_prune( PRUNE_TERM, p );
			continue;
		}
		switch ( *p ) {
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) )
				{ p++; break; }
			// no break
		case '~':
		case '%':
			if ( empty || !db_feel( p, DB_CONDITION, db ) )
				return 1;
			p = p_prune( PRUNE_TERM, p );
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

//===========================================================================
//	db_fetch
//===========================================================================
static DBTraverseCB fetch_CB;

listItem *
db_fetch( char *expression, CNDB *db )
{
	listItem *results = NULL;
	db_traverse( expression, db, fetch_CB, &results );
	return results;
}
static int
fetch_CB( CNInstance *e, CNDB *db, void *results )
{
	addIfNotThere((listItem **) results, e );
	return DB_CONTINUE;
}

//===========================================================================
//	db_release
//===========================================================================
static DBTraverseCB release_CB;

void
db_release( char *expression, CNDB *db )
{
#ifdef DEBUG
fprintf( stderr, "db_release: %s {\n", expression );
#endif
	db_traverse( expression, db, release_CB, NULL );
#ifdef DEBUG
fprintf( stderr, "db_release: }\n" );
#endif
}
static int
release_CB( CNInstance *e, CNDB *db, void *user_data )
{
	db_deprecate( e, db );
	return DB_CONTINUE;
}

//===========================================================================
//	db_outputf
//===========================================================================
void
db_outputf( char *format, char *expression, CNDB *db )
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
			case 's':
				if ( *expression ) {
					db_output( expression, db );
					expression = p_prune( PRUNE_TERM, expression );
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
//	db_output
//===========================================================================
static DBTraverseCB output_CB;
typedef struct {
	int first;
	CNInstance *last;
} OutputData;

void
db_output( char *expression, CNDB *db )
/*
	outputs expression's results
	note that we rely here on db_traverse to eliminate doublons
*/
{
	OutputData data;
	data.first = 1;
	data.last = NULL;
	db_traverse( expression, db, output_CB, &data );
	if ( data.first )
		cn_out( stdout, data.last, db );
	else {
		printf( ", " );
		cn_out( stdout, data.last, db );
		printf( " }" );
	}
}
static int
output_CB( CNInstance *e, CNDB *db, void *user_data )
{
	OutputData *data = user_data;
	if (( data->last )) {
		if ( data->first ) {
			printf( "{ " );
			data->first = 0;
		}
		else printf( ", " );
		cn_out( stdout, data->last, db );
	}
	data->last = e;
	return DB_CONTINUE;
}

//===========================================================================
//	p_lookup, p_register
//===========================================================================
static char *p_extract( char *p );

CNInstance *
p_lookup( int privy, char *p, CNDB *db )
{
	char *term = p_extract( p );
	CNInstance *e = db_lookup( privy, term, db );
	free( term );
	return e;
}
static CNInstance *
p_register( char *p, CNDB *db )
{
	char *term = p_extract( p );
	CNInstance *e = db_lookup( 0, term, db );
	if ( e == NULL ) e = db_register( term, db ); 
	free( term );
	return e;
}
static char *
p_extract( char *p )
{
	CNString *s = newString();
	if ( *p != '*' ) {
		for ( char *q=p; *q; q++ ) {
			if ( is_separator( *q ) )
				break;
			StringAppend( s, *q );
		}
	}
	else StringAppend( s, '*' );
	char *term = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return term;
}

//===========================================================================
//	p_locate
//===========================================================================
static char *locate_mark( char *expression, listItem **exponent );

char *
p_locate( char *expression, char *fmt, listItem **exponent )
/*
	if fmt is "?", returns first '?' found in expression, with
	corresponding exponent (in reverse order). otherwise returns
	first term which is not a wildcard and is not negated.
*/
{
	if ( !strcmp( fmt, "?" ) ) return locate_mark( expression, exponent );

	union { int value; void *ptr; } icast;
	struct {
		listItem *not;
		listItem *tuple;
		listItem *level;
		listItem *premark;
	} stack = { NULL, NULL, NULL, NULL };

	listItem *level = NULL,
		*mark_exp = NULL,
		*star_exp = NULL;

	int	scope = 1,
		identifier = 0,
		tuple = 0, // default is singleton
		not = 0;

	char *star_p = NULL, *p = expression;
	while ( *p && scope ) {
		switch ( *p ) {
		case '~':
			not = !not;
			p++; break;
		case '*':
			if ( !(star_exp) && !not ) {
				// save star in case we cannot find identifier
				if ( p[1] && !strmatch( ":,)", p[1] ) ) {
					push_exponent( AS_SUB, 0, &star_exp );
					push_exponent( AS_SUB, 0, &star_exp );
					push_exponent( SUB, 1, &star_exp );
				}
				for ( listItem *i=*exponent; i!=NULL; i=i->next )
					addItem( &star_exp, i->ptr );
				star_p = p;
			}
			// apply dereferencing operator to whatever comes next
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				push_exponent( SUB, 1, exponent );
				push_exponent( AS_SUB, 0, exponent );
				push_exponent( AS_SUB, 1, exponent );
			}
			p++; break;
		case '%':
			locate_mark( p+1, &mark_exp );
			p++; break;
		case '(':
			scope++;
			icast.value = tuple;
			addItem( &stack.tuple, icast.ptr );
			tuple = !p_single( p );
			if (( mark_exp )) {
				tuple |= 2;
				addItem( &stack.premark, *exponent );
				do addItem( exponent, popListItem( &mark_exp ));
				while (( mark_exp ));
			}
			if ( tuple & 1 ) {	// not singleton
				push_exponent( AS_SUB, 0, exponent );
			}
			addItem( &stack.level, level );
			level = *exponent;
			icast.value = not;
			addItem( &stack.not, icast.ptr );
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( scope == 1 ) { scope=0; break; }
			while ( *exponent != level )
				popListItem( exponent );
			popListItem( exponent );
			push_exponent( AS_SUB, 1, exponent );
			p++; break;
		case ')':
			scope--;
			if ( !scope ) break;
			not = (int) popListItem( &stack.not );
			if ( tuple & 1 ) {	// not singleton
				while ( *exponent != level )
					popListItem( exponent );
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			if ( tuple & 2 ) {	// sub expression
				listItem *tag = popListItem( &stack.premark );
				while ( *exponent != tag )
					popListItem( exponent );
			}
			tuple = (int) popListItem( &stack.tuple );
			p++; break;
		case '?':
		case '.':
			p++; break;
		default:
			if ( not ) p++;
			else scope = 0;
		}
	}
	freeListItem( &stack.not );
	freeListItem( &stack.tuple );
	freeListItem( &stack.level );
	freeListItem( &stack.premark );
	if ( *p && *p!=',' && *p!=')' ) {
		freeListItem( &star_exp );
		return p;
	}
	else if (( star_exp )) {
		freeListItem( exponent );
		do { addItem( exponent, popListItem( &star_exp ) ); }
		while (( star_exp ));
		return star_p;
	}
	return NULL;
}

static char *
locate_mark( char *expression, listItem **exponent )
/*
	returns first '?' found in expression, together with exponent
	Note that we ignore the '~' signs and %(...) sub-expressions
	Note also that the caller is expected to reorder the list
*/
{
	union { int value; void *ptr; } icast;
	struct {
		listItem *couple;
		listItem *level;
	} stack = { NULL, NULL };
	listItem *level = NULL;

	int	scope = 1,
		couple = 0; // default is singleton

	char *p = expression;
	while ( *p && scope ) {
		switch ( *p ) {
		case '~':
			p++; break;
		case '*':
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				push_exponent( AS_SUB, 1, exponent );
				push_exponent( SUB, 0, exponent );
				push_exponent( SUB, 1, exponent );
			}
			p++; break;
		case '%':
			p = p_prune( PRUNE_FILTER, p );
			break;
		case '(':
			scope++;
			icast.value = couple;
			addItem( &stack.couple, icast.ptr );
			couple = !p_single( p );
			if ( couple ) {
				push_exponent( SUB, 0, exponent );
			}
			addItem( &stack.level, level );
			level = *exponent;
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( scope == 1 ) { scope=0; break; }
			while ( *exponent != level )
				popListItem( exponent );
			popListItem( exponent );
			push_exponent( SUB, 1, exponent );
			p++; break;
		case ')':
			scope--;
			if ( !scope ) break;
			if ( couple ) {
				while ( *exponent != level )
					popListItem( exponent );
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			couple = (int) popListItem( &stack.couple );
			p++; break;
		case '?':
			scope = 0;
			break;
		case '.':
			p++; break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	freeListItem( &stack.couple );
	freeListItem( &stack.level );
	return ((*p=='?') ? p : NULL );
}

