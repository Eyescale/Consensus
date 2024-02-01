/*===========================================================================
|
|	bm_traverse template - to be included in (user)_traversal.h
|
+==========================================================================*/
// Assumption: "traversal.h" already included
#include "traversal_flags.h"
#include "traversal_CB.h"

static inline char * prune_level( char *p );
static char *
bm_traverse( char *expression, BMTraverseData *traverse_data, int flags ) {
	int f_next, mode = traverse_data->done;
	traverse_data->done = 0;

	listItem **stack = traverse_data->stack;
	char *p = expression;

	// invoke BMTermCB - if it is set - first thing
	do {
CB_TermCB	} while ( 0 );

	while ( *p && !traverse_data->done ) {
		switch ( *p ) {
			case '>':
				if is_f( EENOV ) {
CB_EEnovEndCB				f_clr( EENOV )
					f_set( INFORMED ) }
				else if is_f( VECTOR ) {
					f_next = cast_i((*stack)->ptr);
CB_EndSetCB				f_pop( stack, 0 )
					f_set( INFORMED ) }
				else if ( p!=expression ) {
					traverse_data->done = 1;
					break; }
				else { // output symbol
					if ( p[1]=='&' ) p++; }
				p++; break;
			case '<':
				if is_f( INFORMED ) { // input or EENO
					if ( mode&TERNARY ) {
						f_clr( NEGATED|FILTERED|INFORMED )
						p++; break; }
					else {
						traverse_data->done=1;
						break; } }
CB_BgnSetCB			f_push( stack )
				f_reset( VECTOR|FIRST, 0 )
				p++;
CB_TermCB			break;
			case '!':
				if ( p[1]=='!' )
					p = p_prune( PRUNE_IDENTIFIER, p+2 );
				else if ( strmatch("?^",p[1]) ) {
CB_WildCardCB				f_set( INFORMED )
					p+=3; }
				else {
CB_EMarkCharacterCB			f_set( INFORMED )
					p++; }
				break;
			case '@':
				// Assumption: p[1]=='<' && p[2]=='\0'
CB_ActivateCB			p+=2; break;
			case '~':
				if ( p[1]=='<' ) {
CB_ActivateCB				p+=2; }
				else {
CB_NotCB				if is_f( NEGATED ) f_clr( NEGATED )
					else f_set( NEGATED )
					p++; }
				break;
			case '{':
CB_BgnSetCB			f_push( stack )
				f_reset( SET|FIRST, 0 )
				p++;
CB_TermCB			break;
			case '}':
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( !is_f(SET) ) {
					traverse_data->done = 1;
					break; }
				f_next = cast_i((*stack)->ptr);
CB_EndSetCB			f_pop( stack, 0 )
				p++; // move past '}'
CB_LoopCB			f_set( INFORMED )
				break;
			case '|':
				if ( !is_f(LEVEL|SET|CARRY) ) {
					traverse_data->done = 1;
					break; }
CB_BgnPipeCB			f_push( stack )
				f_reset( PIPED|FIRST, SET )
				p++; break;
			case '*':
				if ( p[1]=='^' ) {
CB_DereferenceCB			f_set( INFORMED )
					p = p_prune( PRUNE_TERM, p ); }
				if ( !is_separator(p[1]) || strmatch("*.%(?",p[1]) ) {
CB_DereferenceCB			f_clr( INFORMED )
					p++; }
				else {
CB_StarCharacterCB			f_set( INFORMED )
					p++; }
				break;
			case '%':
				if ( p[1]=='(' ) {
CB_SubExpressionCB			p++;
					if ( p[1]==':' )
						f_next = FIRST|SUB_EXPR|ASSIGN;
					else if ( !(mode&INFORMED) && !p_single(p) )
						f_next = FIRST|SUB_EXPR|COUPLE;
					else f_next = FIRST|SUB_EXPR;
					f_next |= is_f(SET);
CB_OpenCB				f_push( stack )
					f_reset( f_next, 0 )
					if ( *++p==':' ) p++;
					break; }
				else if ( p[1]=='<' ) {
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2;
					if ( strmatch( "?!(.", *p ) ) {
						f_set( EENOV )
						p = p_prune( PRUNE_TERM, p ); }
					break; }
				else if ( strmatch( "?!|%@", p[1] ) ) {
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2;
					if ( *p=='^' ) {
						p = p_prune( PRUNE_TERM, p+1 ); }
					else if ( *p=='~' && p[1]!='<' ) {
CB_SignalCB					p++; }
					break; }
				else {
CB_ModCharacterCB			f_set( INFORMED )
					p++; break; }
				break;
			case '^':
CB_RegisterVariableCB		p+=2; // assuming ^^
				break;
			case '(':
				if ( p[1]==':' ) {
					if ( (mode&LITERAL) && !is_f(SUB_EXPR)) {
CB_LiteralCB					f_set( INFORMED )
						break; }
					f_next = FIRST|LEVEL|ASSIGN; }
				else if ( !(mode&INFORMED) && !p_single(p) )
					f_next = FIRST|LEVEL|COUPLE;
				else f_next = FIRST|LEVEL;
				f_next |= is_f(SET|SUB_EXPR|MARKED);
CB_OpenCB			f_push( stack )
				f_reset( f_next, 0 )
				if ( p[1]==':' ) { p+=2; break; }
				else {	p++;
CB_TermCB				break; }
			case ':':
				if ( p[1]=='<' ) {
					f_clr( NEGATED|INFORMED|FILTERED )
					p++; break; }
				else {
CB_FilterCB				f_clr( NEGATED|INFORMED )
					p++; break; }
				break;
			case ',':
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( !is_f(VECTOR|SET|SUB_EXPR|LEVEL) ) {
					traverse_data->done = 1;
					break; }
CB_DecoupleCB			if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
				f_clr( NEGATED|FILTERED|INFORMED )
				p++;
CB_TermCB			break;
			case ')':
				if ( p[1]=='(' ) { // skip )(_)
					 if ( !(mode&TERNARY) ) {
						p = p_prune( PRUNE_TERM, p+1 );
						break; } }
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( !is_f(VECTOR|SET|SUB_EXPR|LEVEL) ) {
					traverse_data->done = 1;
					break; }
				f_next = cast_i((*stack)->ptr);
CB_CloseCB			f_pop( stack, 0 );
				p++; // move past ')'
				if ( *p=='^' ) {
CB_NewBornCB				p++; }
CB_LoopCB			f_set( INFORMED )
				break;
			case '?':
				if is_f( INFORMED ) {
					if ( !is_f(SET|LEVEL|SUB_EXPR) ) {
						traverse_data->done=1;
						break; }
					f_set( TERNARY )
CB_TernaryOperatorCB			f_clr( NEGATED|FILTERED|INFORMED ) }
				else {
CB_WildCardCB				if ( !strncmp( p+1, ":...", 4 ) )
						p+=4;
					f_set( INFORMED ) }
				p++; break;
			case '.':
				if ( p[1]=='(' ) {
CB_DotExpressionCB			p++;
					f_next=FIRST|DOT|LEVEL|is_f(SET|SUB_EXPR|MARKED);
					if (!(mode&INFORMED) && !p_single(p))
						f_next |= COUPLE;
CB_OpenCB				f_push( stack )
					f_reset( f_next, 0 )
					p++;
CB_TermCB				break; }
				else if ( p[1]=='?' ) {
CB_DotIdentifierCB			p+=2;
					f_set( INFORMED )
					break; }
				else if ( !is_separator(p[1]) ) {
CB_DotIdentifierCB			p = p_prune( PRUNE_FILTER, p+2 );
					f_set( INFORMED )
					break; }
				else if ( p[1]=='.' ) {
					if ( p[2]=='.' ) {
						/* Assumption: p[3]==')'
						   Starting with cases
							%(list,...)
							%(list,?:...)
							%((?,...):list)
							.proto:(_,...)
							.proto:(_(.param,...)_) */
						if ( is_f(SUB_EXPR)||(mode&(SUB_EXPR|FILTERED))) {
							f_set( ELLIPSIS )
CB_WildCardCB						p+=3; }
						else {	// instantiating or deternarizing
							if ( p[4]==':' ) { // ((list,...):_:)
CB_ListCB							f_pop( stack, 0 ) }
							else if ( mode&TERNARY ) {
								p+=3; }
							else { // ((list,...),xpan)
								f_set( ELLIPSIS )
								f_pop( stack, ELLIPSIS )
								p+=4; } }
						f_set( INFORMED )
						break; }
					else {
CB_RegisterVariableCB				f_set( INFORMED )
						p+=2; break; } }
				else {
CB_WildCardCB				f_set( INFORMED )
					p++; break; }
			case '"':
CB_FormatCB			p = p_prune( PRUNE_FILTER, p );
				f_set( INFORMED )
				break;
			case '\'':
CB_CharacterCB			p = p_prune( PRUNE_FILTER, p );
				f_set( INFORMED )
				break;
			case '/':
CB_RegexCB			p = p_prune( PRUNE_FILTER, p );
				f_set( INFORMED )
				break;
			default:
				if ( is_separator(*p) ) {
					traverse_data->done = BMTraverseError; }
				else {
CB_IdentifierCB				p = p_prune( PRUNE_IDENTIFIER, p );
					if ( *p=='~' && p[1]!='<' ) {
CB_SignalCB					p++; }
					f_set( INFORMED )
					break; } } }
	switch ( traverse_data->done ) {
	case BMTraverseError:
		fprintf( stderr, ">>>>> B%%: Error: bm_traverse: syntax error "
			"in expression: %s, at %s\n", expression, p ); }
	traverse_data->flags = flags;
	traverse_data->p = p;
	return p; }

static inline char * prune_level( char *p ) {
	p = p_prune( PRUNE_TERM, p );
	for ( ; ; ) {
		switch ( *p ) {
		case '|':
			if ( *++p=='{' ) break;
			// no break
		case '(':
			p = p_prune( PRUNE_TERM, p );
			break;
		case '{':
			do p = p_prune( PRUNE_TERM, p+1 );
			while ( *p!='}' );
			p++; break;
		default:
			return p; } } }

