#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "traverse.h"

#define BMTraverseError -1

//===========================================================================
//	bm_traverse	- generic B% expression traversal
//===========================================================================
static char * _traverse( char *, BMTraverseData *, int );

char *
bm_traverse( char *expression, BMTraverseData *traverse_data, int flags )
{
	char *p = _traverse( expression, traverse_data, flags );
	switch ( traverse_data->done ) {
	case BMTraverseError:
		fprintf( stderr, ">>>>> B%%: Error: bm_traverse: syntax error "
			"in expression: %s, at %s\n", expression, p ); }
	return p;
}

//===========================================================================
//	_traverse
//===========================================================================
static BMCBTake traverse_CB( BMCBName, BMTraverseData *, char **, int *, int );
#define _CB( cb ) \
	if ( traverse_CB(cb,traverse_data,&p,&flags,f_next)==BM_DONE ) \
		continue;
#define _sCB( cb ) \
	if ( !(mode&TERNARY) || p_ternary(p) ) \
		_CB( cb )

static char *
_traverse( char *expression, BMTraverseData *traverse_data, int flags )
{
	int f_next, mode = traverse_data->done;
	traverse_data->done = 0;

	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	listItem **stack = traverse_data->stack;
	union { int value; void *ptr; } icast;
	char *p = expression;

	while ( *p && !traverse_data->done ) {
		switch ( *p ) {
			case '>':
				if ( is_f(VECTOR) ) {
					f_pop( stack, 0 )
					f_cls }
				p++; break;
			case '<':
				if ( is_f(INFORMED) ) // EENO
					f_clr( NEGATED|FILTERED|INFORMED )
				p++; break;
			case '!':
				p = p_prune( PRUNE_IDENTIFIER, p+2 );
				break;
			case '@':
			case '~':
				if ( p[1]=='<' ) {
					if ( mode&NEW ){ traverse_data->done=1; break; }
					else { p+=2; break; } }
_CB( BMNotCB )			if is_f( NEGATED ) f_clr( NEGATED )	
				else f_set( NEGATED )
				p++; break;
			case '{':
_CB( BMBgnSetCB )		f_push( stack )
				f_reset( FIRST|SET, 0 )
				p++;
				if ( table[BMTermCB] && p_filtered(p) )
					f_set( FILTERED )
_CB( BMTermCB )			break;
			case '}':
				if ( !is_f(SET) ) { traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
_CB( BMEndSetCB )		f_pop( stack, 0 )
				f_cls; p++; break;
			case '|':
				if ( !is_f(LEVEL|SET) ) { traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
_CB( BMBgnPipeCB )		f_push( stack )
				f_reset( PIPED, SET )
				p++; break;
			case '*':
				if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
_CB( BMStarCharacterCB )		f_cls }
				else {
_CB( BMDereferenceCB )			f_clr( INFORMED ) }
				p++; break;
			case '%':
				if ( strmatch( "?!|%", p[1] ) ) {
_CB( BMRegisterVariableCB )		f_cls; p+=2; break; }
				else if ( p[1]=='(' ) {
_CB( BMSubExpressionCB )		p++;
					f_next = FIRST|SUB_EXPR|is_f(SET);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
_sCB( BMOpenCB )			f_push( stack )
					f_reset( f_next, 0 )
					p++; break; }
				else if ( !p[1] || strmatch( ",:)}|", p[1] ) ) {
_CB( BMModCharacterCB )			f_cls; p++; break; }
				else traverse_data->done = BMTraverseError;
				break;
			case '(':
				if ( p[1]==':' ) {
_CB( BMLiteralCB )			if (!table[ BMLiteralCB ]) {
						// p_prune will return past closing ')'
						p = p_prune( PRUNE_LITERAL, p ); }
					f_set( INFORMED ) }
				else {
					f_next = FIRST|LEVEL|is_f(SET|SUB_EXPR|MARKED);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
_sCB( BMOpenCB )			f_push( stack )
					f_reset( f_next, 0 )
					p++;
					if ( table[BMTermCB] && p_filtered(p) )
						f_set( FILTERED )
_CB(( BMTermCB ))			}
				break;
			case ':':
				if ( p[1]=='<' ) {
					f_push( stack );
					f_set( VECTOR );
					p+=2; break; }
				else {
_CB( BMFilterCB )			f_cls; p++; break; }
			case ',':
				if ( !is_f(VECTOR|SET|SUB_EXPR|LEVEL) )
					{ traverse_data->done=1; break; }
_CB( BMDecoupleCB )		if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
				f_clr( FILTERED|INFORMED )
				p++;
				if ( table[BMTermCB] && p_filtered(p) )
					f_set( FILTERED )
_CB( BMTermCB )			break;
			case ')':
				if ( is_f(PIPED) && !is_f(LEVEL|SUB_EXPR) ) {
					if ((*stack)) {
						icast.ptr = (*stack)->ptr;
						f_next = icast.value; }
_CB( BMEndPipeCB )			f_pop( stack, 0 ) }
				if ( !is_f(LEVEL|SUB_EXPR) ) { traverse_data->done=1; break; }
				if ((*stack)) {
					icast.ptr = (*stack)->ptr;
					f_next = icast.value; }
_CB( BMCloseCB )		f_pop( stack, 0 );
				f_cls; p++; break;
			case '?':
				if is_f( INFORMED ) {
					if ( !is_f(SET|LEVEL|SUB_EXPR) ) { traverse_data->done=1; break; }
_CB( BMTernaryOperatorCB )		f_clr( NEGATED|FILTERED|INFORMED )
					f_set( TERNARY ) }
				else {
_CB( BMWildCardCB )			f_cls }
				p++; break;
			case '.':
				if ( p[1]=='(' ) {
_CB( BMDotExpressionCB )		p++;
					f_next=FIRST|DOT|LEVEL|is_f(SET|SUB_EXPR|MARKED);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
_sCB( BMOpenCB )			f_push( stack )
					f_reset( f_next, 0 )
					p++;
					if ( table[BMTermCB] && p_filtered(p) )
						f_set( FILTERED )
_CB( BMTermCB )				break; }
				else if ( !is_separator(p[1]) ) {
_CB( BMDotIdentifierCB )		p = p_prune( PRUNE_FILTER, p+2 );
					f_cls; break; }
				else if ( p[1]=='.' ) {
					if ( p[2]=='.' ) {
_CB( BMListCB )					if (!table[ BMListCB ]) {
							// p_prune will return on closing ')'
							p = p_prune( PRUNE_LIST, p ); }
						f_pop( stack, 0 )
						f_cls; break; }
					else {
_CB( BMRegisterVariableCB )			f_cls; p+=2; break; } }
				else {
_CB( BMWildCardCB )			f_cls; p++; break; }
			case '"':
_CB( BMFormatCB )		p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			case '\'':
_CB( BMCharacterCB )		p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			case '/':
_CB( BMRegexCB )		p = p_prune( PRUNE_FILTER, p );
				f_cls; break;
			default:
				if ( is_separator(*p) ) traverse_data->done = BMTraverseError;
				else {
_CB( BMIdentifierCB )			p = p_prune( PRUNE_FILTER, p );
					if ( *p=='~' ) {
_CB( BMSignalCB )				p++; }
					f_cls; break;
			} } }
	traverse_data->flags = flags;
	traverse_data->p = p;
	return p;
}

static BMCBTake
traverse_CB( BMCBName cb, BMTraverseData *traverse_data, char **p, int *f_ptr, int f_next )
{
	BMTraverseCB **table = (BMTraverseCB **) traverse_data->table;
	if (( table[ cb ] )) {
		BMCBTake take = table[cb]( traverse_data, p, *f_ptr, f_next );
		switch ( take ) {
		case BM_CONTINUE:
		case BM_DONE:
			return take;
		case BM_PRUNE_FILTER:
			*f_ptr |= INFORMED;
			*f_ptr &= ~NEGATED;
			switch ( cb ) {
			case BMDecoupleCB:
			case BMFilterCB:
				*p = p_prune( PRUNE_FILTER, *p+1 );
				break;
			default:
				*p = p_prune( PRUNE_FILTER, *p ); }
			return BM_DONE;
		case BM_PRUNE_TERM:
			switch ( cb ) {
			case BMDecoupleCB:
			case BMFilterCB:
				*p = p_prune( PRUNE_TERM, *p+1 );
				break;
			case BMTernaryOperatorCB:
				*f_ptr |= TERNARY;
				*p = p_prune( PRUNE_TERM, *p+1 );
				break;
			default:
				*p = p_prune( PRUNE_TERM, *p ); }
			return BM_DONE;
		default:
			return BM_CONTINUE; } }
	else return BM_CONTINUE;
}

