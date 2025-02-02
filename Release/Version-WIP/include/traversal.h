/*===========================================================================
|
|	bm_traverse template - to be included in (user)_traversal.h
|
+==========================================================================*/
// Assumption: "traversal.h" already included
#include "parser_flags.h"
#include "traversal_CB.h"

#define BASE !is_f(VECTOR|SET|SUB_EXPR|LEVEL) && !(mode&TERNARY)

static char *
bm_traverse( char *expression, BMTraverseData *traversal, int flags ) {
	int f_next, mode = traversal->done;
	traversal->done = 0;

	listItem **stack = traversal->stack;
	char *p = expression;

	// invoke BMTermCB - if it is set - first thing
	do {
CB_TermCB	} while ( 0 );
	while ( *p && !traversal->done ) {
		switch ( *p ) {
			case '^':
				if ( strmatch("^.",p[1]) ) {
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2; }
				else if ( p[1]=='?' ) {
					p=p_prune( PRUNE_TERM, p+1 );
					f_set( INFORMED ) }
				else p++;
				break;
			case '{':
				if ( is_f(INFORMED) && BASE ) {
					traversal->done = 1; // forescan
					break; }
CB_BgnSetCB			f_push( stack )
				f_reset( SET|FIRST, 0 )
				p++;
CB_TermCB			break;
			case '}':
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( BASE ) {
					traversal->done = 1;
					break; }
				f_next = cast_i((*stack)->ptr);
CB_EndSetCB			f_pop( stack, 0 )
				p++; // move past '}'
				if ( *p=='|' ) {
					f_set( INFORMED );
					break; }
				else {
CB_LoopCB				f_set( INFORMED )
					break; }
			case '|':
				if ( p[1]=='^' ) {
CB_TagCB				if ( p[2]=='.' ) p+=3;
					else p = p_prune( PRUNE_IDENTIFIER, p+2 );
					if ( *p=='~' ) p++; }
				else if is_f( PIPED ) {
CB_BgnPipeCB				f_clr( NEGATED|INFORMED|FILTERED )
					p++; }
				else if ( BASE && !(mode&CARRY) && (!(mode&ASSIGN) || p[1]==':')) {
					traversal->done = 1; }
				else {
CB_BgnPipeCB				f_push( stack )
					f_reset( PIPED|FIRST, SET )
					p++; }
				break;
			case '>':
				if is_f( EENOV ) {
CB_EEnovEndCB				f_clr( EENOV )
					f_set( INFORMED ) }
				else if is_f( VECTOR ) {
					f_next = cast_i((*stack)->ptr);
CB_EndSetCB				f_pop( stack, 0 )
					f_set( INFORMED ) }
				else if ( p!=expression ) {
					traversal->done = 1;
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
						traversal->done=1;
						break; } }
CB_BgnSetCB			f_push( stack )
				f_reset( VECTOR|FIRST, 0 )
				p++;
CB_TermCB			break;
			case '!':
				if ( p[1]=='!' )
					p = p_prune( PRUNE_IDENTIFIER, p+2 );
				else if ( p[1]=='?' ) {
CB_WildCardCB				f_set( INFORMED )
					p+=3; }
				else if ( p[1]=='^' ) {
CB_WildCardCB				f_set( INFORMED )
					p+= p[2]=='(' ? p[7]==':' ? 8 : 7 : 3; }
				else {
					f_set( INFORMED )
CB_EMarkCharacterCB			p++; }
				break;
			case '@':
				// Assumption: p[1]=='<' && p[2]=='\0'
CB_ActivateCB			p+=2; break;
			case '~':
				if ( p[1]=='<' ) {
CB_ActivateCB				p+=2; }
				else {
CB_NotCB				if is_f( NEGATED )
						f_clr( NEGATED )
					else	f_set( NEGATED )
					p++; }
				break;
			case '*':
				if ( p[1]=='^' ) {
CB_DereferenceCB			if ( p[2]=='%' ) p = p_prune( PRUNE_TERM, p );
					else p+=3; // Assumption: *^^
					f_set( INFORMED ) }
				else if ( !is_separator(p[1]) || strmatch("*.%(?",p[1]) ) {
CB_DereferenceCB			f_clr( INFORMED )
					p++; }
				else {
CB_StarCharacterCB			f_set( INFORMED )
					p++; }
				break;
			case '%':
				switch ( p[1] ) {
				case '(':
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
					break;
				case '<':
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2;
					if ( strmatch( "?!(.", *p ) ) {
						f_set( EENOV )
						p = p_prune( PRUNE_TERM, p ); }
					break;
				case '%':
				case '@':
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2; break;
				case '|':
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2;
					if ( *p==':' ) {
						p = p_prune( PRUNE_TERM, p+1 ); }
					break;
				case '?':
				case '!':
CB_RegisterVariableCB			f_set( INFORMED )
					p+=2;
					if ( (mode&LITERAL) && !is_f(SUB_EXPR)) {
						if ( *p=='~' && p[1]!='<' ) {
CB_SignalCB						p++; }
						else if( *p==':' && is_xpn_expr(p+1) ) {
							p = p_prune( PRUNE_TERM, p+1 ); } }
					break;
				default:
					if ( !is_separator( p[1] ) ) {
CB_RegisterVariableCB				f_set( INFORMED )
						p = p_prune( PRUNE_IDENTIFIER, p+1 ); }
					else {
CB_ModCharacterCB				f_set( INFORMED )
						p++; } }
				break;
			case '(':
				if ( is_f(INFORMED) && BASE ) {
					traversal->done = 1; // forescan
					break; }
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
				if ( p[1]==':' ) {
CB_TermCB				p+=2; break; }
				else {	p++;
CB_TermCB				break; }
			case ':':
				if ( p[1]=='<' ) {
					f_clr( NEGATED|INFORMED|FILTERED )
					p++; break; }
CB_FilterCB			if ( p[1]=='|' ) {
					if ( !is_f(PIPED) ) {
						f_push( stack )
						f_reset( PIPED|FIRST, SET ) }
					else f_clr( NEGATED|INFORMED|FILTERED )
					p+=2; break; }
				else {
					f_clr( NEGATED|INFORMED )
					p++; break; }
			case ',':
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( BASE ) {
					traversal->done = 1;
					break; }
CB_CommaCB			if is_f( SUB_EXPR|LEVEL ) f_clr( FIRST )
				f_clr( NEGATED|FILTERED|INFORMED )
				p++;
CB_TermCB			break;
			case ')':
				if ( p[1]=='(' && !(mode&TERNARY) ) {
					// ?:(_)(_) skip to last parenthesis
					p = p_prune( PRUNE_TERM, p+1 );
					p -= ( *p==')' && p[-1]=='|' ? 3 : 1 ); }
				if ( is_f(PIPED) && !is_f(SUB_EXPR|LEVEL) ) {
					f_next = cast_i((*stack)->ptr);
CB_EndPipeCB				f_pop( stack, 0 ) }
				if ( BASE ) {
					traversal->done = 1;
					break; }
				f_next = cast_i((*stack)->ptr);
CB_CloseCB			f_pop( stack, 0 );
				p++; // move past ')'
				if ( *p=='^' ) {
CB_NewBornCB				p+=(p[1]=='('?6:1); }
				if ( *p=='|' ) {
					f_set( INFORMED )
					break; }
CB_LoopCB			f_set( INFORMED )
				if ( !strncmp(p,":|",2) ) {
					if ( !is_f(PIPED) ) {
						f_push( stack )
						f_reset( PIPED|FIRST, SET ) }
					else f_clr( NEGATED|INFORMED|FILTERED )
					p+=2; }
				break;
			case '?':
				if is_f( INFORMED ) {
					if ( BASE ) {
						traversal->done=1;
						break; }
					f_set( TERNARY )
CB_TernaryOperatorCB			f_clr( NEGATED|FILTERED|INFORMED ) }
				else {
CB_WildCardCB				if ( !strncmp( p+1, ":...", 4 ) )
						p+=4; }
					f_set( INFORMED )
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
				else if ( !is_separator(p[1]) ) {
CB_DotIdentifierCB			p = p_prune( PRUNE_FILTER, p+2 );
					f_set( INFORMED )
					break; }
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
			case '&':
				p++; break;
			default:
				if ( is_separator(*p) ) {
					traversal->done = BMTraverseError; }
				else {
CB_IdentifierCB				p = p_prune( PRUNE_IDENTIFIER, p );
					if ( *p=='~' && p[1]!='<' ) {
CB_SignalCB					p++; }
					f_set( INFORMED )
					break; } } }
	if ( !*p && is_f(PIPED) ) do {
		f_next = cast_i((*stack)->ptr);
CB_EndPipeCB	f_pop( stack, 0 ) } while ( 0 );

	switch ( traversal->done ) {
	case BMTraverseError:
		fprintf( stderr, ">>>>> B%%: Error: bm_traverse: syntax error "
			"in expression: %s, at %s\n", expression, p ); }
	traversal->flags = flags;
	traversal->p = p;
	return p; }

