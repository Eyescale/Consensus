#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "locate.h"

//===========================================================================
//	bm_locate_pivot
//===========================================================================
char *
bm_locate_pivot( char *expression, listItem **exponent )
/*
	returns first term (according to prioritization) which
	is not a wildcard and is not negated, with corresponding
	exponent (in reverse order).
*/
{
	int target = xp_target( expression, QMARK );
	if ( target == 0 ) return NULL;
	else target =	( target & QMARK ) ? QMARK :
			( target & IDENTIFIER ) ? IDENTIFIER :
			( target & MOD ) ? MOD :
			( target & CHARACTER ) ? CHARACTER :
			( target & STAR ) ? STAR :
			( target & EMARK ) ? EMARK : 0;

	struct { listItem *flags, *level, *premark; } stack;
	memset( &stack, 0, sizeof(stack) );
	listItem *level = NULL, *mark_exp = NULL;
	int	scope = 0, done = 0,
		tuple = 0, // default is singleton
		not = 0;
	char *	p = expression, *q;
	while ( *p && !done ) {
		switch ( *p ) {
		case '~':
			not = !not;
			p++; break;
		case '*':
			if ( !not && target==STAR ) {
				if ( p[1] && !strmatch( ":,)", p[1] ) ) {
					xpn_add( exponent, SUB, 1 );
					xpn_add( exponent, AS_SUB, 0 );
					xpn_add( exponent, AS_SUB, 0 );
				}
				done=2; break;
			}
			// apply dereferencing operator to whatever comes next
			if ( p[1] && !strmatch( ":,)", p[1] ) ) {
				xpn_add( exponent, SUB, 1 );
				xpn_add( exponent, AS_SUB, 0 );
				xpn_add( exponent, AS_SUB, 1 );
			}
			p++; break;
		case '%':
			if ( p[1]=='?' ) {
				if ( !not && target==QMARK ) { done=2; break; }
				p+=2; break;
			}
			else if ( p[1]=='!' ) {
				if ( !not && target==EMARK ) { done=2; break; }
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				if ( !not && target==MOD ) { done=2; break; }
				p++; break;
			}
			else { bm_locate_mark( p+1, &mark_exp ); p++; }
			break;
		case '(':
			scope++;
			add_item( &stack.flags, not|tuple );
			tuple = p_single(p) ? 0 : 2;
			if (( mark_exp )) {
				tuple |= 4;
				addItem( &stack.premark, *exponent );
				do addItem( exponent, popListItem( &mark_exp ));
				while (( mark_exp ));
			}
			if ( tuple & 2 ) { // not singleton
				xpn_add( exponent, AS_SUB, 0 );
			}
			addItem( &stack.level, level );
			level = *exponent;
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( !scope ) { done=1; break; }
			while ( *exponent != level )
				popListItem( exponent );
			xpn_set( *exponent, AS_SUB, 1 );
			p++; break;
		case ')':
			if ( !scope ) { done=1; break; }
			scope--;
			if ( tuple & 2 ) {	// not singleton
				while ( *exponent != level )
					popListItem( exponent );
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			if ( tuple & 4 ) {	// sub expression
				listItem *tag = popListItem( &stack.premark );
				while ( *exponent != tag )
					popListItem( exponent );
			}
			int flags = pop_item( &stack.flags );
			not = flags & 1;
			tuple = flags & 6;
			p++; break;
		case '?':
			p++; break;
		case '.':
			p = p_prune( PRUNE_IDENTIFIER, p+1 );
			break;
		case '/':
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		case '\'':
			if ( !not && target==CHARACTER ) { done=2; break; }
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		default:
			if ( !not && target==IDENTIFIER ) { done=2; break; }
			p = p_prune( PRUNE_IDENTIFIER, p+1 );
		}
	}
	if ( scope ) {
		freeListItem( &stack.flags );
		freeListItem( &stack.level );
		freeListItem( &stack.premark );
	}
	return ( done==2 ? p : NULL );
}

int
xp_target( char *expression, int target )
{
	char *	p = expression, *q;
	int	candidate = 0, not = 0,
		level = 0, done = 0;
	while ( *p && !done ) {
		switch ( *p ) {
		case '~':
			not = !not;
			p++; break;
		case '*':
			if ( !not ) candidate |= STAR;
			p++; break;
		case '%':
			if ( p[1]=='?' ) {
				if ( !not ) {
					if ( target&QMARK ) {
						candidate |= QMARK;
						return candidate;
					}
					else candidate |= QMARK;
				}
				p+=2; break;
			}
			else if ( p[1]=='!' ) {
				if ( !not ) candidate |= EMARK;
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ":,)", p[1] ) ) {
				if ( !not ) candidate |= MOD;
				p++; break;
			}
			else { p++; break; }
			break;
		case '(':
			// skip ternary-operated expressions
			q = p_prune( PRUNE_TERNARY, p );
			if ( *q=='?' ) {
				q = p_prune( PRUNE_TERNARY, q ); // ':'
				q = p_prune( PRUNE_TERNARY, q ); // ')'
				p = q+1; break;
			}
			level++;
			p++; break;
		case ')':
			if ( !level ) { done=1; break; }
			level--;
			p++; break;
		case ',':
			if ( !level ) { done=1; break; }
			p++; break;
		case ':':
		case '?':
			p++; break;
		case '.':
			p = p_prune( PRUNE_IDENTIFIER, p+1 );
			break;
		case '/':
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		case '\'':
			if ( !not ) candidate |= CHARACTER;
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		default:
			if ( !not ) candidate |= IDENTIFIER;
			p = p_prune( PRUNE_IDENTIFIER, p+1 );
		}
	}
	return candidate;
}

