#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "parser_macros.h"
#include "traverse.h"

//===========================================================================
//	bm_parse_init / bm_parse_exit
//===========================================================================
void
bm_parse_init( BMParseData *data, BMParseMode mode )
{
	data->string = newString();
	data->state = "base";
	data->TAB_LAST = -1;
	data->flags = FIRST;
	data->type = ( mode==BM_STORY ) ? 0 : DO;
}
void
bm_parse_exit( BMParseData *data )
{
	freeString( data->string );
}

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
bm_parse( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )
{
	CNIO *io = data->io;

	// shared by reference
	listItem **	stack	= &data->stack.flags;
	CNString *	s	= data->string;
	int *		tab	= data->tab;
	int *		type	= &data->type;

	// shared by copy
	char *		state	= data->state;
	int		flags	= data->flags;
	int		errnum	= 0;
	int		line	= io->line;
	int		column	= io->column;

	//----------------------------------------------------------------------
	// bm_parse:	Parser Begin
	//----------------------------------------------------------------------
	BMParseBegin( state, event, line, column )
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
			end }
else {		bgn_
			on_( EOF )	do_( "" )
			on_( '#' )	do_( "#" )
			ons( " \t\n" )	do_( same )
			on_other	do_( "expr" )	REENTER
			end }
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
CB_if_( NarrativeTake, mode, data ) {
				do_( "§" )	REENTER }
		end
		in_( "§" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' ) if ( is_f(INFORMED) && !is_f(LEVEL) ) {
					do_( "def$_" )	REENTER
							f_clr( INFORMED ) }
			on_( '(' ) if ( !is_f(INFORMED) ) {
					do_( same )	s_take
							f_push( stack )
							f_clr( INFORMED )
							f_set( FIRST|LEVEL ) }
			on_( '.' ) if ( is_f(LEVEL) && !is_f(INFORMED) ) {
					do_( "§." )	}
			on_( ',' ) if ( are_f(INFORMED|FIRST|LEVEL) ) {
					do_( same )	s_take
							f_clr( FIRST|INFORMED ) }
			on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
					do_( same )	s_take
							f_pop( stack, 0 )
							f_set( INFORMED ) }
			on_( '%' )	do_( "§%" )	s_take
			on_separator	; // err
			on_other
				if ( is_f(LEVEL) && !is_f(INFORMED) ) {
					do_( "§$" )	s_take }
			end
		in_( "§%" ) bgn_
			on_( '%' )	do_( "§" )	s_take
							f_set( INFORMED )
			end
		in_( "§." ) bgn_
			on_( '.' )	do_( "§.." )
			on_separator	do_( "§" )	REENTER
							s_add( "." )
							f_set( INFORMED )
			on_other	do_( "§.$" )	REENTER
							s_add( "." )
			end
			in_( "§.." ) bgn_
				on_( '.' ) if ( !is_f(FIRST) ) {
						do_( "§..." )	s_add( "..." )
								f_pop( stack, 0 )
								f_set( INFORMED ) }
				on_other	do_( "§" )	REENTER
								s_add( ".." )
								f_set( INFORMED )
				end
			in_( "§..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' ) if ( !is_f(LEVEL) ) {
						do_( "§" ) 	s_take }
				end
		in_( "§.$" ) bgn_
			ons( " \t:,)" )	do_( "§.$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
			in_( "§.$_" ) bgn_
				ons( " \t" )	do_( same )
				on_( ',' ) if ( is_f(FIRST) ) {
						do_( "§.$," )	s_take
								f_clr( FIRST ) }
				on_( ':' )	do_( "§.$:" )	s_take
				on_( ')' )	do_( "§" )	REENTER
								f_set( INFORMED )
				end
			in_( "§.$," ) bgn_
				ons( " \t" )	do_( same )
				on_( '.' )	do_( "§.$,." )
				on_other	do_( "§" )	REENTER
				end
				in_( "§.$,." ) bgn_
					on_( '.' )	do_( "§.$,.." )
					on_separator	do_( "§" )	REENTER
									s_add( "." )
									f_set( INFORMED )
					on_other	do_( "§.$" )	REENTER
									s_add( "." )
					end
				in_( "§.$,.." ) bgn_
					on_( '.' )	do_( "§" )	s_add( "..." )
									f_set( INFORMED )
					on_other	do_( "§" )	REENTER
									s_add( ".." )
									f_set( INFORMED )
					end
			in_( "§.$:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '(' )	do_( "§" )	REENTER
				on_separator	; // err
				on_other	do_( "§" )	REENTER
				end
		in_( "§$" ) bgn_
			on_separator	do_( "§" )	REENTER
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
CB_if_( NarrativeTake, mode, data ) {
				do_( "def$_" )	REENTER }
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( "def$_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
CB_if_( ProtoSet, mode, data ) {
				do_( "base" )	TAB_CURRENT = 0;
						TAB_LAST = -1; }
		end
	//----------------------------------------------------------------------
	// bm_parse:	Narrative en / in / on / do / per / else Command
	//----------------------------------------------------------------------
	in_( "cmd" ) bgn_
		ons( " \t" ) if ( !s_cmp( "en" ) ) {
				do_( "cmd_" )	*type |= EN; }
			else if ( !s_cmp( "in" ) ) {
				do_( "cmd_" )	*type |= IN; }
			else if ( !s_cmp( "on" ) ) {
				do_( "cmd_" )	*type |= ON; }
			else if ( !s_cmp( "do" ) ) {
				do_( "cmd_" )	*type |= DO; }
			else if ( !s_cmp( "per" ) ) {
				do_( "cmd_" )	*type |= PER|ON; }
			else if ( !s_cmp( "else" ) && !(*type&ELSE) ) {
				do_( "else" )	*type = ELSE; }
		on_( '\n' )
			if ( !s_cmp( "else" ) && !(*type&ELSE) ) {
				do_( "else" )	REENTER
						*type = ELSE; }
		on_( '%' )	do_( "%_" )	s_take
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
	in_( "%_" ) bgn_
		on_( '(' )	do_( "_expr" )	s_take
						f_push( stack )
						f_reset( FIRST|SUB_EXPR, 0 )
						TAB_CURRENT += TAB_SHIFT;
						*type |= EN;
		on_other	; // err
		end
	in_( "cmd_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	; // err
		on_other	do_( "_expr" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						s_reset( CNStringAll )
		end
	in_( "_expr" )
CB_if_( OccurrenceAdd, mode, data ) {
		bgn_
			on_any	do_( "expr" )	REENTER
						TAB_LAST = TAB_CURRENT;
						data->opt = 1;
			end }
EXPR_BGN:CND_endif
	//----------------------------------------------------------------------
	// bm_parse:	Expression
	//----------------------------------------------------------------------
	in_( "expr" ) bgn_
		on_( '\n' ) if ( is_f(INFORMED) && !is_f(LEVEL|SUB_EXPR|SET|CARRY|EENOV|VECTOR) &&
				( !(*type&PER) || *type&ON_X ) && ( !is_f(ASSIGN) || is_f(FILTERED) ) ) {
				do_( "expr_" )	REENTER }
			else if ( is_f(SET|CARRY|VECTOR) ) { // allow \nl inside resp. {} () <>
				do_( "_^" ) }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) || is_f(TERNARY) )
				; // err
			else if ( is_f(LEVEL|SUB_EXPR) ) {
				if ( !is_f(FIRST) )
					; // err
				else if ( is_f(LISTABLE) && !is_f(SUB_EXPR|NEGATED) ) {
					do_( "," )	f_clr( FIRST|INFORMED|FILTERED ) }
				else if ( is_f(SUB_EXPR) && !is_f(LEVEL|MARKED) ) {
					do_( "," )	f_clr( FIRST|INFORMED|FILTERED ) }
				else if ( are_f(SUB_EXPR|CLEAN|MARKED) && !is_f(FILTERED) ) {
					do_( "," )	f_clr( FIRST|INFORMED )
							f_set( LISTABLE ) }
				else {	do_( same )	s_take
							f_clr( FIRST|INFORMED|FILTERED ) } }
			else if ( is_f(SET|CARRY|VECTOR) ) {
				do_( "," ) }
			else if ( is_f(ASSIGN) ) {
				if ( !is_f(FILTERED) ) {
					do_( same )	s_take
							f_clr( INFORMED )
							f_set( FILTERED ) } }
			else if ( *type&DO )
				do_( "_," )
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
		on_( '@' ) if ( *type&DO && is_f(INFORMED) ) {
				do_( "@" )	s_take }
		on_( '~' ) if ( *type&DO && is_f(INFORMED) ) {
				do_( "@" )	s_take }
			else if ( !is_f(INFORMED) ) {
				do_( "~" ) }
		on_( '.' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "." ) 	s_take }
		on_( '*' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "*" )	s_take }
		on_( '%' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "%" ) }
		on_( ':' ) if ( s_empty ) {
				do_( same )	s_take
						f_set( ASSIGN ) }
			else if ( !is_f(INFORMED) || is_f(EENOV) || ( *type&PER && !s_diff("~.") ) )
				; // err
			else if ( is_f(ASSIGN) && !is_f(LEVEL|SUB_EXPR) ) {
				if ( !is_f(FILTERED) ) {
					do_( same )	s_add( "," )
							f_clr( INFORMED )
							f_set( FILTERED ) } }
			else if ( is_f(TERNARY) ) {
				if ( !is_f(FILTERED) ) {
					do_( "(_?_:" )	s_take
							f_clr( INFORMED )
							f_set( FILTERED ) }
				else if ( !is_f(FIRST) ) {
					do_( same )	REENTER
							f_pop( stack, 0 ) } }
			else if ( !is_f(COMPOUND) ) {
					do_( ":" )	f_clr( INFORMED )
							f_set( FILTERED ) }
		on_( '/' ) if ( !is_f(INFORMED) && ( !(*type&DO) || is_f(EENOV) ) ) {
				do_( "/" )	s_take }
		on_( '<' ) if ( is_f(SET|LEVEL|SUB_EXPR|EENOV|VECTOR) )
				; // err
			else if ( *type&ON && is_f(INFORMED) && (is_f(ASSIGN)?is_f(FILTERED):is_f(FIRST)) ) {
				do_( same )	s_take
						*type = (*type&~ON)|ON_X;
						f_clr( ASSIGN|INFORMED|NEGATED|FILTERED )
						f_set(FIRST) }
			else if ( *type&DO ) {
				if ( is_f(ELLIPSIS) && !is_f(NEGATED|FILTERED|INFORMED) ) {
					do_( same )	s_take
							f_set( VECTOR ) }
				else if ( is_f(ASSIGN) && !is_f(NEGATED|INFORMED) ) {
					do_( ":<" )	s_take
							f_set( VECTOR ) }
				else if ( is_f(INFORMED) && !is_f(ASSIGN|ELLIPSIS) ) {
					do_( "_<" )	s_take } }
			else if ( *type&OUTPUT && !is_f(NEGATED|FILTERED|INFORMED) ) {
				do_( same )	s_take
						f_set( VECTOR ) }
		on_( '>' ) if ( *type&DO && s_empty ) {
				do_( ">" )	s_take
						*type = (*type&ELSE)|OUTPUT; }
			else if ( are_f(EENOV|INFORMED) && !is_f(LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, 0 )
						f_set( INFORMED ) }
			else if ( *type&(DO|OUTPUT) && are_f(INFORMED|VECTOR) && !is_f(LEVEL|SUB_EXPR|EENOV) ) {
					do_( same )	s_take
							f_clr( VECTOR )
				if ( *type&OUTPUT ) {	f_set( COMPOUND ) } }
		on_( '!' ) if ( *type&DO && ( s_empty || (are_f(ASSIGN|FILTERED) &&
				!is_f(INFORMED|LEVEL|SUB_EXPR|SET|NEGATED)) ) ) {
				do_( "!" )	s_take }
			else if ( are_f(EENOV|SUB_EXPR) && !is_f(EMARKED|INFORMED|NEGATED) ) {
				do_( same )	s_take
						f_tag( stack, EMARKED )
						f_set( EMARKED|INFORMED ) }
		on_( '?' ) if ( is_f(INFORMED) ) {
				if ( is_f(TERNARY) ) {
					do_( "(_?" )	s_take
							f_push( stack )
							f_clr( FIRST|INFORMED|FILTERED ) }
				else if ( is_f(FIRST) && is_f(LEVEL|SUB_EXPR) && !is_f(COMPOUND|EENOV) ) {
					do_( "(_?" )	s_take
							f_clr( INFORMED|FILTERED )
							f_set( TERNARY ) } }
			else if ( !is_f(MARKED|NEGATED) ) {
				if ( is_f(EENOV) ) {
					do_( same )	s_take
							f_set( MARKED|INFORMED ) }
				else if ( is_f(SUB_EXPR) || ( *type&ON_X ? !is_f(LEVEL|SET) : *type&(IN|ON) ) ) {
					do_( same )	s_take
							f_tag( stack, MARKED )
							f_set( MARKED|INFORMED ) } }
		on_( ')' ) if ( is_f(EENOV) ) {
				if ( is_f(INFORMED) && ( is_f(LEVEL) || are_f(SUB_EXPR|EMARKED) ) ) {
					do_( same )	s_take
							f_pop( stack, MARKED )
							f_set( INFORMED ) } }
			else if ( is_f(LEVEL|SUB_EXPR) && (is_f(TERNARY)?is_f(FILTERED):is_f(INFORMED)) ) {
					do_( same )	s_take
				if ( is_f(TERNARY) ) {	while (!is_f(FIRST)) f_pop( stack, 0 ) }
							f_pop( stack, 0 )
							f_set( INFORMED ) }
			else if ( is_f(ELLIPSIS) && is_f(INFORMED) && !is_f(LEVEL|SUB_EXPR|VECTOR) ) {
					do_( same )	s_take
							f_pop( stack, 0 )
							f_set( INFORMED ) }
			else if ( is_f(CARRY) && !is_f(LEVEL|SUB_EXPR) ) {
					do_( same )	s_take
							f_clr( CARRY )
							f_set( INFORMED|COMPOUND ) }
