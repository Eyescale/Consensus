#include <stdio.h>
#include <stdlib.h>

#include "parser_private.h"
#include "traverse.h"
#include "parser.h"

//===========================================================================
//	bm_parse
//===========================================================================
/*
		 B% Narrative File Format Interface

   used in either: a) mode==BM_STORY; or b) mode==BM_LOAD or mode==BM_INPUT,
   in which case (b) the relevant macros allow bm_parse() to filter out the
   following expressions:

	*expr		dereferencing
	%( )		sub_expr
	expr : expr	filter
	.		wildcard
	?		wildcard
	~		negation
	<		input
	>		output
*/
char *
bm_parse( int event, BMParseData *data, BMParseMode mode, BMParseCB _CB )
{
	CNParser *parser = data->parser;

	// shared by reference
	listItem **	stack	= &data->stack.flags;
	CNString *	s	= data->string;
	int *		tab	= data->tab;
	int *		type	= &data->type;

	// shared by copy
	int		flags	= data->flags;

	//----------------------------------------------------------------------
	// bm_parse:	Parser Begin
	//----------------------------------------------------------------------
	BMParseBegin( parser )
CND_if_( data->opt, EXPR_BGN )
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
			on_( '%' )	do_( "cmd" )	REENTER
							TAB_BASE = column;
			on_separator	; // err
			on_other	do_( "cmd" )	s_take
							TAB_BASE = column;
			end
} else {	bgn_
			on_( EOF )	do_( "" )
			on_( '#' )	do_( "#" )
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
						s_reset( CNStringAll )
		on_other	do_( "cmd" )	REENTER
						s_reset( CNStringAll )
		end
	in_( "cmd_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	; // err
		on_other	do_( "_expr" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						s_reset( CNStringAll )
		end
	in_( "_expr" )
if ( _CB( OccurrenceAdd, mode, data ) ) {
		bgn_
			on_any	do_( "expr" )	REENTER
						TAB_LAST = TAB_CURRENT;
						data->opt = 1;
			end
}
EXPR_BGN:CND_endif
	//----------------------------------------------------------------------
	// bm_parse:	Expression
	//----------------------------------------------------------------------
	in_( "expr" ) bgn_
		on_( '\n' ) if ( is_f(INFORMED) && !is_f(LEVEL|SET|SUB_EXPR) ) {
				do_( "expr_" )	REENTER }
			else if ( is_f(SET) ) { // allow \nl inside { }
				do_( "_^" ) }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) || is_f(TERNARY) )
					; // err
			else if ( are_f(FIRST|INFORMED) && is_f(LEVEL|SUB_EXPR) ) {
				if ( *type&DO && is_f(LISTABLE) && !is_f(SUB_EXPR|NEGATED) ) {
					do_( "," )	s_take
							f_clr( FIRST|INFORMED ) }
				else {	do_( same )	s_take
							f_clr( FIRST|INFORMED ) } }
			else if ( is_f(SET) ) {
					do_( "," ) }
CND_if_( mode==BM_STORY, A )
		ons( " \t" ) if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	REENTER }
			else {	do_( same ) }
		ons( "*%" ) if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_set( INFORMED ) }
		on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, 0 )
						f_set( INFORMED ) }
