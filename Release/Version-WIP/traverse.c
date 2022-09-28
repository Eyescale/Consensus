#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
#define BMTraverseError -1
#define bar_( CB, out ) \
	if ((table[CB]) && table[CB]( traverse_data, p, flags )==BM_DONE ) { \
		p = traverse_data->p; \
		flags = traverse_data->flags; \
		out; }
char *
bm_traverse( char *expression, BMTraverseData *traverse_data, listItem **stack, int flags )
{
	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	union { int value; void *ptr; } icast;
	char *	p = expression;
	while ( *p && !traverse_data->done ) {
		bar_( BMPreemptCB, continue )
		switch ( *p ) {
		case '~':
			bar_( BMNegatedCB, break )
			if is_f( NEGATED ) f_clr( NEGATED )	
			else f_set( NEGATED )
			p++; break;
		case '{':
			bar_( BMBgnSetCB, break )
			f_push( stack )
			f_reset( SET|FIRST, 0 )
			p++; break;
		case '}':
			if ( !is_f(SET) )
				{ traverse_data->done = 1; break; }
			icast.ptr = (*stack)->ptr;
			traverse_data->f_next = icast.value;
			bar_( BMEndSetCB, break )
			f_pop( stack, 0 )
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '|':
			bar_( BMBgnPipeCB, break )
			f_push( stack )
			f_reset( PIPED, SET )
			p++; break;
		case '*':
			if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				bar_( BMStarCharacterCB, break )
				f_clr( NEGATED )
				f_set( INFORMED )
				p++; break;
			}
			bar_( BMDereferenceCB, break )
			f_clr( INFORMED )
			p++; break;
		case '%':
			if ( strmatch( "?!", p[1] ) ) {
				bar_( BMMarkRegisterCB, break )
				f_clr( NEGATED )
				f_set( INFORMED )
				p+=2; break;
			}
			else if ( p[1]=='(' ) {
				if ( !p_single(p) ) f_set( COUPLE )
				bar_( BMSubExpressionCB, break )
				f_push( stack )
				f_reset( SUB_EXPR|FIRST, 0 )
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				bar_( BMModCharacterCB, break )
				f_clr( NEGATED )
				f_set( INFORMED )
				p++; break;
			}
			else {
				bar_( BMSyntaxErrorCB, break )
				fprintf( stderr, ">>>>> B%%: Error: C_traverse: unexpected '%%'_continuation\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			break;
		case '(':
			if ( p[1]==':' ) {
				bar_( BMLiteralCB, break )
				p = p_prune( PRUNE_LITERAL, p );
				f_clr( NEGATED )
				f_set( INFORMED )
				break;
			}
			if ( !p_single(p) ) f_set( COUPLE )
			bar_( BMOpenCB, break )
			f_push( stack )
			f_reset( LEVEL|FIRST, 0 )
			p++; break;
		case ',':
			if ( !is_f(SET|LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			bar_( BMDecoupleCB, break )
			if is_f( LEVEL|SUB_EXPR ) f_clr( FIRST )
			f_clr( NEGATED|FILTERED|INFORMED )
			p++; break;
		case ':':
			if ( !is_f(SET|LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			bar_( BMFilterCB, break )
			f_clr( NEGATED|INFORMED )
			f_set( FIRST|FILTERED )
			p++; break;
		case ')':
			if ( is_f(PIPED) && !is_f(LEVEL|SUB_EXPR) ) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
				bar_( BMEndPipeCB, break )
				f_pop( stack, 0 )
			}
			if ( !is_f(LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			icast.ptr = (*stack)->ptr;
			traverse_data->f_next = icast.value;
			bar_( BMCloseCB, break )
			f_pop( stack, 0 );
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				bar_( BMTernaryOperatorCB, break )
				if ( !is_f(SET|LEVEL|SUB_EXPR) )
					{ traverse_data->done = 1; break; }
				f_clr( NEGATED|FILTERED|INFORMED )
				f_set( TERNARY )
				p++; break;
			}
			bar_( BMWildCardCB, break )
			f_set( INFORMED )
			p++; break;
		case '.':
			if ( p[1]=='(' ) {
				bar_( BMDotExpressionCB, break )
				f_push( stack )
				f_reset( DOT|LEVEL|FIRST, 0 )
				p+=2; break;
			}
			else if ( !is_separator(p[1]) ) {
				bar_( BMDotIdentifierCB, break )
				p = p_prune( PRUNE_IDENTIFIER, p+1 );
				f_clr( NEGATED )
				f_set( INFORMED )
				break;
			}
			bar_( BMWildCardCB, break )
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '"':
			bar_( BMFormatCB, break )
			p = p_prune( PRUNE_FORMAT, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '\'':
			bar_( BMCharacterCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '/':
			bar_( BMRegexCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		default:
			if ( is_separator(*p) ) {
				bar_( BMSyntaxErrorCB, break )
				fprintf( stderr, ">>>>> B%%: Error: C_traverse: expected identifier\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			bar_( BMIdentifierCB, break )
			p = p_prune( PRUNE_IDENTIFIER, p );
			if ( *p=='~' ) {
				bar_( BMSignalCB, break )
				p++;
			}
			f_clr( NEGATED )
			f_set( INFORMED )
		}
	}
	return p;
}
