#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "util.h"

//===========================================================================
//	p_strip
//===========================================================================
char *
p_strip( char *p )
/*
	returns string extracted from proto p, where
	. leading ':' has been removed
	. ((..%arg..))
		if followed by ':' has been removed
		if preceded by ':' has been removed
		otherwise has been replaced with '.'
*/
{
	if (( p == NULL ) || !strcmp(p,":") || *p!=':' )
		return NULL;

	CNString *s = newString();
	struct { char *bgn, *end; } arg;
	for ( p++; *p; p=( arg.end[1]==':' ) ? arg.end+2 : arg.end+1 ) {
		// locate %arg
		char *q = p;
		for ( int done=0; !done; )
			switch ( *q ) {
			case '\0':
				done = 1;
				break;
			case '%':
				if ( !is_separator(q[1]) ) {
					done = 1;
					break;
				}
				// no break
			default:
				q++;
			}
		if ( *q == '\0' ) break;
		arg.bgn = q; // on the '%'
		arg.end = p_prune( PRUNE_IDENTIFIER, q+1 );

		// expand to ((..%arg..))
		while ( arg.bgn!=p && arg.bgn[-1]=='(' && arg.end[1]==')' )
			{ arg.bgn--; arg.end++; }

		// skip or replace ((..%arg..)) with '.' depending on bordering ':'
		if ( arg.bgn != p ) {
			arg.bgn--;
			for ( char *q=p; q!=arg.bgn; q++ )
				StringAppend( s, *q );

			if ( arg.bgn[ 0 ] != ':' ) {
				StringAppend( s, *arg.bgn );
				if ( arg.end[ 1 ] != ':' )
					StringAppend( s, '.' );
			}
		}
		else if ( arg.end[ 1 ] != ':' )
			StringAppend( s, '.' );
	}
	p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return p;
}

//===========================================================================
//	p_locate
//===========================================================================
char *
p_locate( char *expression, listItem **exponent )
/*
	returns first term which is not a wildcard and is not negated,
	with corresponding exponent (in reverse order).
*/
{
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
					xpn_add( &star_exp, AS_SUB, 0 );
					xpn_add( &star_exp, AS_SUB, 0 );
					xpn_add( &star_exp, SUB, 1 );
				}
				for ( listItem *i=*exponent; i!=NULL; i=i->next )
					addItem( &star_exp, i->ptr );
				star_p = p;
			}
			// apply dereferencing operator to whatever comes next
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				xpn_add( exponent, SUB, 1 );
				xpn_add( exponent, AS_SUB, 0 );
				xpn_add( exponent, AS_SUB, 1 );
			}
			p++; break;
		case '%':
			if ( !strncmp( p, "%?", 2 ) ) {
				if ( not ) p+=2;
				else scope = 0;
			}
			else if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				if ( not ) p++;
				else scope = 0;
			}
			else { p_locate_mark( p+1, &mark_exp ); p++; }
			break;
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
				xpn_add( exponent, AS_SUB, 0 );
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
			xpn_add( exponent, AS_SUB, 1 );
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

//===========================================================================
//	p_locate_mark
//===========================================================================
char *
p_locate_mark( char *expression, listItem **exponent )
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
				xpn_add( exponent, AS_SUB, 1 );
				xpn_add( exponent, SUB, 0 );
				xpn_add( exponent, SUB, 1 );
			}
			p++; break;
		case '%':
			if ( p[1] == '?' )
				p+=2;
			else if ( !p[1] || strmatch( ":,)", p[1] ) )
				p++;
			else p = p_prune( PRUNE_COLON, p );
			break;
		case '(':
			scope++;
			icast.value = couple;
			addItem( &stack.couple, icast.ptr );
			couple = !p_single( p );
			if ( couple ) {
				xpn_add( exponent, SUB, 0 );
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
			xpn_add( exponent, SUB, 1 );
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

//===========================================================================
//	p_extract
//===========================================================================
char *
p_extract( char *p )
{
	CNString *s = newString();
	switch ( *p ) {
	case '%': // %? should be handled separately
#if 0
		if ( p[1] == '?' ) {
			fprintf( stderr, "Error: p_extract: %%?\n" );
			exit( -1 );
		}
#endif
	case '*':
		StringAppend( s, *p );
		break;
	default:
		for ( char *q=p; !is_separator(*q); q++ ) {
			StringAppend( s, *q );
		}
	}
	char *term = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return term;
}

//===========================================================================
//	p_prune
//===========================================================================
typedef struct {
	int level;
	int type;
} PruneData;
static void p_set( PruneData *, PruneType type, char * );
static int p_skip( char *p, PruneData *prune );

char *
p_prune( PruneType type, char *p )
{
	PruneData data;
	p_set( &data, type, p );
	while ( p_skip( p, &data ) ) { p++; }
	return p;
}
static void
p_set( PruneData *prune, PruneType type, char *p )
{
	prune->type = type;
	prune->level = ( *p==')' ) ? 2 : 1;
}
static int
p_skip( char *p, PruneData *prune )
{
	if ( prune->type == PRUNE_IDENTIFIER ) {
		if ( is_separator( *p ) )
			prune->level = 0;
	}
	else {
		switch ( *p ) {
		case '\0': prune->level=0; break;
		case '(': prune->level++; break;
		case ')': prune->level--; break;
		case ':':
			if ( prune->type != PRUNE_COLON )
				break;
			// no break
		case ',':
			if ( prune->level == 1 )
				prune->level = 0;
			break;
		}
	}
	return prune->level;
}

//===========================================================================
//	p_single
//===========================================================================
int
p_single( char *p )
/*
	assumption: *p == '('
*/
{
	int scope = 1;
	while ( *p++ ) {
		switch ( *p ) {
		case '(':
			scope++;
			break;
		case ')':
			scope--;
			if ( !scope ) return 1;
			break;
		case ',':
			if ( scope == 1 ) return 0;
			break;
		}
	}
	return 1;
}

//===========================================================================
//	p_filtered
//===========================================================================
int
p_filtered( char *p )
{
	int scope = 0;
	while ( *p ) {
		switch ( *p++ ) {
		case '(':
			scope++;
			break;
		case ')':
			if ( !scope ) return 0;
			scope--;
			break;
		case ',':
			if ( !scope ) return 0;
			break;
		case ':':
			if ( !scope ) return 1;
			break;
		}
	}
	return 0;
}

//===========================================================================
//	xpn_add
//===========================================================================
void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
}

//===========================================================================
//	xpn_out
//===========================================================================
void
xpn_out( FILE *stream, listItem *xp )
{
	while ( xp ) {
		fprintf( stream, "%d", (int)xp->ptr );
		xp = xp->next;
	}
}

//===========================================================================
//	dbg_out
//===========================================================================
void
dbg_out( char *pre, CNInstance *e, char *post, CNDB *db )
{
	if (( pre )) fprintf( stderr, "%s", pre );
	if (( e )) cn_out( stderr, e, db );
	if (( post )) fprintf( stderr, "%s", post );
}
