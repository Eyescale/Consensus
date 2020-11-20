#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "util.h"

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
			else { p_locate_param( p+1, &mark_exp, NULL, NULL ); p++; }
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
			p++; break;
		case '.':
			p = p_prune( PRUNE_IDENTIFIER, p+1 );
			break;
		case '/':
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		default:
			if ( not ) p_prune( PRUNE_IDENTIFIER, p );
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
		do addItem( exponent, popListItem( &star_exp ) );
		while (( star_exp ));
		return star_p;
	}
	return NULL;
}

//===========================================================================
//	p_locate_param
//===========================================================================
char *
p_locate_param( char *expression, listItem **exponent, BMLocateCB arg_CB, void *user_data )
/*
	if arg_CB is set
		invokes arg_CB on each .arg found in expression
	Otherwise
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
		case '%':
			p = p_prune( PRUNE_FILTER, p+1 );
			break;
		case '*':
			if (( arg_CB )) {
				p = p_prune( PRUNE_FILTER, p+1 );
				break;
			}
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				xpn_add( exponent, AS_SUB, 1 );
				xpn_add( exponent, SUB, 0 );
				xpn_add( exponent, SUB, 1 );
			}
			p++; break;
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
			if (( arg_CB )) p++;
			else scope = 0;
			break;
		case '.':
			p++;
			if ( !is_separator(*p) ) {
				if (( arg_CB )) {
	 				arg_CB( p, *exponent, user_data );
				}
				else p = p_prune( PRUNE_IDENTIFIER, p );
			}
			break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	freeListItem( &stack.couple );
	freeListItem( &stack.level );
	return ((*p=='?') ? p : NULL );
}

//===========================================================================
//	p_prune
//===========================================================================
static char *prune_format( char * );
static char *prune_regex( char * );

char *
p_prune( PruneType type, char *p )
{
	switch ( type ) {
	case PRUNE_FORMAT:
		return prune_format( p );
	case PRUNE_IDENTIFIER:
		switch ( *p ) {
		case '\'':
			return p + ( p[1]=='\\' ? p[2]=='x' ? 6 : 4 : 3 );
		case '/':
			return prune_regex( p );
		default:
			while ( !is_separator(*p) ) p++;
			return p;
		}
	case PRUNE_TERM:
	case PRUNE_FILTER:
		if ( *p==')' ) p++;
		int level = 1;
		for ( ; ; ) {
			switch ( *p ) {
			case '\0': return p;
			case '(':
				level++;
				p++; break;
			case ')':
				if ( level == 1 ) return p;
				level--;
				p++; break;
			case '/':
				p = prune_regex( p );
				break;
			case '"':
				p = prune_format( p );
				break;
			case '\'':
				p += ( p[1]=='\\' ? p[2]=='x' ? 6 : 4 : 3 );
				break;
			case ':':
				if ( type==PRUNE_FILTER && level==1 )
					return p;
				p++; break;
			case ',':
				if ( level == 1 ) return p;
				p++; break;
			default:
				p++; break;
			}
		}
	}
}
static char *
prune_format( char *p )
{
	for ( p++; *p; )
		switch ( *p++ ) {
		case '\"': return p;
		case '\\': p++; break;
		}
	return p;
}
static char *
prune_regex( char *p )
{
	int bracket = 0;
	for ( p++; *p; )
		switch ( *p++ ) {
		case '/':
			if ( !bracket )
				return p;
			break;
		case '\\':
			p++; break;
		case '[':
			bracket = 1;
			break;
		case ']':
			bracket = 0;
			break;
		}
	return p;
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