B:CND_endif
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_( '{' ) if ( is_f(INFORMED|SUB_EXPR|NEGATED|FILTERED|VECTOR|EENOV|ELLIPSIS) )
				; // err
			else if ( *type&DO && !is_f(ASSIGN) ) {
				do_( same )	s_take
						f_push( stack )
						f_clr( LEVEL )
						f_set( SET ) }
			else if ( *type&ON_X && !is_f(LEVEL|SET) ) {
				do_( same )	s_take
						f_set( SET ) }
		on_( '}' ) if ( !are_f(INFORMED|SET) || is_f(LEVEL|SUB_EXPR) )
				; // err
			else if ( *type&ON_X ) {
				do_( same )	s_take
						f_clr( SET )
						f_set( COMPOUND ) }
			else {	do_( same )	s_take
						f_pop( stack, 0 )
						f_tag( stack, COMPOUND )
						f_set( INFORMED|COMPOUND ) }
		on_( '|' ) if ( *type&DO && is_f(INFORMED) && is_f(LEVEL|SET) &&
				!is_f(ASSIGN|FILTERED|SUB_EXPR|NEGATED|VECTOR) ) {
				do_( "|" )	s_take
						f_tag( stack, COMPOUND )
						f_clr( INFORMED ) }
		on_( '#' )	do_( "_#" )
		on_separator	; // err
		on_other if ( !is_f(INFORMED) ) {
				if ( mode==BM_STORY && *type&DO && ( is_f(LEVEL) ?
					is_f(TERNARY) && f_signal_authorize(stack) :
					!is_f(NEGATED|SUB_EXPR|SET|ASSIGN|EENOV) ) ) {
					do_( "term_" )	s_take } // signalable
				else {	do_( "term" )	s_take } }
		end
	//----------------------------------------------------------------------
	// bm_parse:	Expression sub-states
	//----------------------------------------------------------------------