A:CND_else_( B )
		ons( " \t" )	do_( same )
		on_( '*' ) if ( !is_f(INFORMED) ) { do_( "*" )	s_take }
		on_( '%' ) if ( !is_f(INFORMED) ) { do_( "%" )	s_take }
		on_( '~' ) if ( !is_f(INFORMED) ) { do_( "~" ) }
		on_( '/' ) if ( !is_f(INFORMED) && !(*type&DO) ) {
				do_( "/" )	s_take }
		on_( '>' ) if ( s_empty && *type&DO ) {
				do_( ">" )	s_take
						*type = (*type&ELSE)|OUTPUT; }
		on_( '.' ) if ( !is_f(INFORMED) ) {
				do_( "." ) 	s_take }
		on_( '?' ) if ( are_f(TERNARY|INFORMED) ) {
				do_( "(_?" )	s_take
						f_push( stack )
						f_clr( FIRST|INFORMED|FILTERED ) }
			else if ( are_f(FIRST|INFORMED) && is_f(LEVEL|SUB_EXPR) && !is_f(COMPOUND) ) {
				do_( "(_?" )	s_take
						f_clr( INFORMED|FILTERED )
						f_set( TERNARY ) }
			else if ( !is_f(INFORMED|MARKED|NEGATED) && (*type&(IN|ON)||is_f(SUB_EXPR)) ) {
				do_( same )	s_take
						f_tag( stack, MARKED )
						f_set( MARKED|INFORMED ) }
		on_( ':' ) if ( s_empty ) {
					do_( same )	s_take
							f_set( ASSIGN ) }
			else if ( are_f(ASSIGN|INFORMED) && !is_f(LEVEL|SUB_EXPR) ) {
				if ( !is_f(FILTERED) ) {
					do_( same )	s_take
							f_clr( INFORMED )
							f_set( FILTERED ) } }
			else if ( are_f(TERNARY|INFORMED) ) {
				if ( !is_f(FILTERED) ) {
					do_( "(_?_:" )	s_take
							f_clr( INFORMED )
							f_set( FILTERED ) }
				else if ( !is_f(FIRST) ) {
					do_( same )	REENTER
							f_pop( stack, 0 ) } }
			else if ( is_f(INFORMED) && !is_f(COMPOUND) ) {
					do_( ":" )	s_take
							f_clr( INFORMED )
							f_set( FILTERED ) }
		on_( ')' ) if ( is_f(LEVEL|SUB_EXPR) && (is_f(TERNARY)?is_f(FILTERED):is_f(INFORMED)) ) {
					do_( same )	s_take
				if ( is_f(TERNARY) ) {	while (!is_f(FIRST)) f_pop( stack, 0 ) }
							f_pop( stack, 0 )
							f_set( INFORMED ) }
B:CND_endif
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_( '{' ) if ( *type&DO && is_f(LEVEL|SET) &&
				!is_f(INFORMED|ASSIGN|FILTERED|SUB_EXPR|NEGATED) ) {
				do_( same )	s_take
						f_push( stack )
						f_clr( LEVEL )
						f_set( SET ) }
		on_( '}' ) if ( are_f(INFORMED|SET) && !is_f(LEVEL|SUB_EXPR) ) {
				do_( "}" )	s_take
						f_pop( stack, 0 )
						f_tag( stack, COMPOUND )
						f_set( INFORMED|COMPOUND ) }
		on_( '|' ) if ( *type&DO && is_f(INFORMED) && is_f(LEVEL|SET) &&
				!is_f(ASSIGN|NEGATED|FILTERED|SUB_EXPR) ) {
				do_( "|" )	s_take
						f_tag( stack, COMPOUND )
						f_clr( INFORMED ) }
		on_( '#' )	do_( "_#" )
		on_separator	; // err
		on_other if ( !is_f(INFORMED) ) {
				do_( "term" )	s_take }
		end
	//----------------------------------------------------------------------
	// bm_parse:	Expression sub-states
	//----------------------------------------------------------------------
