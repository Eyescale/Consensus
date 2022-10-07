#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"

#define BMTraverseError -1

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
static void CB_alert( BMCBName, char * );

char *
bm_traverse( char *expression, BMTraverseData *traverse_data, listItem **stack, int flags )
{
	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	union { int value; void *ptr; } icast;
	char *p = expression;
	while ( *p && !traverse_data->done ) {
		if (( table[ BMPreemptCB ] )) {
			switch ( table[BMPreemptCB]( traverse_data, p, flags ) ) {
			case BM_CONTINUE:
				break;
			case BM_DONE:
				flags = traverse_data->flags;
				p = traverse_data->p;
				continue;
			case BM_PRUNE_TERM:
				flags = traverse_data->flags;
				p = p_prune( PRUNE_TERM, p+1 );
				continue;
			default: CB_alert( BMPreemptCB, p );
			} }
		switch ( *p ) {
		case '>':
		case '<':
			p++; break;
		case '~':
			if (( table[ BMNotCB ] )) {
				switch ( table[BMNotCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				case BM_PRUNE_FILTER:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_FILTER, p+1 );
					continue;
				case BM_PRUNE_TERM:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_TERM, p+1 );
					continue;
				default: CB_alert( BMNotCB, p );
				} }
			if is_f( NEGATED ) f_clr( NEGATED )	
			else f_set( NEGATED )
			p++; break;
		case '{':
			if (( table[ BMBgnSetCB ] )) {
				switch ( table[BMBgnSetCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				default: CB_alert( BMBgnSetCB, p );
				} }
			f_push( stack )
			f_reset( FIRST|SET, 0 )
			p++; break;
		case '}':
			if ( !is_f(SET) )
				{ traverse_data->done = 1; break; }
			if ((*stack)) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
			}
			if (( table[ BMEndSetCB ] )) {
				switch ( table[BMEndSetCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				default: CB_alert( BMEndSetCB, p );
				} }
			f_pop( stack, 0 )
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
			if (( table[ BMBgnPipeCB ] )) {
				switch ( table[BMBgnPipeCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				default: CB_alert( BMBgnPipeCB, p );
				} }
			f_push( stack )
			f_reset( PIPED, SET )
			p++; break;
		case '*':
			if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				if (( table[ BMStarCharacterCB ] )) {
					switch ( table[BMStarCharacterCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					default: CB_alert( BMStarCharacterCB, p );
					} }
				f_clr( NEGATED )
				f_set( INFORMED )
				p++; break;
			}
			if (( table[ BMDereferenceCB ] )) {
				switch ( table[BMDereferenceCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				case BM_PRUNE_FILTER:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_FILTER, p+1 );
					continue;
				case BM_PRUNE_TERM:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_TERM, p+1 );
					continue;
				default: CB_alert( BMDereferenceCB, p );
				} }
			f_clr( INFORMED )
			p++; break;
		case '%':
			if ( strmatch( "?!", p[1] ) ) {
				if (( table[ BMMarkRegisterCB ] )) {
					switch ( table[BMMarkRegisterCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					case BM_PRUNE_FILTER:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_FILTER, p );
						continue;
					case BM_PRUNE_TERM:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_TERM, p );
						continue;
					default: CB_alert( BMMarkRegisterCB, p );
					} }
				f_clr( NEGATED )
				f_set( INFORMED )
				p+=2; break;
			}
			else if ( p[1]=='(' ) {
				if (( table[ BMSubExpressionCB ] )) {
					switch ( table[BMSubExpressionCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					case BM_PRUNE_FILTER:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_FILTER, p+1 );
						continue;
					case BM_PRUNE_TERM:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_TERM, p+1 );
						continue;
					default: CB_alert( BMSubExpressionCB, p );
					} }
				f_push( stack )
				f_reset( FIRST|SUB_EXPR, SET )
				p+=2; break;
			}
			else if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
				if (( table[ BMModCharacterCB ] )) {
					switch ( table[BMModCharacterCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					default: CB_alert( BMModCharacterCB, p );
					} }
				f_clr( NEGATED )
				f_set( INFORMED )
				p++; break;
			}
			else {
				fprintf( stderr, ">>>>> B%%: Error: bm_traverse: unexpected '%%'_continuation\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			break;
		case '(':
			if ( p[1]==':' ) {
				if (( table[ BMLiteralCB ] )) {
					switch ( table[BMLiteralCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					default: CB_alert( BMLiteralCB, p );
					} }
				p = p_prune( PRUNE_LITERAL, p );
				f_set( INFORMED )
				break;
			}
			if (( table[ BMOpenCB ] )) {
				switch ( table[BMOpenCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMOpenCB, p );
				} }
			f_push( stack )
			f_reset( FIRST|LEVEL, SET|SUB_EXPR|MARKED )
			p++; break;
		case ':':
			if (( table[ BMFilterCB ] )) {
				switch ( table[BMFilterCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				case BM_PRUNE_FILTER:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_FILTER, p+1 );
					continue;
				case BM_PRUNE_TERM:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_TERM, p+1 );
					continue;
				case BM_PRUNE_TERNARY:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_TERNARY, p );
					continue;
				} }
			f_clr( INFORMED )
			f_set( FILTERED )
			p++; break;
		case ',':
			if ( !is_f(SET|SUB_EXPR|LEVEL) )
				{ traverse_data->done = 1; break; }
			if (( table[ BMDecoupleCB ] )) {
				switch ( table[BMDecoupleCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				case BM_PRUNE_FILTER:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_FILTER, p+1 );
					continue;
				case BM_PRUNE_TERM:
					flags = traverse_data->flags;
					p = p_prune( PRUNE_TERM, p+1 );
					continue;
				default: CB_alert( BMDecoupleCB, p );
				} }
			if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
			f_clr( FILTERED|INFORMED )
			p++; break;
		case ')':
			if ( is_f(PIPED) && !is_f(LEVEL|SUB_EXPR) ) {
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					traverse_data->f_next = icast.value;
				}
				if (( table[ BMEndPipeCB ] )) {
					switch ( table[BMEndPipeCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					default: CB_alert( BMEndPipeCB, p );
					} }
				f_pop( stack, 0 )
			}
			if ( !is_f(LEVEL|SUB_EXPR) )
				{ traverse_data->done = 1; break; }
			if ((*stack)) {
				icast.ptr = (*stack)->ptr;
				traverse_data->f_next = icast.value;
			}
			if (( table[ BMCloseCB ] )) {
				switch ( table[BMCloseCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE: // Assumption: user moved past ':'
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMCloseCB, p );
				} }
			f_pop( stack, 0 );
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '?':
			if is_f( INFORMED ) {
				if ( !is_f(SET|LEVEL|SUB_EXPR) )
					{ traverse_data->done = 1; break; }
				if (( table[ BMTernaryOperatorCB ] )) {
					switch ( table[BMTernaryOperatorCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE: // Assumption: user moved past ':'
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					case BM_PRUNE_TERNARY: // Assumption: user moved to ':'
						flags = traverse_data->flags;
						p = traverse_data->p;
						p = p_prune( PRUNE_TERNARY, p ); // move to ')'
						continue;
					default: CB_alert( BMTernaryOperatorCB, p );
					} }
				f_clr( NEGATED|FILTERED|INFORMED )
				f_set( TERNARY )
				p++; break;
			}
			if (( table[ BMWildCardCB ] )) {
				switch ( table[BMWildCardCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMWildCardCB, p );
				} }
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '.':
			if ( p[1]=='(' ) {
				if (( table[ BMDotExpressionCB ] )) {
					switch ( table[BMDotExpressionCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					case BM_PRUNE_FILTER:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_FILTER, p+1 );
						continue;
					case BM_PRUNE_TERM:
						flags = traverse_data->flags;
						p = p_prune( PRUNE_TERM, p+1 );
						continue;
					default: CB_alert( BMDotExpressionCB, p );
					} }
				f_push( stack )
				f_reset( FIRST|DOT|LEVEL, SET|SUB_EXPR|MARKED )
				p+=2; break;
			}
			else if ( !is_separator(p[1]) ) {
				if (( table[ BMDotIdentifierCB ] )) {
					switch ( table[BMDotIdentifierCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					default: CB_alert( BMDotIdentifierCB, p );
					} }
				p = p_prune( PRUNE_IDENTIFIER, p+2 );
				f_clr( NEGATED )
				f_set( INFORMED )
				break;
			}
			else if ( p[1]=='.' ) {
				if (( table[ BMListCB ] )) {
					switch ( table[BMListCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					case BM_DONE:
						flags = traverse_data->flags;
						p = traverse_data->p;
						continue;
					default: CB_alert( BMListCB, p );
					} }
				f_pop( stack, 0 );
				// p_prune will return on closing ')'
				p = p_prune( PRUNE_LIST, p );
				f_set( INFORMED )
				break;
			}
			if (( table[ BMWildCardCB ] )) {
				switch ( table[BMWildCardCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMWildCardCB, p );
				} }
			f_clr( NEGATED )
			f_set( INFORMED )
			p++; break;
		case '"':
			if (( table[ BMFormatCB ] )) {
				switch ( table[BMFormatCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				default: CB_alert( BMFormatCB, p );
				} }
			p = p_prune( PRUNE_FORMAT, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '\'':
			if (( table[ BMCharacterCB ] )) {
				switch ( table[BMCharacterCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMCharacterCB, p );
				} }
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		case '/':
			if (( table[ BMRegexCB ] )) {
				switch ( table[BMRegexCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				default: CB_alert( BMRegexCB, p );
				} }
			p = p_prune( PRUNE_IDENTIFIER, p );
			f_clr( NEGATED )
			f_set( INFORMED )
			break;
		default:
			if ( is_separator(*p) ) {
				fprintf( stderr, ">>>>> B%%: Error: bm_traverse: identifier expected\n" );
				traverse_data->done = BMTraverseError;
				break;
			}
			if (( table[ BMIdentifierCB ] )) {
				switch ( table[BMIdentifierCB]( traverse_data, p, flags ) ) {
				case BM_CONTINUE:
					break;
				case BM_DONE:
					flags = traverse_data->flags;
					p = traverse_data->p;
					continue;
				default: CB_alert( BMIdentifierCB, p );
				} }
			p = p_prune( PRUNE_IDENTIFIER, p );
			if ( *p=='~' ) {
				if (( table[ BMSignalCB ] )) {
					switch ( table[BMSignalCB]( traverse_data, p, flags ) ) {
					case BM_CONTINUE:
						break;
					default: CB_alert( BMSignalCB, p );
					} }
				p++;
			}
			f_clr( NEGATED )
			f_set( INFORMED )
		}
	}
	return (( traverse_data->p = p ));
}

static void
CB_alert( BMCBName name, char *p )
{
	fprintf( stderr, ">>>>> B%%: Error: bm_traverse: callback return value not handled\n" );
}
