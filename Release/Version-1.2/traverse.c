#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"

#define BMTraverseError -1

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
static BMCB_take traverse_CB( BMCBName, BMTraverseData *, char **, int * );
#define _CB( func ) \
	if ( traverse_CB( func, traverse_data, &p, &flags )==BM_DONE ) continue;

char *
bm_traverse( char *expression, BMTraverseData *traverse_data, listItem **stack, int flags )
{
	union { int value; void *ptr; } icast;
	char *p = expression;
	while ( *p && !traverse_data->done ) {
		switch ( *p ) {
		case '>':
		case '<':
			p++; break;
		case '~':
_CB( BMNotCB )		if is_f( NEGATED ) f_clr( NEGATED )	
			else f_set( NEGATED )
			p++; break;
		case '{':
_CB( BMBgnSetCB )	f_push( stack )
			f_reset( FIRST|SET, 0 )
			p++;
_CB( BMPreTermCB )	break;
		case '}':
			if ( !is_f(SET) )
				{ traverse_data->done = 1; break; }
			if ((*stack)) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
			}
_CB( BMEndSetCB )	f_pop( stack, 0 )
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '|':
			if ( !is_f(SET) )
				{ traverse_data->done = 1; break; }
			if ((*stack)) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
			}
_CB( BMBgnPipeCB )	f_push( stack )
			f_reset( PIPED, SET )
			p++; break;
		case '*':
			if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
_CB( BMStarCharacterCB )	f_clr( NEGATED )
				f_set( INFORMED ) }
			else {
_CB( BMDereferenceCB )		f_clr( INFORMED ) }
			p++; break;
		case '%':
			if ( strmatch( "?!", p[1] ) ) {
_CB( BMMarkRegisterCB )		f_clr( NEGATED )
				f_set( INFORMED )
				p+=2; break;
			}
			else if ( p[1]=='(' ) {
_CB( BMSubExpressionCB )	f_push( stack )
				f_reset( FIRST|SUB_EXPR, SET )
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
_CB( BMModCharacterCB )		f_clr( NEGATED )
				f_set( INFORMED )
				p++; break;
			}
			else traverse_data->done = BMTraverseError;
			break;
		case '(':
			if ( p[1]==':' ) {
_CB( BMLiteralCB )		p = p_prune( PRUNE_LITERAL, p );
				f_set( INFORMED ) }
			else {
_CB( BMOpenCB )			f_push( stack )
				f_reset( FIRST|LEVEL, SET|SUB_EXPR|MARKED )
				p++;
_CB(( BMPreTermCB ))		}
			break;
		case ':':
_CB( BMFilterCB )	f_clr( INFORMED )
			f_set( FILTERED )
			p++; break;
		case ',':
			if ( !is_f(SET|SUB_EXPR|LEVEL) )
				{ traverse_data->done = 1; break; }
_CB( BMDecoupleCB )	if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
			f_clr( FILTERED|INFORMED )
			p++;
_CB( BMPreTermCB )	break;
		case ')':
			if ( is_f(PIPED) && !is_f(LEVEL|SUB_EXPR) ) {
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					traverse_data->f_next = icast.value;
				}
_CB( BMEndPipeCB )		f_pop( stack, 0 )
			}
			if ( !is_f(LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			if ((*stack)) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
			}
_CB( BMCloseCB )	f_pop( stack, 0 );
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				if ( !is_f(SET|LEVEL|SUB_EXPR) )
					{ traverse_data->done = 1; break; }
_CB( BMTernaryOperatorCB )	f_clr( NEGATED|FILTERED|INFORMED )
				f_set( TERNARY ) }
			else {
_CB( BMWildCardCB )		f_clr( NEGATED )
				f_set( INFORMED ) }
			p++; break;
		case '.':
			if ( p[1]=='(' ) {
_CB( BMDotExpressionCB )	f_push( stack )
				f_reset( FIRST|DOT|LEVEL, SET|SUB_EXPR|MARKED )
				p+=2; break; }
			else if ( !is_separator(p[1]) ) {
_CB( BMDotIdentifierCB )	p = p_prune( PRUNE_IDENTIFIER, p+2 );
				f_clr( NEGATED )
				f_set( INFORMED )
				break; }
			else if ( p[1]=='.' ) {
_CB( BMListCB )			f_pop( stack, 0 );
				// p_prune will return on closing ')'
				p = p_prune( PRUNE_LIST, p );
				f_set( INFORMED )
				break; }
			else {
_CB( BMWildCardCB )		f_clr( NEGATED )
				f_set( INFORMED )
				p++; break; }
		case '"':
