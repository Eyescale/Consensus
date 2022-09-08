#include <stdio.h>
#include <stdlib.h>

#include "flags.h"
#include "parser.h"
#include "parser_private.h"

//===========================================================================
//	bm_parse
//===========================================================================
/*
		 B% Narrative File Format Interface

   used in both readStory() and db_input(), in which case mode value and
   the relevant macros allow to filter out the following expressions:

	*expr		dereferencing
	%( )		sub_expr
	expr : expr	filter
	. ?		wildcard
	~		negation
	< >		input/output
*/
char *
bm_parse( int event, CNParserData *parser, BMParseMode mode, BMParseCB _CB )
{
	BMStoryData *data = parser->user_data;

	// shared by reference
	listItem **	stack	= &data->stack;
	CNString *	s	= data->string;
	int *		tab	= data->tab;
	int *		type	= &data->type;

	// shared by copy
	int		flags	= data->flags;

	//----------------------------------------------------------------------
	// bm_parse:	Parser Begin
	//----------------------------------------------------------------------
	CNParserBegin( parser )
	in_( "base" )
if ( mode==BM_STORY ) {
		bgn_
			on_( EOF )	do_( "" )
			on_( '\t' )	do_( same )	TAB_CURRENT++;
			on_( '\n' )	do_( same )	TAB_CURRENT = 0;
			on_( '.' ) 	do_( "^." )	s_take
							TAB_BASE = column;
			on_( '#' ) if ( !TAB_CURRENT ) {
					do_( "#" ) }
			on_( '+' ) if ( !TAB_CURRENT ) {
					do_( "+" )	TAB_SHIFT++; }
			on_( '-' ) if ( !TAB_CURRENT ) {
					do_( "-" )	TAB_SHIFT--; }
			on_( ':' ) if ( !TAB_CURRENT ) {
					do_( "def" )	TAB_SHIFT = 0; }
			on_( '%' )
	if ( _CB( isBaseNarrative, mode, data ) ) {
					do_( "cmd" )	REENTER
							TAB_BASE = column;
	}
			on_separator	; // err
			on_other	do_( "cmd" )	s_take
							TAB_BASE = column;
			end
} else {	bgn_
			on_( EOF )	do_( "" )
			ons( "#" )	do_( "#" )
			ons( " \t\n" )	do_( same )
			on_other	do_( "expr" )	REENTER
			end
}
	in_( "#" ) bgn_
		on_( '\n' )	do_( "base" )
		on_other	do_( same )
		end
CND_ifn( mode==BM_STORY, EXPR_BGN )
	in_( "+" ) bgn_
		on_( '+' )	do_( same )	TAB_SHIFT++;
		on_other	do_( "base" )	REENTER
		end
	in_( "-" ) bgn_
		on_( '-' )	do_( same )	TAB_SHIFT--;
		on_other	do_( "base" )	REENTER
		end
	in_( "^." ) bgn_
		on_separator	; // err
		on_other	do_( "^.$" )	s_take
		end
		in_ ( "^.$" ) bgn_
			ons( " \t" )	do_( "^.$_" )
			ons( "\n:" )	do_( "^.$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
		in_( "^.$_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '.' )	do_( "^." )	s_add( " " )
							s_take
							*type = LOCALE;
			on_( '\n' )	do_( "_expr" )	REENTER
							*type = LOCALE;
							TAB_CURRENT += TAB_SHIFT;
							f_set( INFORMED )
			on_( ':' ) if (!( *type==LOCALE || TAB_CURRENT )) {
					do_( ".id:" )	s_take
							TAB_SHIFT = 0; }
			end

	//----------------------------------------------------------------------
	// bm_parse:	Narrative Declaration
	//----------------------------------------------------------------------
	in_( ".id:" ) bgn_
		ons( " \t" )	do_( same )
		on_( '(' )
if ( _CB( NarrativeTake, mode, data ) ) {
				do_( "sub" )	REENTER
}
		end
		in_( "sub" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' ) if ( is_f(INFORMED) && !is_f(LEVEL) ) {
					do_( "def$_" )	REENTER
							f_clr( INFORMED ) }
			on_( '(' ) if ( !is_f(INFORMED) ) {
					do_( same )	s_take
							f_push( stack )
							f_clr( INFORMED )
							f_set( FIRST|LEVEL ) }
			on_( ',' ) if ( are_f(INFORMED|FIRST|LEVEL) ) {
					do_( same )	s_take
							f_clr( FIRST|INFORMED ) }
			on_( '.' ) if ( is_f(LEVEL) && !is_f(INFORMED) ) {
					do_( "sub." )	s_take }
			on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
					do_( same )	s_take
							f_pop( stack, 0 )
							f_set( INFORMED ) }
			on_separator	; // err
			on_other
				if ( is_f(LEVEL) && !is_f(INFORMED) ) {
					do_( "sub$" )	s_take }
			end
		in_( "sub." ) bgn_
			ons( " \t,)" )	do_( "sub" )	REENTER
							f_set( INFORMED )
			on_separator	; // err
			on_other	do_( "sub.$" )	s_take
			end
		in_( "sub.$" ) bgn_
			ons( " \t:," )	do_( "sub.$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
			in_( "sub.$_" ) bgn_
				ons( " \t" )	do_( same )
				on_( ':' )	do_( "sub.$:" )	s_take
				ons( ",)" )	do_( "sub" )	REENTER
								f_set( INFORMED )
				end
			in_( "sub.$:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '(' )	do_( "sub" )	REENTER
				on_separator	; // err
				on_other	do_( "sub" )	REENTER
				end
		in_( "sub$" ) bgn_
			on_separator	do_( "sub" )	REENTER
			on_other	do_( same )	s_take
							f_set( INFORMED )
			end
	in_( "def" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "def$_" )	REENTER
		on_( '(' )	do_( ".id:" )	REENTER
						s_add( ".this:" )
		on_separator	; // err
		on_other	do_( "def$" )	s_take
		end
	in_( "def$" ) bgn_
		ons( " \t\n" )
if ( _CB( NarrativeTake, mode, data ) ) {
				do_( "def$_" )	REENTER
}
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( "def$_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
if ( _CB( ProtoSet, mode, data ) ) {
				do_( "base" )	TAB_CURRENT = 0;
						TAB_LAST = -1;
}
		end

	//----------------------------------------------------------------------
	// bm_parse:	Narrative in / on / do / en / else Command
	//----------------------------------------------------------------------
	in_( "cmd" ) bgn_
		ons( " \t" ) if ( !s_cmp( "in" ) ) {
				do_( "cmd_" )	*type |= IN; }
			else if ( !s_cmp( "on" ) ) {
				do_( "cmd_" )	*type |= ON; }
			else if ( !s_cmp( "do" ) ) {
				do_( "cmd_" )	*type |= DO; }
			else if ( !s_cmp( "else" ) ) {
				do_( "else" )	*type = ELSE; }
		on_( '\n' )
			if ( !s_cmp( "else" ) && !(*type&ELSE) ) {
				do_( "else" )	REENTER
						*type = ELSE; }
		on_( '%' )	do_( "_expr" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						*type |= EN;
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( "else" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "_expr" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						f_set( INFORMED )
						s_clean( CNStringAll )
		on_other	do_( "cmd" )	REENTER
						s_clean( CNStringAll )
		end
	in_( "cmd_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	; // err
		on_other	do_( "_expr" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						s_clean( CNStringAll )
		end
	in_( "_expr" )
if ( _CB( OccurrenceAdd, mode, data ) ) {
	bgn_
		on_any	do_( "expr" )	REENTER
					TAB_LAST = TAB_CURRENT;
		end
}
EXPR_BGN:CND_endif
	//----------------------------------------------------------------------
	// bm_parse:	Expression
	//----------------------------------------------------------------------
	in_( "expr" ) bgn_
CND_if_( mode==BM_STORY, A )
		ons( " \t" ) if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	REENTER
						f_clr( COMPOUND ) }
			else {	do_( same ) }
		ons( "*%" ) if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_set( INFORMED ) }
		on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, COMPOUND )
						f_set( INFORMED ) }
A:CND_else_( B )
		ons( " \t" )	do_( same )
		on_( '*' ) if ( !is_f(INFORMED) ) { do_( "*" )	s_take }
		on_( '%' ) if ( !is_f(INFORMED) ) { do_( "%" )	s_take }
		on_( '~' ) if ( !is_f(INFORMED) ) { do_( "~" ) }
		on_( '.' ) if ( !is_f(INFORMED) ) { do_( "." ) }
		on_( '/' ) if ( !is_f(INFORMED) && !(*type&DO) ) {
				do_( "/" )	s_take }
		on_( '?' ) if ( !is_f(INFORMED|MARKED|NEGATED) &&
				( is_f(SUB_EXPR) || *type&(IN|ON) ) ) {
				do_( same )	s_take
						f_set( MARKED|INFORMED ) }
		on_( '>' ) if ( *type&DO && s_empty ) {
				do_( ">" )	s_take
						*type = (*type&ELSE)|OUTPUT; }
		on_( ':' ) if ( s_empty ) {
				do_( "expr" )	s_take
						f_set( ASSIGN ) }
			else if ( are_f(ASSIGN|INFORMED) && !is_f(LEVEL) ) {
				if ( !is_f(FILTERED) ) {
				do_( "expr" )	s_take
						f_clr( INFORMED )
						f_set( FILTERED ) } }
			else if ( is_f(INFORMED) && !is_f(COMPOUND) ) {
				do_( ":" )	s_take
						f_clr( INFORMED )
						f_set( FILTERED ) }
		on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
				do_( "pop" )	s_take
						f_pop( stack, COMPOUND )
						f_set( INFORMED ) }
B:CND_endif
		on_( '\n' ) if ( is_f(ASSIGN) && !is_f(FILTERED) )
				; // err
			else if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	REENTER
						f_clr( COMPOUND ) }
			else if ( is_f(SET) ) { // allow \nl inside { }
				do_( "_^" ) }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) )
				; // err
			else if ( are_f(FIRST|LEVEL|INFORMED) ) {
				do_( same )	s_take
						f_clr( FIRST|INFORMED ) }
			else if ( is_f( SET ) ) {
				do_( same )	s_take
						f_clr( INFORMED ) }
		on_( '|' ) if ( *type&DO && is_f(INFORMED) && is_f(LEVEL|SET) &&
				!is_f(ASSIGN|FILTERED|SUB_EXPR) ) {
				do_( "|" )	s_take
						f_clr( INFORMED ) }
		on_( '{' ) if ( *type&DO && is_f(LEVEL|SET) &&
				!is_f(INFORMED|ASSIGN|FILTERED|SUB_EXPR) ) {
				do_( same )	s_take
						f_push( stack )
						f_clr( LEVEL )
						f_set( SET ) }
		on_( '}' ) if ( are_f(INFORMED|SET) && !is_f(LEVEL) ) {
				do_( "}" )	s_take
						f_pop( stack, 0 )
						f_set( INFORMED|COMPOUND ) }
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_separator	; // err
		on_other if ( !is_f(INFORMED) ) {
				do_( "term" )	s_take }
		end
	//----------------------------------------------------------------------
	// bm_parse:	Expression sub-states
	//----------------------------------------------------------------------
CND_ifn( mode==BM_STORY, C )
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)}\n" )	do_( "expr" )	REENTER
						f_set( INFORMED )
		ons( "{" )	; //err
		on_other	do_( "expr" )	REENTER
		end
	in_( "%" ) bgn_
		on_( '(' )	do_( "expr" )	s_take
						f_push( stack )
						f_clr( MARKED|NEGATED|SET )
						f_set( FIRST|LEVEL|SUB_EXPR )
		ons( "?!" )
			if (!( *type&EN && s_empty )) {
				do_( "expr" )	s_take
						f_set( INFORMED ) }
		on_other
			if (!( *type&EN && s_empty )) {
				do_( "%_" )	REENTER }
		end
		in_( "%_" ) bgn_
			ons( " \t" )	do_( same )
			ons( ":,)\n" )	do_( "expr" )	REENTER
							f_set( INFORMED )
			end
	in_( "~" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( "expr" )
		on_( '.' ) if ( s_empty || ( is_f(ASSIGN) && !is_f(LEVEL) ) ) {
				do_( "expr" )	REENTER
						s_add( "~" ) }
		on_( '(' ) if ( s_empty ) {
				do_( "expr" )	REENTER
						s_add( "~" ) }
			else {	do_( "expr" )	REENTER
						s_add( "~" )
						f_set( NEGATED ) }
		ons( "{}?" )	; //err
		on_other	do_( "expr" )	REENTER
						s_add( "~" )
		end
	in_( "." ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						s_add( "." )
						f_set( INFORMED )
		on_( '(' )
_CB( ExpressionPush, mode, data );
				do_( "expr" )	REENTER
						f_clr( FIRST )
						f_set( DOT )
		on_separator	; // err
		on_other
_CB( ExpressionPush, mode, data );
				do_( "term" )	s_take
						f_clr( FIRST )
						f_set( DOT )
		end
	in_( ">" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' )	do_( ">:" )	s_take
		on_( '\"' )	do_( ">\"" )	s_take
		end
		in_( ">:" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	do_( "expr" )	REENTER
							f_set( INFORMED )
			ons( "{}" )	; //err
			on_other	do_( "expr" )	REENTER
			end
		in_( ">\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '\\' )	do_( ">\"\\" )	s_take
			on_( '\"' )	do_( ">_" )	s_take
			on_other	do_( same )	s_take
			end
		in_( ">\"\\" ) bgn_
			on_any		do_( ">\"" )	s_take
			end
		in_( ">_" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( ":" )	s_take
			on_( '\n' )	do_( "expr" )	REENTER
							f_set( INFORMED )
			end
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '/' ) 	do_( "/" )	s_take
		on_( '<' ) if ( !is_f(ASSIGN) ) {
				do_( ":_<" )	s_take }
		on_( '\"' ) if ( *type&DO && !is_f(LEVEL|SET|ASSIGN) ) {
				do_( ":\"" )	s_take }
		on_other	do_( "expr" )	REENTER
		end
		in_( ":~" ) bgn_
			ons( " \t" )	do_( same )
			on_( '~' )	do_( ":" )
			on_other	do_( "~" )	REENTER
			end
		in_( ":\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '\\' )	do_( ":\"\\" )	s_take
			on_( '\"' )	do_( ":_" )	s_take
			on_other	do_( same )	s_take
			end
		in_( ":\"\\" ) bgn_
			on_any		do_( ":\"" )	s_take
			end
		in_( ":_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '<' )	do_( ":_<" )	s_take
			end
		in_( ":_<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	do_( "expr" )	REENTER
							*type = (*type&ELSE)|INPUT;
							f_set( INFORMED )
			end
	in_( "/" ) bgn_
		on_( '/' )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_( '\\' )	do_( "/\\" )	s_take
		on_( '[' )	do_( "/[" )	s_take
		on_printable	do_( same )	s_take
		end
		in_( "/\\" ) bgn_
			ons( "nt0\\./[]" ) do_( "/" )	s_take
			on_( 'x' )	do_( "/\\x" )	s_take
			end
		in_( "/\\x" ) bgn_
			on_xdigit	do_( "/\\x_" )	s_take
			end
		in_( "/\\x_" ) bgn_
			on_xdigit	do_( "/" )	s_take
			end
		in_( "/[" ) bgn_
			on_( '\\' )	do_( "/[\\" )	s_take
			on_( ']' )	do_( "/" )	s_take
			on_printable	do_( same )	s_take
			end
		in_( "/[\\" ) bgn_
			ons( "nt0\\[]" ) do_( "/[" )	s_take
			on_( 'x' )	do_( "/[\\x" )	s_take
			end
		in_( "/[\\x" ) bgn_
			on_xdigit	do_( "/[\\x_" ) s_take
			end
		in_( "/[\\x_" ) bgn_
			on_xdigit	do_( "/[" )	s_take
			end
	in_( "pop" ) bgn_
		on_any 	if ( is_f(DOT) ) {
_CB( ExpressionPop, mode, data );
				do_( "expr" )	REENTER
						f_clr( DOT ) }
			else {	do_( "expr" )	REENTER }
		end
C:CND_endif
	in_( "_^" ) bgn_	// \nl allowed inside { }
		ons( " \t\n" )	do_( same )
		on_( '#' )	do_( "_^#" )
		on_( '}' )	do_( "expr" )	REENTER
		on_( ',' )	; // err
		on_other // allow newline to act as comma
			if ( is_f(INFORMED) && !is_f(LEVEL) ) {
				do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
			else {	do_( "expr" )	REENTER }
		end
		in_( "_^#" ) bgn_
			on_( '\n' )	do_( "_^" )
			on_other	do_( same )
			end
	in_( "(" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' ) if ( *type & DO ) {
				do_( "(:" )	s_take }
		on_other	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( DOT|SET )
						f_set( FIRST|LEVEL )
		end
	in_( "(:" ) bgn_
		on_( ')' )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_( '\\' )	do_( "(:\\" )	s_take
		on_( '%' )	do_( "(:%" )	s_take
		on_other	do_( same )	s_take
		end
		in_( "(:\\" ) bgn_
			on_( 'x' )	do_( "(:\\x" )	s_take
			on_other	do_( "(:" )	s_take
			end
		in_( "(:\\x" ) bgn_
			on_xdigit	do_( "(:\\x_" ) s_take
			end
		in_( "(:\\x_" ) bgn_
			on_xdigit	do_( "(:" )	s_take
			end
		in_( "(:%" ) bgn_
			ons( "% " )	do_( "(:" )	s_take
			on_separator	; // err
			on_other	do_( "(:" )	s_take
			end
	in_( "|" ) bgn_
		ons( " \t" )	do_( same )
		ons( "({" )	do_( "expr" )	REENTER
		end
	in_( "}" ) bgn_
		ons( " \t" )	do_( same )
		ons( ",)" )	do_( "expr" )	REENTER
		end
	in_( "char" ) bgn_
		on_( '\\' )	do_( "char\\" )	s_take
		on_printable	do_( "char_" )	s_take
		end
		in_( "char\\" ) bgn_
			on_escapable	do_( "char_" )	s_take
			on_( 'x' )	do_( "char\\x" ) s_take
			end
		in_( "char\\x" ) bgn_
			on_xdigit	do_( "char\\x_" ) s_take
			end
		in_( "char\\x_" ) bgn_
			on_xdigit	do_( "char_" )	s_take
			end
		in_( "char_" ) bgn_
			on_( '\'' )	do_( "expr" )	s_take
							f_set( INFORMED )
			end
	in_( "term" ) bgn_
CND_ifn( mode==BM_STORY, D )
		on_( '~' ) if ( *type&DO && !is_f(LEVEL|SET|ASSIGN|DOT) ) {
				do_( "expr" )	s_take
						f_set( INFORMED ) }
D:CND_endif
		on_separator
			if ( is_f(DOT) ) {
_CB( ExpressionPop, mode, data );
				do_( "expr" )	REENTER
						f_clr( DOT )
						f_set( INFORMED )
			}
			else {	do_( "expr" )	REENTER
						f_set( INFORMED ) }
		on_other	do_( same )	s_take
		end

	//----------------------------------------------------------------------
	// bm_parse:	Expression End
	//----------------------------------------------------------------------
	in_( "expr_" )
_CB( ExpressionTake, mode, data );
	bgn_
		on_any
if ( mode==BM_STORY ) {		do_( "base" )	f_reset( FIRST, COMPOUND );
						TAB_CURRENT = 0;
						*type = 0;
}
else if ( mode==BM_INI ) {	do_( "base" )	f_reset( FIRST, COMPOUND );
						TAB_CURRENT = 0;
}
else if ( mode==BM_INSTANCE )	do_( "" )
		end

	//----------------------------------------------------------------------
	// bm_parse:	Error Handling
	//----------------------------------------------------------------------
	CNParserDefault
		on_( EOF )		errnum= ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState;
		in_( "^:." ) bgn_
			ons( " \t\n" )	errnum = ErrNarrativeEmpty;
			on_other	errnum = ErrSyntaxError;
			end
		in_( "^:_" ) bgn_
			on_( '\n' )	errnum = ErrNarrativeDouble;
			on_other	errnum = ErrSyntaxError;
			end
		in_( "cmd" )		errnum = ErrUnknownCommand;
		in_( "_expr" )		errnum = ErrIndentation; column=TAB_BASE;
		in_( "sibling" )	errnum = ErrIndentation; column=TAB_BASE;
		in_( ":_" ) 		errnum = ErrInputScheme;
		in_( ":_<" ) 		errnum = ErrInputScheme;
		in_( ">" )		errnum = ErrOutputScheme;
		in_( ">_" )		errnum = ErrOutputScheme;
		in_( "?" )		errnum = ( is_f(NEGATED) ? ErrMarkNegated :
						   is_f(MARKED) ? ErrMarkMultiple :
						   ErrSyntaxError);
		in_other bgn_
			on_( '\n' )	errnum = ErrUnexpectedCR;
			on_( ' ' )	errnum = ErrSpace;
			on_other	errnum = ErrSyntaxError;
			end

	//----------------------------------------------------------------------
	// bm_parse:	Parser End
	//----------------------------------------------------------------------
	CNParserEnd

	if ( errnum ) {
		bm_parser_report( errnum, parser, mode );
	}
	data->flags = flags;
	return state;
}

//===========================================================================
//	bm_parser_report
//===========================================================================
//===========================================================================
//	bm_parser_report
//===========================================================================
void
bm_parser_report( BMParserError errnum, CNParserData *parser, BMParseMode mode )
{
	BMStoryData *data = parser->user_data;
	char *s = StringFinish( data->string, 0 );

if ( mode==BM_INSTANCE )
	switch ( errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: unknown state \"%s\" <<<<<<\n", parser->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: bm_read: in expression '%s' unexpected EOF\n", s  ); break;
	case ErrUnexpectedCR:
		fprintf( stderr, "Error: bm_read: in expression '%s' unexpected \\cr\n", s  ); break;
	default:
		fprintf( stderr, "Error: bm_read: in expression '%s' syntax error\n", s  ); break;
	}
else {
	char *	src = (mode==CN_INI) ? "bm_load": "bm_read";
	int 	l = parser->line, c = parser->column;
	switch ( errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: l%dc%d: unknown state \"%s\" <<<<<<\n", l, c, parser->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: %s: l%d: unexpected EOF\n", src, l  ); break;
	case ErrSpace:
		fprintf( stderr, "Error: %s: l%dc%d: unexpected ' '\n", src, l, c  ); break;
	case ErrUnexpectedCR:
		fprintf( stderr, "Error: %s: l%dc%d: statement incomplete\n", src, l, c  ); break;
	case ErrIndentation:
		fprintf( stderr, "Error: %s: l%dc%d: indentation error\n", src, l, c  ); break;
	case ErrExpressionSyntaxError:
		fprintf( stderr, "Error: %s: l%dc%d: syntax error in expression\n", src, l, c  ); break;
	case ErrInputScheme:
		fprintf( stderr, "Error: %s: l%dc%d: unsupported input scheme\n", src, l, c  ); break;
	case ErrOutputScheme:
		fprintf( stderr, "Error: %s: l%dc%d: unsupported output scheme\n", src, l, c  ); break;
	case ErrMarkMultiple:
		fprintf( stderr, "Error: %s: l%dc%d: '?' already informed\n", src, l, c  ); break;
	case ErrMarkNegated:
		fprintf( stderr, "Error: %s: l%dc%d: '?' negated\n", src, l, c  ); break;
	case ErrNarrativeEmpty:
		fprintf( stderr, "Error: %s: l%dc%d: empty narrative\n", src, l, c  ); break;
	case ErrNarrativeDouble:
		fprintf( stderr, "Error: %s: l%dc%d: narrative already defined\n", src, l, c  ); break;
	case ErrUnknownCommand:
		fprintf( stderr, "Error: %s: l%dc%d: unknown command '%s'\n", src, l, c, s  ); break;
	default:
		fprintf( stderr, "Error: %s: l%dc%d: syntax error\n", src, l, c  ); break;
	} }
}

//===========================================================================
//	cn_parser_init / cn_parser_getc
//===========================================================================
static int preprocess( int event, int *mode, int *buffer, int *skipped );
enum {
	COMMENT = 0,
	BACKSLASH,
	STRING,
	QUOTE
};

int
cn_parser_init( CNParserData *parser, char *state, FILE *stream, void *user_data )
{
	memset( parser, 0, sizeof(CNParserData) );
	parser->user_data = user_data;
	parser->state = state;
	parser->stream = stream;
	parser->line = 1;
	return 1;
}

int
cn_parser_getc( CNParserData *data )
/*
	filters out comments and \cr line continuation from input
*/
{
	int event, skipped[ 2 ] = { 0, 0 };
	int buffer = data->buffer;
	do {
		if ( buffer ) { event=buffer; buffer=0; }
		else event = fgetc( data->stream );
		event = preprocess( event, data->mode, &buffer, skipped );
	} while ( !event );
	data->buffer = buffer;
	if ( skipped[ 1 ] ) {
		data->line += skipped[ 1 ];
		data->column = skipped[ 0 ];
	} else {
		data->column += skipped[ 0 ];
	}
	return event;
}

static int
preprocess( int event, int *mode, int *buffer, int *skipped )
{
	int output = 0;
	if ( event == EOF )
		output = event;
	else if ( mode[ COMMENT ] ) {
		switch ( event ) {
		case '/':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 2;
				skipped[ 0 ]++;
			}
			else if ( mode[ COMMENT ] == 4 )
				mode[ COMMENT ] = 0;
			skipped[ 0 ]++;
			break;
		case '\n':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 0;
				output = '/';
				*buffer = '\n';
			}
			else if ( mode[ COMMENT ] == 2 ) {
				mode[ COMMENT ] = 0;
				output = '\n';
			}
			else {
				if ( mode[ COMMENT ] == 4 )
					mode[ COMMENT ] = 3;
				skipped[ 0 ] = 0;
				skipped[ 1 ]++;
			}
			break;
		case '*':
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 3;
				skipped[ 0 ]++;
			}
			else if ( mode[ COMMENT ] == 3 )
				mode[ COMMENT ] = 4;
			else if ( mode[ COMMENT ] == 4 )
				mode[ COMMENT ] = 3;
			skipped[ 0 ]++;
			break;
		default:
			if ( mode[ COMMENT ] == 1 ) {
				mode[ COMMENT ] = 0;
				output = '/';
				*buffer = event;
			}
			else {
				if ( mode[ COMMENT ] == 4 )
					mode[ COMMENT ] = 3;
				skipped[ 0 ]++;
			}
		}
	}
	else if ( mode[ BACKSLASH ] ) {
		if ( mode[ BACKSLASH ] == 4 ) {
			mode[ BACKSLASH ] = 0;
			output = event;
		}
		else switch ( event ) {
		case ' ':
		case '\t':
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 2;
				skipped[ 0 ]++;
			}
			skipped[ 0 ]++;
			break;
		case '\\':
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 4;
				output = '\\';
				*buffer = '\\';
			}
			else mode[ BACKSLASH ] = 1;
			break;
		case '\n':
			if ( mode[ BACKSLASH ] == 3 ) {
				mode[ BACKSLASH ] = 0;
				output = '\n';
			}
			else {
				mode[ BACKSLASH ] = 3;
				skipped[ 0 ] = 0;
				skipped[ 1 ]++;
			}
			break;
		default:
			if ( mode[ BACKSLASH ] == 1 ) {
				mode[ BACKSLASH ] = 4;
				output = '\\';	
				*buffer = event;
			}
			else {
				mode[ BACKSLASH ] = 0;
				*buffer = event;
			}
		}
	}
	else {
		switch ( event ) {
		case '\\':
			switch ( mode[ QUOTE ] ) {
			case 0:
				mode[ BACKSLASH ] = 1;
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -4;
				output = event;
				break;
			case -4: // case '\\
				mode[ QUOTE ] = -1;
				output = event;
				break;
			default:
				mode[ QUOTE ] = 0;
				mode[ BACKSLASH ] = 1;
				output = event;
			}
			break;
		case '/':
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( !mode[ STRING ] )
					mode[ COMMENT ] = 1;
				else
					output = event;
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -1;
				output = event;
				break;
			default:
				mode[ QUOTE ] = 0;
				if ( !mode[ STRING ] )
					mode[ COMMENT ] = 1;
				else
					output = event;
			}
			break;
		case '"':
			switch ( mode[ QUOTE ] ) {
			case 0:
				mode[ STRING ] = !mode[ STRING ];
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -1;
				break;
			case -4: // allowing '\"'
				mode[ QUOTE ] = -1;
				break;
			default:
				mode[ QUOTE ] = 0;
				mode[ STRING ] = !mode[ STRING ];
				break;
			}
			output = event;
			break;
		case '\'':
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( !mode[ STRING ] )
					mode[ QUOTE ] = 1;
				break;
			case 1: // allowing ''
				mode[ QUOTE ] = 0;
				break;
			case -4: // allowing '\''
				mode[ QUOTE ] = -1;
				break;
			default:
				mode[ QUOTE] = 0;
				if ( !mode[ STRING ] )
					mode[ QUOTE ] = 1;
			}
			output = event;
			break;
		default:
			if ( event == '\n' )
				mode[ QUOTE ] = 0;
			else
				switch ( mode[ QUOTE ] ) {
				case 0: break;
				case 1:
					mode[ QUOTE ] = -1;
					break;
				case -4:
					if ( event == 'x' )
						mode[ QUOTE ] = -3;
					else 
						mode[ QUOTE ] = -1;
					break;
				case -3:
				case -2:
				case -1:
					mode[ QUOTE ]++;
					break;
				}
			output = event;
		}
	}
	return output;
}