CND_ifn( mode==BM_STORY, C )
	in_( "." ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_( '(' )	do_( "expr" )	REENTER
		on_separator	; // err
		on_other	do_( "term" )	s_take
		end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)}\n" )	do_( "expr" )	REENTER
						f_set( INFORMED )
		ons( "{~" )	; //err
		on_other	do_( "expr" )	REENTER
		end
	in_( "%" ) bgn_
		on_( '(' )	do_( "expr" )	s_take
						f_push( stack )
						f_reset( FIRST|SUB_EXPR, 0 )
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
		on_( '.' ) if ( s_empty || ( is_f(ASSIGN|FILTERED) && !is_f(LEVEL|SUB_EXPR) ) ) {
				do_( "expr" )	REENTER
						s_add( "~" ) }
		on_( '(' )	do_( "expr" )	REENTER
				if ( s_empty ) {
						s_add( "~" ) }
				else {		s_add( "~" )
						f_set( NEGATED ) }
		ons( "{}?" )	; //err
		on_other	do_( "expr" )	REENTER
						s_add( "~" )
		end
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '/' ) 	do_( "/" )	s_take
		on_( '<' ) if ( !is_f(LEVEL|SUB_EXPR|SET|ASSIGN) ) {
				do_( ":_<" )	s_take }
		on_( '\"' ) if ( *type&DO && !is_f(LEVEL|SUB_EXPR|SET|ASSIGN) ) {
				do_( ":\"" )	s_take }
		on_( ')' ) if ( are_f(TERNARY|FILTERED) ) {
				do_( "expr" )	REENTER
						f_set( INFORMED ) }
			else {	do_( "expr" )	REENTER }
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
	in_( "(_?" ) bgn_ // Assumption: TERNARY is set, not INFORMED
		ons( " \t" )	do_( same )
		on_( ':' )	do_( "(_?:" )	s_take
						f_set( FILTERED )
		on_other	do_( "expr" )	REENTER
		end
		in_( "(_?:" ) bgn_
			ons( " \t" )	do_( same )
			on_( ')' )	; // err
			on_other	do_( "expr" )	REENTER
			end
	in_( "(_?_:" ) bgn_ // Assumption: TERNARY|FILTERED are set, not INFORMED
		ons( " \t" )	do_( same )
		on_( ':' )	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "expr" )	REENTER
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
C:CND_endif
	in_( "_^" ) bgn_	// \nl allowed inside { }
		ons( " \t\n" )	do_( same )
		on_( '#' )	do_( "_#" )
		on_( '}' )	do_( "expr" )	REENTER
		on_( ',' )	; // err
		on_other // allow newline to act as comma
			if ( is_f(INFORMED) && !is_f(LEVEL|SUB_EXPR) ) {
				do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
			else {	do_( "expr" )	REENTER }
		end
	in_( "_#" ) bgn_
		on_( '\n' )	do_( "expr" )	REENTER
		on_other	do_( same )
		end
	in_( "(" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' ) if ( *type&DO && !is_f(SUB_EXPR|NEGATED) ) {
				do_( "(:" )	s_take
						f_set( LITERAL ) }
		on_( '(' )	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( SET|TERNARY )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( FIRST|LEVEL|LISTABLE|CLEAN ) }
				else {		f_set( FIRST|LEVEL|CLEAN ) }
		on_other	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( SET|TERNARY )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( FIRST|LEVEL|LISTABLE ) }
				else {		f_set( FIRST|LEVEL ) }
		end
	in_( "," ) bgn_
		ons( " \t" )	do_( same )
		on_( '}' )	do_( "expr" )	REENTER
		on_( '.' ) if ( is_f(LEVEL|SUB_EXPR) ) {
				do_( ",." ) }
			else {	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
		on_other if ( is_f(LEVEL|SUB_EXPR) ) {
				do_( "expr" )	REENTER }
			else {	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
		end
		in_( ",." ) bgn_
			on_( '.' )	do_( ",.." )	s_add( ".." )
			on_other
if ( mode==BM_STORY ) {			do_( "." )	REENTER }
else 					; // err
			end
			in_( ",.." ) bgn_
				on_( '.' )	do_( ",..." )	s_take
				end
			in_( ",..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' )	do_( ",...)" )	s_take
								f_pop( stack, 0 )
				end
			in_( ",...)" ) bgn_
				on_( ':' )	do_( "(:" )	s_take
				end
	in_( "term" ) bgn_
CND_ifn( mode==BM_STORY, D )
		on_( '~' ) if ( *type&DO && !is_f(LEVEL|SUB_EXPR|SET|ASSIGN) ) {
				do_( "expr" )	s_take
						f_set( INFORMED ) }
D:CND_endif
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( same )	s_take
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
	in_( "(:" ) bgn_
		on_( ')' ) if ( is_f(LITERAL) ) {
				do_( "expr" )	s_take
						f_tag( stack, COMPOUND )
						f_set( INFORMED ) }
		on_( '\\' )	do_( "(:\\" )	s_take
		on_( '%' )	do_( "(:%" )	s_take
		on_( ':' ) if ( is_f(LITERAL) ) {
				do_( same )	s_take }
			else {	do_( "(::" )	s_take }
		on_( '\n' )	; // err
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
		in_( "(::" ) bgn_
			on_( ')' )	do_( "expr" )	REENTER
							f_tag( stack, COMPOUND )
							f_set( INFORMED )
			on_other	do_( "(:" )	REENTER
			end
	in_( "}" ) bgn_
		ons( " \t" )	do_( same )
		ons( ",)" )	do_( "expr" )	REENTER
		end
	in_( "|" ) bgn_
		ons( " \t" )	do_( same )
		ons( "({" )	do_( "expr" )	REENTER
						f_set( COMPOUND )
		end
	//----------------------------------------------------------------------
	// bm_parse:	Expression End
	//----------------------------------------------------------------------
	in_( "expr_" )
_CB( ExpressionTake, mode, data );
		bgn_
			on_any
if ( mode==BM_INPUT ) {		do_( "" ) }
else if ( is_f(ASSIGN) ? is_f(FILTERED) : !( *type&DO && is_f(FILTERED) ) ) {
				do_( "base" )	f_reset( FIRST, 0 );
						TAB_CURRENT = 0;
						data->opt = 0;
	if ( mode==BM_STORY ) {			*type = 0; } }
			end
	//----------------------------------------------------------------------
	// bm_parse:	Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
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
		in_( "expr_" )		errnum = ErrInstantiationFiltered;
		in_other bgn_
			on_( '\n' )	errnum = ErrUnexpectedCR;
			on_( ' ' )	errnum = ErrSpace;
			on_( '?' )	errnum = ( is_f(NEGATED) ? ErrMarkNegated :
						   is_f(MARKED) ? ErrMarkMultiple :
						   ErrSyntaxError );
			on_other	errnum = ErrSyntaxError;
			end
	//----------------------------------------------------------------------
	// bm_parse:	Parser End
	//----------------------------------------------------------------------
	BMParseEnd
	if ( errnum ) bm_parse_report( data, errnum, mode );
	data->flags = flags;
	return state;
}

//===========================================================================
//	bm_parse_report
//===========================================================================
void
bm_parse_report( BMParseData *data, BMParseErr errnum, BMParseMode mode )
{
	CNParser *parser = data->parser;
	char *src = (mode==BM_LOAD)?(mode==BM_INPUT)?"bm_input":"bm_load":"bm_read";

if ( mode==BM_INPUT ) {
	char *s = StringFinish( data->string, 0 );
	switch ( errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: unknown state \"%s\" <<<<<<\n", parser->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: %s: in expression '%s' unexpected EOF\n", src, s  ); break;
	case ErrUnexpectedCR:
		fprintf( stderr, "Error: %s: in expression '%s' unexpected \\cr\n", src, s  ); break;
	default:
		fprintf( stderr, "Error: %s: in expression '%s' syntax error\n", src, s  ); break;
	}
}
else {
	char *s = StringFinish( data->string, 0 );
	int l = parser->line, c = parser->column;
	switch ( errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: l%dc%d: unknown state \"%s\" <<<<<<\n", l, c, parser->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: %s: l%d: unexpected EOF\n", src, l  ); break;
	case ErrSpace:
		fprintf( stderr, "Error: %s: l%dc%d: unexpected ' '\n", src, l, c  ); break;
	case ErrInstantiationFiltered:
		fprintf( stderr, "Error: %s: l%dc%d: instantiation must be unfiltered\n", src, l, c  ); break;
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
//	bm_parse_init
//===========================================================================
int
bm_parse_init( BMParseData *data, CNParser *parser, BMParseMode mode, char *state, FILE *file )
{
	cn_parser_init( parser, "base", file );
	data->parser = parser;
	data->TAB_LAST = -1;
	data->flags = FIRST;
	return 1;
}

//===========================================================================
//	cn_parser_init
//===========================================================================
int
cn_parser_init( CNParser *parser, char *state, FILE *stream )
{
	memset( parser, 0, sizeof(CNParser) );
	parser->state = state;
	parser->stream = stream;
	parser->line = 1;
	return 1;
}

//===========================================================================
//	cn_parser_getc
//===========================================================================
static int preprocess( int event, int *mode, int *buffer, int *skipped );

int
cn_parser_getc( CNParser *data )
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
#define COMMENT		0
#define BACKSLASH	1
#define STRING		2
#define QUOTE		3
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
		case '"':
			output = event;
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
			break;
		case '\'':
			output = event;
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( mode[ STRING ] )
					break;
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
			}
			break;
		case '/':
			switch ( mode[ QUOTE ] ) {
			case 0:
				if ( !mode[ STRING ] ) {
					mode[ COMMENT ] = 1;
					break;
				}
				output = event;
				break;
			case 1: // quote just turned on
				mode[ QUOTE ] = -1;
				output = event;
				break;
			default: // mode[ STRING ] excluded
				mode[ QUOTE ] = 0;
				output = event;
			}
			break;
		default:
			output = event;
			if ( event == '\n' ) {
				mode[ QUOTE ] = 0;
				break;
			}
			switch ( mode[ QUOTE ] ) {
			case 0: break;
			case 1:
				mode[ QUOTE ] = -1;
				break;
			case -4:
				if (!( event == 'x' )) {
					mode[ QUOTE ] = -1;
					break;
				}
				// no break
			default:
				mode[ QUOTE ]++;
				break;
			}
		}
	}
	return output;
}