_CB( BMFormatCB )	p = p_prune( PRUNE_FORMAT, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '\'':
_CB( BMCharacterCB )	p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '/':
_CB( BMRegexCB )	p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		default:
			if ( is_separator(*p) )
				traverse_data->done = BMTraverseError;
			else {
_CB( BMIdentifierCB )		p = p_prune( PRUNE_IDENTIFIER, p );
				if ( *p=='~' ) {
_CB( BMSignalCB )			p++; }
				f_clr( NEGATED )
				f_set( INFORMED ) }
		}
	}
	switch ( traverse_data->done ) {
	case BMTraverseError:
		fprintf( stderr, ">>>>> B%%: Error: bm_traverse: syntax error "
			"in expression: %s, at %s\n", expression, p );
	}
	return (( traverse_data->p = p ));
}

static BMCB_take CB_alert( BMTraverseData *, BMCBName, char * );
static BMCB_take
traverse_CB( BMCBName func, BMTraverseData *traverse_data, char **p, int *flags )
{
	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	if ( table[func] ) {
		switch ( table[func]( traverse_data, *p, *flags ) ) {
		case BM_CONTINUE:
			break;
		case BM_DONE:
			switch ( func ) {
			case BMBgnSetCB:
			case BMEndSetCB:
			case BMBgnPipeCB:
			case BMEndPipeCB:
			case BMFormatCB:
			case BMRegexCB:
			case BMSignalCB:
				return CB_alert( traverse_data, func, *p );
			default:
				*flags = traverse_data->flags;
				*p = traverse_data->p;
				return BM_DONE;
			}
			break;
		case BM_PRUNE_FILTER:
			switch ( func ) {
			case BMMarkRegisterCB:
				*flags = traverse_data->flags;
				*p = p_prune( PRUNE_FILTER, *p+2 );
				return BM_DONE;
			case BMDotExpressionCB:
			case BMDecoupleCB:
			case BMFilterCB:
			case BMSubExpressionCB:
			case BMDereferenceCB:
			case BMNotCB:
				*flags = traverse_data->flags;
				*p = p_prune( PRUNE_FILTER, *p+1 );
				return BM_DONE;
			default:
				return CB_alert( traverse_data, func, *p );
			}
			break;
		case BM_PRUNE_TERM:
			switch ( func ) {
			case BMMarkRegisterCB:
				*flags = traverse_data->flags;
				*p = p_prune( PRUNE_TERM, *p+2 );
				return BM_DONE;
			case BMPreTermCB:
			case BMDotExpressionCB:
			case BMDecoupleCB:
			case BMFilterCB:
			case BMSubExpressionCB:
			case BMDereferenceCB:
			case BMNotCB:
				*flags = traverse_data->flags;
				*p = p_prune( PRUNE_TERM, *p+1 );
				return BM_DONE;
			default:
				return CB_alert( traverse_data, func, *p );
			}
			break;
		case BM_PRUNE_TERNARY:
			switch ( func ) {
			case BMTernaryOperatorCB:
			case BMFilterCB:
				*flags = traverse_data->flags;
				*p = p_prune( PRUNE_TERNARY, *p );
				return BM_DONE;
			default:
				return CB_alert( traverse_data, func, *p );
			}
			break;
		default:
			return CB_alert( traverse_data, func, *p );
		}
	}
	return BM_CONTINUE;
}
static BMCB_take
CB_alert( BMTraverseData *traverse_data, BMCBName func, char *p )
{
	fprintf( stderr, ">>>>> B%%: Error: bm_traverse: callback return value not handled\n" );
	traverse_data->done = BMTraverseError;
	return BM_DONE;
}
