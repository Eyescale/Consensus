#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
#define BMTraverseError -1
#define bar( CB, out ) \
	if ((table[CB]) && table[CB]( traverse_data, p, flags )==BM_DONE ) { \
		p = traverse_data->p; \
		flags = traverse_data->flags; \
		out; }
char *
bm_traverse( char *expression, BMTraverseData *traverse_data )
{
	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	union { int value; void *ptr; } icast;
	listItem *stack = NULL;
	char *	p = expression;
	int 	flags = FIRST;
	bar( BMBgnCB, return p )
	while ( *p && !traverse_data->done ) {
		bar( BMPreemptCB, continue )
		switch ( *p ) {
		case '~':
			bar( BMNegatedCB, break )
			if is_f( NEGATED ) f_clr( NEGATED )	
			else f_set( NEGATED )
			p++; break;
		case '{':
			bar( BMBgnSetCB, break )
			f_push( &stack )
			f_reset( SET|FIRST, 0 )
			p++; break;
		case '}':
			if ( !is_f(SET) )
				{ traverse_data->done = 1; break; }
			icast.ptr = stack->ptr;
			traverse_data->f_next = icast.value;
			bar( BMEndSetCB, break )
			f_pop( &stack, 0 )
			f_set( INFORMED )
			p++; break;
		case '|':
			bar( BMBgnPipeCB, break )
			f_push( &stack )
			f_reset( PIPED, SET )
			p++; break;
		case '*':
			if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				bar( BMStarCharacterCB, break )
				f_set( INFORMED )
				p++; break;
			}
			bar( BMDereferenceCB, break )
			f_clr( INFORMED )
			p++; break;
		case '%':
			if ( strmatch( "?!", p[1] ) ) {
				bar( BMMarkRegisterCB, break )
				f_set( INFORMED )
				p+=2; break;
			}
			else if ( p[1]=='(' ) {
				bar( BMSubExpressionCB, break )
				f_push( &stack )
				f_reset( SUB_EXPR|FIRST, SET )
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				bar( BMModCharacterCB, break )
				f_set( INFORMED )
				p++; break;
			}
			else {
				bar( BMSyntaxErrorCB, break )
				fprintf( stderr, ">>>>> B%%: Error: C_traverse: unexpected '%%'_continuation\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			break;
		case '(':
			if ( p[1]==':' ) {
				bar( BMLiteralCB, break )
				p = p_prune( PRUNE_LITERAL, p );
				f_set( INFORMED ); break;
			}
			bar( BMOpenCB, break )
			f_push( &stack )
			f_reset( LEVEL|FIRST, SET )
			p++; break;
		case ',':
			if ( !is_f(SET|LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			bar( BMDecoupleCB, break )
			if is_f( LEVEL|SUB_EXPR ) f_clr( FIRST )
			f_clr( INFORMED )
			p++; break;
		case ':':
			if ( !is_f(SET|LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			bar( BMFilterCB, break )
			f_clr( INFORMED )
			f_set( FIRST )
			p++; break;
		case ')':
			if ( is_f(PIPED) && !is_f(LEVEL|SUB_EXPR) ) {
				icast.ptr = stack->ptr;
				traverse_data->f_next = icast.value;
				bar( BMEndPipeCB, break )
				f_pop( &stack, 0 )
			}
			if ( !is_f(SET|LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			icast.ptr = stack->ptr;
			traverse_data->f_next = icast.value;
			bar( BMCloseCB, break )
			f_pop( &stack, 0 );
			f_set( INFORMED );
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				bar( BMTernaryOperatorCB, break )
				if ( !is_f(SET|LEVEL|SUB_EXPR) )
					{ traverse_data->done = 1; break; }
				f_clr( INFORMED )
				f_set( TERNARY|FIRST )
				p++; break;
			}
			bar( BMWildCardCB, break )
			f_set( INFORMED );
			p++; break;
		case '.':
			if ( p[1]=='(' ) {
				bar( BMDotExpressionCB, break )
				f_push( &stack )
				f_reset( DOT|LEVEL|FIRST, 0 )
				p+=2; break;
			}
			else if ( !is_separator(p[1]) ) {
				bar( BMDotIdentifierCB, break )
				p = p_prune( PRUNE_IDENTIFIER, p+1 );
				f_set( INFORMED ); break;
			}
			bar( BMWildCardCB, break )
			f_set( INFORMED );
			p++; break;
		case '"':
			bar( BMFormatCB, break )
			p = p_prune( PRUNE_FORMAT, p );
			f_set( INFORMED ); break;
		case '\'':
			bar( BMCharacterCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_set( INFORMED ); break;
		case '/':
			bar( BMRegexCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_set( INFORMED ); break;
		default:
			if ( is_separator(*p) ) {
				bar( BMSyntaxErrorCB, break )
				fprintf( stderr, ">>>>> B%%: Error: C_traverse: expected identifier\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			bar( BMIdentifierCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			if ( *p=='~' ) {
				bar( BMSignalCB, break )
				p++;
			}
			f_set( INFORMED )
		}
	}
	freeListItem( &stack );
	bar( BMEndCB, {} )
	return p;
}