//===========================================================================
//	bm_locate_mark, bm_locate_param
//===========================================================================
char *
bm_locate_mark( char *expression, listItem **exponent )
{
	return bm_locate_param( expression, exponent, NULL, NULL );
}

char *
bm_locate_param( char *expression, listItem **exponent, BMLocateCB arg_CB, void *user_data )
/*
	if arg_CB is set
		invokes arg_CB on each .arg found in expression
	Otherwise
		returns first '?' found in expression, together with exponent
	Note that we do not enter the '~' signed and %(...) sub-expressions
	Note also that the caller is expected to reorder the list of exponents
*/
{
	struct { listItem *couple, *level; } stack = { NULL, NULL };
	listItem *level = NULL;
	int	scope = 0,
		done = 0,
		couple = 0; // base is singleton
	char *p = expression, *q;
	while ( *p && !done ) {
		switch ( *p ) {
		case '~':
			if ( !arg_CB ) { p++; break; }
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
			q = p_prune( PRUNE_TERNARY, p );
			if ( *q=='?' ) {
				if ( !arg_CB ) {
					p = q;
					done=2; break;
				}
				q = p_prune( PRUNE_TERNARY, q ); // ':'
				q = p_prune( PRUNE_TERNARY, q ); // ')'
				p = q+1; break;
			}
			scope++;
			add_item( &stack.couple, couple );
			if ( *q==',' ) {
				couple = 1;
				xpn_add( exponent, SUB, 0 );
			}
			else couple = 0;
			addItem( &stack.level, level );
			level = *exponent;
			p++; break;
		case ':':
			while ( *exponent != level )
				popListItem( exponent );
			p++; break;
		case ',':
			if ( !scope ) { done=1; break; }
			while ( *exponent != level ) {
				popListItem( exponent );
			}
			xpn_set( *exponent, SUB, 1 );
			p++; break;
		case ')':
			if ( !scope ) { done=1; break; }
			scope--;
			if ( couple ) {
				while ( *exponent != level ) {
					popListItem( exponent );
				}
				popListItem( exponent );
			}
			level = popListItem( &stack.level );
			couple = pop_item( &stack.couple );
			p++; break;
		case '?':
			if (( arg_CB )) p++; // just ignore
			else done = 2;
			break;
		case '.':
			p++;
			if ( arg_CB && !is_separator(*p) )
	 			arg_CB( p, *exponent, user_data );
			// no break
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	if ( scope ) {
		freeListItem( &stack.couple );
		freeListItem( &stack.level );
	}
	return ((*p=='?') ? p : NULL );
}

//===========================================================================
//	xpn_add, xpn_set
//===========================================================================
void
xpn_add( listItem **xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	addItem( xp, icast.ptr );
}
void
xpn_set( listItem *xp, int as_sub, int position )
{
	union { int value; void *ptr; } icast;
	icast.value = as_sub + position;
	xp->ptr = icast.ptr;
}

//===========================================================================
//	xpn_out
//===========================================================================
void
xpn_out( FILE *stream, listItem *xp )
{
	union { int value; void *ptr; } icast;
	while ( xp ) {
		icast.ptr = xp->ptr;
		fprintf( stream, "%d", icast.value );
		xp = xp->next;
	}
}