CND_ifn( mode==BM_STORY, C )
	in_( "!" ) bgn_
		on_( '!' )	do_( "!!" )	s_take
						f_clr( ASSIGN|FILTERED )
						f_set( NEW )
		end
		in_( "!!" ) bgn_
			ons( " \t" )	do_( same )
			on_separator	; // err
			on_other	do_( "!!$" )	s_take
			end
		in_( "!!$" ) bgn_
			ons( " \t" )	do_( "!!$_" )
			on_( '(' )	do_( "!!$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
		in_( "!!$_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "expr" )	s_take
							f_clr( NEW )
							f_set( CARRY )
			end
	in_( "@" ) bgn_
		on_( '<' )	do_( "@<" )	s_take
		end
		in_( "@<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	do_( "expr" )	REENTER
			end
	in_( "~" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( "expr" )
		on_( '(' ) if ( s_empty && *type&(ON|DO) ) {
				do_( "expr" )	REENTER
						s_add( "~" ) }
			else {	do_( "expr" )	REENTER
						s_add( "~" )
						f_set( NEGATED ) } // object is to prevent MARK
		ons( "{}?" )	; //err
		on_other	do_( "expr" )	REENTER
						s_add( "~" )
						f_set( NEGATED )
		end
	in_( "." ) bgn_
		ons( " \t" )	do_( same )
		on_( '.' )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_( '(' )	do_( "expr" )	REENTER
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "term" )	s_take
		end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		on_( '*' )	do_( same )	s_take
		ons( ".%(?" )	do_( "expr" )	REENTER
		ons( "~{" )	; //err
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "term" )	s_take
		end
	in_( "%" ) bgn_
		on_( '(' )	do_( "%(" )	f_push( stack )
						f_reset( FIRST|SUB_EXPR, 0 )
		ons( "?!" )	do_( "%?_" )	s_add( "%" )
						s_take
						f_set( INFORMED )
		ons( "|%@" )	do_( "expr" )	s_add( "%" )
						s_take
						f_set( INFORMED )
		on_( '<' )	do_( "%<" )	s_add( "%<" )
		on_other	do_( "expr" )	REENTER
						s_add( "%" )
						f_set( INFORMED )
		end
		in_( "%?_" ) bgn_
			on_( '~' ) if ( *type&DO && ( is_f(LEVEL) ?
					is_f(TERNARY) && f_signal_authorize(stack) :
					!is_f(NEGATED|SUB_EXPR|SET|ASSIGN|EENOV) ) ) {
					do_( "term~" )	s_take }
			on_other	do_( "expr" )	REENTER
			end
		in_( "%(" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "expr" )	s_add( "%((" )
							f_push( stack )
							f_set( LEVEL|CLEAN )
			on_other	do_( "expr" )	REENTER
							s_add( "%(" )
			end
		in_( "%<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "expr" )	s_take
							f_push( stack )
							f_reset( EENOV|FIRST, 0 )
							f_push( stack )
							f_reset( EENOV|SUB_EXPR|FIRST, 0 )
			ons( "?!" )	do_( "%<?" )	s_take
							f_push( stack )
							f_reset( EENOV|INFORMED|FIRST, 0 )
			on_other	do_( "expr" )	REENTER
							f_set( INFORMED )
			end
		in_( "%<?" ) bgn_
			ons( " \t" )	do_( same )
			on_( '>' )	do_( "expr" )	REENTER
			on_( ':' )	do_( "expr" )	s_take
							f_clr( INFORMED )
			end
	in_( ":<" ) bgn_ // Assumption: assign and vector are set
		ons( " \t" )	do_( same )
		on_( '\n' ) if ( !is_f(FILTERED) ) {
				do_( "expr" )	REENTER
						*type = (*type&ELSE)|INPUT;
						f_clr( ASSIGN|VECTOR )
						f_set( INFORMED ) }
		on_other	do_( "expr" )	REENTER
						f_set( VECTOR )
		end
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '/' ) 	do_( "/" )	s_add( ":/" )
		on_( '<' ) if ( !is_f(LEVEL|SUB_EXPR|SET|ASSIGN) ) {
				do_( "_," )	REENTER }
		on_( '\"' ) if ( *type&DO && !is_f(LEVEL|SUB_EXPR|SET|ASSIGN) ) {
				do_( "_," )	REENTER }
		on_( ')' )	do_( "expr" )	REENTER
						s_add( ":" )
		if are_f( TERNARY|FILTERED ) {	f_set( INFORMED ) }
		on_other	do_( "expr" )	REENTER
						s_add( ":" )
		end
		in_( ":~" ) bgn_
			ons( " \t" )	do_( same )
			on_( '~' )	do_( ":" )
			on_other	do_( "~" )	REENTER
							s_add( ":" )
			end
	in_( "_," ) bgn_
		ons( " \t" )	do_( same )
		on_( '<' )	do_( "_<" )	s_take
		on_( '\"' )	do_( "_,\"" )	s_add( ",\"" )
		on_other	; // err
		end
		in_( "_,\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '%' )	do_( "_,\"%" )	s_take
			on_( '\\' )	do_( "_,\"\\" )	s_take
			on_( '\"' )	do_( "_,_" )	s_take
			on_other	do_( same )	s_take
			end
			in_( "_,\"%" ) bgn_
				ons( "%c_" )	do_( "_,\"" )	s_take
				on_other	do_( "_,\"" )	s_add( "_" )
								bm_parse_caution( data, WarnInputFormat, mode );
				end
			in_( "_,\"\\" ) bgn_
				on_any		do_( "_,\"" )	s_take
				end
			in_( "_,_" ) bgn_
				ons( " \t" )	do_( same )
				on_( '<' )	do_( "_<" )	s_take
				end
		in_( "_<" ) bgn_
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
			on_other	do_( "expr" )	REENTER
			end
		in_( ">\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '%' )	do_( ">\"%" )	s_take
			on_( '\\' )	do_( ">\"\\" )	s_take
			on_( '\"' )	do_( ">_" )	s_take
			on_other	do_( same )	s_take
			end
		in_( ">\"%" ) bgn_
			ons( "%s_" )	do_( ">\"" )	s_take
			on_other	do_( ">\"" )	s_add( "_" )
							bm_parse_caution( data, WarnOutputFormat, mode );
			end
		in_( ">\"\\" ) bgn_
			on_any		do_( ">\"" )	s_take
			end
		in_( ">_" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( "expr" )	s_take
			on_( '\n' )	do_( "expr" )	REENTER
							f_set( INFORMED )
			end
C:CND_endif
	in_( "_^" ) bgn_	// \nl allowed inside {} () <>
		ons( " \t\n" )	do_( same )
		on_( '#' )	do_( "_#" )
		ons( "})>" )	do_( "expr" )	REENTER
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
						f_set( FIRST|LEVEL )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( CLEAN|LISTABLE ) }
				else if ( are_f(SUB_EXPR|CLEAN) ) {
						// CLEAN only for %((?,...):_)
						f_clr( CLEAN ) }
				else {		f_set( CLEAN ) }
		on_other	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( SET|TERNARY )
						f_set( FIRST|LEVEL )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( LISTABLE ) }
		end
	in_( "," ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' ) if ( is_f(SET|CARRY|VECTOR) ) {
				do_( "_^" ) } // allow \nl inside {} () <> as comma
		ons( "})>" )	do_( "expr" )	REENTER // ignore trailing comma
		on_( '.' ) if ( is_f(LISTABLE) || ( is_f(SUB_EXPR) && !is_f(LEVEL|MARKED) ) ) {
				do_( ",." ) 	s_add( "," ) }
			else {	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
		on_( '?' ) if ( is_f(SUB_EXPR) ) {
				do_( ",?" )	s_add( "," ) }
			else {	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
		on_other	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED )
		end
		in_( ",." ) bgn_
			on_( '.' )	do_( ",.." )
			on_other
if ( mode==BM_STORY ) {			do_( "." )	REENTER
							s_add( "." ) }
else { 					; } // err
			end
			in_( ",.." ) bgn_
				on_( '.' )	do_( ",..." )
				on_other
if ( mode==BM_STORY ) {			do_( "expr" )	REENTER
							s_add( ".." )
							f_set( INFORMED ) }
else {					; } // err
				end
			in_( ",..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' ) if ( !is_f(SUB_EXPR) || is_f(LISTABLE) ) {
						do_( ",...)" )	f_pop( stack, 0 ) }
					else {	do_( "expr" ) 	REENTER
								s_add( "..." )
								f_set( INFORMED ) }
				end
			in_( ",...)" ) bgn_
				ons( " \t" )	do_( same )
				on_( ':' ) if ( is_f(SUB_EXPR) ) { // %((?,...)_
						do_( "expr" )	s_add( "...):" )
								f_clr( FIRST ) }
					else {	do_( "(:" )	s_add( "...):" ) }
				on_( ',' ) if ( !is_f(SUB_EXPR) ) {
						do_( "expr" )	s_add( "...)," )
								f_reset( ELLIPSIS, 0 ) }
				end
		in_( ",?" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( ",?:" )
			on_other	do_( "expr" )	REENTER
							s_add( "?" )
							f_tag( stack, MARKED )
							f_set( MARKED|INFORMED )
			end
			in_( ",?:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '.' )	do_( ",?:." )
				on_other	do_( "expr" )	REENTER
								s_add( "?:" )
								f_tag( stack, MARKED )
								f_set( MARKED )
				end
			in_( ",?:." ) bgn_
				on_( '.' )	do_( ",?:.." )
				on_other	do_( "." )	REENTER
								s_add( "?:." )
								f_tag( stack, MARKED )
								f_set( MARKED )
				end
			in_( ",?:.." ) bgn_
				on_( '.' )	do_( ",?:..." )
				on_other	do_( "expr" )	REENTER
								s_add( "?:.." )
								f_tag( stack, MARKED )
								f_set( MARKED|INFORMED )
				end
			in_( ",?:..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' )	do_( "expr" )	REENTER
								s_add( "?:..." )
								f_set( INFORMED )
				end
	in_( "term" ) bgn_
		on_( '~' ) 	; // err
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other 	do_( same )	s_take
		end
	in_( "term_" ) bgn_
		on_( '~' ) 	do_( "term~" )	s_take
						f_set( INFORMED )
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other 	do_( same )	s_take
		end
		in_( "term~" ) bgn_
			ons( " \t" )	do_( same )
			ons( "?<" )	; // err
			on_other	do_( "expr" )	REENTER
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
	in_( "|" ) bgn_
		ons( " \t" )	do_( same )
		ons( "({" )	do_( "expr" )	REENTER
						f_set( COMPOUND )
		end
	//----------------------------------------------------------------------
	// bm_parse:	Expression End
	//----------------------------------------------------------------------
	in_( "expr_" )
CB_( ExpressionTake, mode, data );
		bgn_
			on_any
if ( mode==BM_INPUT ) {		do_( "" ) }
else {				do_( "base" )	f_reset( FIRST, 0 );
						TAB_CURRENT = 0;
						data->opt = 0;
	if ( mode==BM_STORY ) {			*type = 0; } }
			end
	//----------------------------------------------------------------------
	// bm_parse:	Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
		on_( EOF )		errnum = ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState;
		in_( "§..." ) bgn_
			on_( ')' )	errnum = ErrEllipsisLevel;
			on_other	errnum = ErrSyntaxError;
			end
		in_( "cmd" )		errnum = ErrUnknownCommand;
		in_( "_expr" )		errnum = ErrIndentation; column=TAB_BASE;
		in_( ",_" ) 		errnum = ErrInputScheme;
		in_( "_<" ) 		errnum = ErrInputScheme;
		in_( ">" )		errnum = ErrOutputScheme;
		in_( ">_" )		errnum = ErrOutputScheme;
		in_( "expr_" )		errnum = ErrInstantiationFiltered;
		in_other bgn_
			on_( '\n' )	errnum = ErrUnexpectedCR;
			on_( ' ' )	errnum = ErrSpace;
			on_( ')' )	errnum = ( are_f(EENOV|SUB_EXPR|INFORMED) && !is_f(LEVEL) ?
						   ErrEMarked : ErrSyntaxError );
			on_( '?' )	errnum = ( is_f(NEGATED) ? ErrMarkNegated :
						   is_f(MARKED) ? ErrMarkMultiple :
						   ErrSyntaxError );
			on_( '!' )	errnum = ( are_f(EENOV|SUB_EXPR) && !is_f(INFORMED ) ?
							is_f(NEGATED) ? ErrEMarkNegated :
							is_f(EMARKED) ? ErrEMarkMultiple :
							ErrSyntaxError :
						   ErrSyntaxError );
			on_( ':' )	errnum = !s_cmp( "~." ) ? ErrPerContrary : ErrSyntaxError;
			on_other	errnum = ErrSyntaxError;
			end
	//----------------------------------------------------------------------
	// bm_parse:	Parser End
	//----------------------------------------------------------------------
	BMParseEnd
	data->errnum = errnum;
	data->flags = flags;
	if ( errnum ) bm_parse_report( data, mode );
	return state;
}

//===========================================================================
//	bm_parse_report
//===========================================================================
void
bm_parse_report( BMParseData *data, BMParseMode mode )
{
	CNIO *io = data->io;
	int l = io->line, c = io->column;
	char *src = (mode==BM_LOAD)  ? "bm_load" :
		    (mode==BM_INPUT) ? "bm_input" : "bm_parse";
	char *stump = StringFinish( data->string, 0 );

if ( mode==BM_INPUT ) {
	switch ( data->errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: unknown state \"%s\" <<<<<<\n", data->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: %s: in expression '%s' unexpected EOF\n", src, stump  ); break;
	case ErrUnexpectedCR:
		fprintf( stderr, "Error: %s: in expression '%s' unexpected \\cr\n", src, stump  ); break;
	default:
		fprintf( stderr, "Error: %s: in expression '%s' syntax error\n", src, stump  ); } }
else {
	switch ( data->errnum ) {
	case ErrUnknownState:
		fprintf( stderr, ">>>>> B%%::CNParser: l%dc%d: unknown state \"%s\" <<<<<<\n", l, c, data->state  ); break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: %s: l%d: unexpected EOF\n", src, l  ); break;
	case ErrEllipsisLevel:
		fprintf( stderr, "Error: %s: l%dc%d: ellipsis usage not supported\n", src, l, c ); break;
	case ErrSpace:
		fprintf( stderr, "Error: %s: l%dc%d: unexpected ' '\n", src, l, c  ); break;
	case ErrInstantiationFiltered:
		fprintf( stderr, "Error: %s: l%dc%d: base instantiation must be unfiltered\n", src, l, c  ); break;
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
	case ErrEMarked:
		fprintf( stderr, "Error: %s: l%dc%d: '!' unspecified\n", src, l, c ); break;
	case ErrEMarkMultiple:
		fprintf( stderr, "Error: %s: l%dc%d: '!' already informed\n", src, l, c ); break;
	case ErrEMarkNegated:
		fprintf( stderr, "Error: %s: l%dc%d: '!' negated\n", src, l, c ); break;
	case ErrUnknownCommand:
		fprintf( stderr, "Error: %s: l%dc%d: unknown command '%s'\n", src, l, c, stump  ); break;
	case ErrPerContrary:
		fprintf( stderr, "Error: %s: l%dc%d: per contrary not supported\n", src, l, c ); break;
	default:
		fprintf( stderr, "Error: %s: l%dc%d: syntax error\n", src, l, c  ); } }
}

//---------------------------------------------------------------------------
//	bm_parse_caution
//---------------------------------------------------------------------------
void
bm_parse_caution( BMParseData *data, BMParseErr errnum, BMParseMode mode )
/*
	Assumption: mode==BM_STORY
*/
{
	CNIO *io = data->io;
	int l = io->line, c = io->column;
	switch ( errnum ) {
	case WarnOutputFormat:
		fprintf( stderr, "Warning: bm_parse: l%dc%d: unsupported output format - using default \"%%_\"\n", l, c ); break;
	case WarnInputFormat:
		fprintf( stderr, "Warning: bm_parse: l%dc%d: unsupported input format - using default \"%%_\"\n", l, c ); break;
	default: break; }
}

