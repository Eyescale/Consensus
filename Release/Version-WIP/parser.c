#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "parser_private.h"
// #define DEBUG
#include "parser_macros.h"

// these flags characterize expression as a whole
#define CONTRARY	2
#define RELEASED	4
#define expr(a)		(data->expr&(a))

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
bm_parse_expr( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )
/*
	Assumption: data->expr!=0
*/
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

	BMParseBegin( "bm_parse_expr", state, event, line, column )
	//----------------------------------------------------------------------
	// bm_parse_expr:	Parser Begin
	//----------------------------------------------------------------------
	in_( "expr" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' ) if ( is_f(VECTOR|SET|CARRY) && !is_f(LEVEL) ) {
				do_( "_\\n" ) } // allow \nl inside <> {} and carry()
			else if ( is_f(INFORMED) && !is_f(LEVEL|SUB_EXPR|EENOV) &&
				  !( *type&DO && is_f(MARKED) ) &&
				  !( *type&ON && is_f(MARKED) && expr(RELEASED) ) &&
				     !( *type&PER && !(*type&ON_X) ) ) {
				do_( "expr_" )	REENTER }
		on_( '"' ) if ( s_empty && *type&DO ) {
				do_( "\"" ) 	s_take }
		on_( '~' ) if ( !is_f(INFORMED) ) {
				do_( "~" ) }
			else if ( *type&DO ) {
				do_( "@" )	s_take }
		on_( '%' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "%" ) }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) || is_f(TERNARY) )
				; // err
			else if ( is_f(ASSIGN) ) {
				if ( !is_f(PRIMED|LEVEL|SUB_EXPR) ) {
					do_( *type&DO ? ":_" : same )
							s_take
							f_clr( FIRST|NEGATED|STARRED|INFORMED )
							f_set( PRIMED ) } }
			else if ( is_f(LEVEL|SUB_EXPR) ) {
				if ( are_f(FIRST|SUB_EXPR) ) {
					do_( "," )	s_take
							f_restore( NEGATED|STARRED|FILTERED ) }
				else if ( are_f(FIRST|LEVEL) ) {
					do_( "," )	s_take
							f_restore( NEGATED|STARRED|FILTERED|PIPED|PROTECTED ) } }
			else if ( is_f(VECTOR|SET|CARRY) ) {
				do_( "{_," )	f_clr( NEGATED|STARRED|FILTERED ) } // do not clear FIRST|INFORMED
			else if ( *type&DO ) {
				do_( "_," )	f_clr( FIRST|INFORMED|NEGATED|STARRED|FILTERED ) }
		on_( ':' ) if ( is_f(EENOV) || ((*type&PER)&&!s_cmp("~.")) )
				; // err
			else if ( s_empty || !s_cmp( "~.:" ) ) {
				do_( ":_" )	s_take
						f_set( ASSIGN ) }
			else if ( is_f(INFORMED) ) {
				if ( is_f(ASSIGN) ) {
					if ( !is_f(PRIMED|LEVEL|SUB_EXPR) ) {
						do_( *type&DO ? ":_" : same )
								s_add( "," )
								f_clr( FIRST|NEGATED|STARRED|INFORMED )
								f_set( PRIMED ) } }
				else if ( is_f(TERNARY) ) {
					if ( !is_f(PRIMED) ) {
						do_( "(_?_:" )	s_take
								f_restore( NEGATED|STARRED )
								f_clr( INFORMED|MARKED )
								f_set( PRIMED ) }
					else if ( !is_f(FIRST) ) {
						do_( same )	REENTER
								f_pop( stack, 0 ) } }
				else if ( !is_f(PROTECTED) ) {
						do_( ":" )	f_restore( NEGATED|STARRED )
								f_clr( INFORMED )
								f_set( FILTERED ) } }
		on_( '?' ) if ( is_f(INFORMED) ) {
				if ( !is_f(PROTECTED|EENOV) && is_f(LEVEL|SUB_EXPR) &&
				     ( !is_f(MARKED) || ( is_f(LEVEL) && f_parent(MARKED) ) ) ) {
					do_( "(_?" )	s_take
							f_restore( NEGATED|STARRED|FILTERED )
							f_set( TERNARY );
				if ( is_f(FIRST) ) {	f_set( PRIMED ); }
							f_push( stack )
							f_clr( FIRST|INFORMED|PRIMED ) } }
			else if ( ( is_f(SUB_EXPR|EENOV) || ( !(*type&DO) && !expr(CONTRARY) ) ) &&
				  !is_f(MARKED|NEGATED) && f_ternary_markable(stack) ) {
				if ( is_f(EENOV) ) {
					do_( same )	s_take
							f_set( MARKED|INFORMED ) }
				else if ( is_f(SUB_EXPR) || ( *type&ON_X ? !is_f(LEVEL|SET) : *type&(IN|ON) ) ) {
					do_( same )	s_take
							f_set( MARKED|INFORMED ) } }
		on_( '.' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "." ) 	s_take }
		on_( ')' ) if ( is_f(EENOV) ) {
				if ( is_f(INFORMED) && ( is_f(LEVEL) || are_f(SUB_EXPR|EMARKED) ) ) {
					do_( same )	s_take
							f_pop( stack, MARKED|EMARKED|INFORMED ) } }
			else if ( is_f(LEVEL|SUB_EXPR) ) {
				if ( is_f(TERNARY) ) {
					if ( is_f(PRIMED) ) {
						do_( same )	s_take
								while (!is_f(FIRST)) f_pop( stack, 0 )
								f_pop( stack, 0 ) // never pass MARKED
								f_set( INFORMED ) } }
				else if ( is_f(INFORMED) ) {
					if ( is_f(LEVEL) ) {
						do_( same )	s_take
								f_pop( stack, MARKED|INFORMED ) }
					else {	do_( same )	s_take
								f_pop( stack, INFORMED ) } } }
			else if ( are_f(ELLIPSIS|INFORMED) && !is_f(VECTOR) ) {
					do_( same )	s_take
							f_pop( stack, INFORMED ) }
			else if ( is_f(CARRY) ) {
					do_( same )	s_take
							f_pop( stack, 0 )
							f_tag( stack, PROTECTED )
							f_set( INFORMED ) }
		on_( '*' ) if ( is_f(INFORMED) )
				; // err
			else if ( is_f(EENOV) ) {
				do_( same ) 	s_take
						f_set( INFORMED ) }
			else {	do_( "*" )	s_take
						f_clr( PIPED|PROTECTED )
						f_set( STARRED ) }
		on_( '@' ) if ( *type&DO && is_f(INFORMED) ) {
				do_( "@" )	s_take }
		on_( '/' ) if ( !is_f(INFORMED) && ( !(*type&DO) || is_f(EENOV) ) ) {
				do_( "/" )	s_take }
		on_( '<' ) if ( is_f(SET|LEVEL|SUB_EXPR|EENOV|VECTOR) || !is_f(INFORMED) )
				; // err
			else if ( *type&ON && (is_f(ASSIGN)?is_f(PRIMED):is_f(FIRST)) ) {
				do_( same )	s_take
						*type = (*type&~ON)|ON_X;
						f_reset( FIRST, 0 ) }
			else if ( *type&DO && !is_f(ASSIGN|ELLIPSIS) ) {
				do_( "_<" )	s_take }
		on_( '>' ) if ( *type&DO && s_empty ) {
				do_( ">" )	s_take
						*type = (*type&ELSE)|OUTPUT; }
			else if ( are_f(EENOV|INFORMED) && !is_f(LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, INFORMED ) }
			else if ( are_f(VECTOR|INFORMED) && !is_f(LEVEL|SUB_EXPR|EENOV) ) {
					do_( same )	s_take
							f_pop( stack, INFORMED )
							f_tag( stack, PROTECTED ) }
		on_( '!' ) if ( *type&DO && s_empty ) {
				do_( "!" )	s_take }
			else if ( are_f(EENOV|SUB_EXPR) && !is_f(EMARKED|INFORMED|NEGATED) ) {
				do_( same )	s_take
						f_set( EMARKED|INFORMED ) }
		on_( '{' ) if ( !is_f(INFORMED) && (
				( *type&ON_X && s_at('<') ) ||
				( *type&DO && f_actionable(stack) ) ) ) {
					do_( same )	s_take
							f_push( stack )
							f_reset( FIRST|SET, PROTECTED|PIPED ) }
		on_( '}' ) if ( *type&(DO|ON_X) && are_f(INFORMED|SET) && !is_f(LEVEL|SUB_EXPR) ) {
				do_( same )	s_take
						f_pop( stack, 0 )
						f_tag( stack, PROTECTED )
						f_set( INFORMED ) }
		on_( '|' ) if ( *type&DO && is_f(INFORMED) && is_f(LEVEL|SET) &&
				!is_f(EENOV|SUB_EXPR|FILTERED) && !f_parent(NEGATED|STARRED) ) {
				do_( same )	s_take
						f_tag( stack, PROTECTED )
						f_set( PIPED )
						f_clr( INFORMED ) }
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_separator	; // err
		on_other if ( is_f(INFORMED) )
				; // err
			else if ( *type&DO && !is_f(ASSIGN) && f_actionable(stack) &&
				  ( !is_f(LEVEL) || f_ternary_signalable(stack) ) ) {
				do_( "term" )	s_take } // signalable
			else {	do_( "_term" )	s_take }
		end
	//----------------------------------------------------------------------
	// bm_parse_expr:	Expression sub-states
	//----------------------------------------------------------------------
	in_( "\"" ) bgn_
		on_( '\t' )	do_( same )	s_add( "\\t" )
		on_( '\n' )	do_( same )	s_add( "\\n" )
		on_( '\\' )	do_( "\"\\" )	s_take
		on_( '"' )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_other	do_( same )	s_take
		end
		in_( "\"\\" ) bgn_
			on_any		do_( "\"" )	s_take
			end
	in_( "~" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( "expr" )
		on_( '.' ) if ( !is_f(EENOV) ) {
				do_( "~." ) }
		on_( '(' ) if ( s_empty && *type&(ON|DO) ) {
				do_( "expr" )	REENTER
						s_add( "~" )
						data->expr |= RELEASED; }
			else {	do_( "expr" )	REENTER
						s_add( "~" )
						f_clr( PIPED|PROTECTED )
						f_set( NEGATED ) } // to prevent MARK
		ons( "{}?" )	; //err
		on_other	do_( "expr" )	REENTER
						s_add( "~" )
						f_clr( PIPED|PROTECTED )
						f_set( NEGATED )
		end
		in_( "~." ) bgn_
			ons( " \t" )	do_( same )
			on_( '.' )	do_( "expr" )	s_add( "~.." )
							f_set( INFORMED )
			on_( '(' )	do_( "expr" )	REENTER
							s_add( "~." )
			on_( '?' ) 	; // err
			on_( ':' ) if ( is_f(TERNARY) ) {
					do_( "expr" )	REENTER
							s_add( "~." )
							f_set( INFORMED ) }
				else if ( s_empty && !(*type&DO) ) {
					do_( "expr" )	s_add( "~.:" )
							data->expr |= CONTRARY; }
			on_separator if ( is_f(TERNARY) || ( !is_f(SUB_EXPR|CARRY) &&
					 ( !is_f(LEVEL) || (*type&DO && !is_f(FIRST)) ) )) {
					do_( "expr" )	REENTER
							s_add( "~." )
							f_set( INFORMED ) }
			on_other	do_( "_term" )	s_add( "~." )
							s_take
			end
	in_( "%" ) bgn_
		on_( '(' )	do_( "%(" )	f_push( stack )
						f_reset( FIRST|SUB_EXPR, 0 )
		ons( "?!" )	do_( "%?_" )	s_add( "%" )
						s_take
						f_set( INFORMED )
		on_( '|' ) if ( is_f(PIPED) ) {
				do_( "%|" )	s_add( "%|" )
						f_set( INFORMED ) }
		ons( "%@" )	do_( "expr" )	s_add( "%" )
						s_take
						f_set( INFORMED )
		on_( '<' )	do_( "%<" )	s_add( "%<" )
		on_other	do_( "expr" )	REENTER
						s_add( "%" )
						f_set( INFORMED )
		end
		in_( "%|" ) bgn_
			on_( '^' )	do_( "^sub" )	s_take
							f_push( stack )
							f_clr( LEVEL|INFORMED|MARKED )
							f_set( FIRST )
			on_other	do_( "expr" )	REENTER
			end
			in_( "^sub" ) bgn_
				on_( '.' ) if ( !is_f(LEVEL) ) {
						do_( "expr" )	s_take
								f_pop( stack, 0 ) }
					else if ( !is_f(INFORMED) && (is_f(MARKED)?!is_f(FIRST):is_f(FIRST)) ) {
						do_( same )	s_take
								f_set( INFORMED ) }
				on_( '(' ) if ( !is_f(MARKED) ) {
						do_( same )	s_take
								f_push( stack )
								f_clr( INFORMED )
								f_set( FIRST|LEVEL ) }
				on_( '?' ) if ( is_f(LEVEL) && !is_f(INFORMED|MARKED) ) {
						do_( same )	s_take
								f_set( INFORMED|MARKED ) }
				on_( ',' ) if ( are_f(LEVEL|FIRST) && is_f(INFORMED) ) {
						do_( same )	s_take
								f_clr( FIRST|INFORMED ) }
				on_( ')' ) if ( !is_f(FIRST) ) {
						do_( "^sub_" )	s_take
								f_pop( stack, 0 ) }
				end
			in_( "^sub_" ) bgn_
				on_any	if ( !is_f(LEVEL) ) {
						do_( "expr" )	REENTER
								f_pop( stack, 0 ) }
					else {	do_( "^sub" )	REENTER }
				end
		in_( "%(" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "expr" )	s_add( "%((" )
							f_push( stack )
							f_set( LEVEL|CLEAN )
			on_( ':' )	do_( "expr" )	s_add( "%(:" )
							f_set( ASSIGN )
			on_other	do_( "expr" )	REENTER
							s_add( "%(" )
			end
		in_( "%?_" ) bgn_
			on_( '~' ) if ( !is_f(ASSIGN) && *type&DO && f_actionable(stack) &&
				        ( !is_f(LEVEL) || f_ternary_signalable(stack) ) ) {
					do_( "term~" )	s_take }
			on_other	do_( "expr" )	REENTER
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
	in_( "(" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' ) if ( *type&DO ) {
				if ( f_actionable(stack) ) {
					do_( "(:" )	s_take
							f_set( LITERAL ) } }
			else if ( !is_f(EENOV) ) {
				do_( "expr" )	s_take
						f_push( stack )
						f_clr( CLEAN|TERNARY|PRIMED )
						f_set( LEVEL|ASSIGN ) }
		on_( '(' )	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( TERNARY|ASSIGN|PRIMED )
						f_set( FIRST|LEVEL )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( LISTABLE ) }
				else if ( are_f(SUB_EXPR|CLEAN) ) {
						// CLEAN only for %((?,...):_)
						f_clr( CLEAN ) }
				else {		f_set( CLEAN ) }
		on_other	do_( "expr" )	REENTER
						f_push( stack )
						f_clr( TERNARY|ASSIGN|PRIMED )
						f_set( FIRST|LEVEL )
				if ( *type&DO && is_f(CLEAN) ) {
						f_set( LISTABLE ) }
		end
	in_( "," ) bgn_ // Assumption: all flags untouched as before comma
		ons( " \t" )	do_( same )
		on_( '.' ) if ( is_f(SUB_EXPR) ?
				is_f(MARKED) ? is_f(CLEAN) && !is_f(FILTERED) : !is_f(LEVEL) :
				is_f(LISTABLE) ) {
				do_( ",." )	f_clr( FIRST|INFORMED ) }
			else {	do_( "expr" )	REENTER
						f_clr( FIRST|INFORMED ) }
		on_( '?' ) if ( is_f(SUB_EXPR) && !is_f(LEVEL|MARKED) ) {
				do_( ",?" )	f_clr( FIRST|INFORMED ) }
			else {	do_( "expr" )	REENTER
						f_clr( FIRST|INFORMED ) }
		on_other	do_( "expr" )	REENTER
						f_clr( FIRST|INFORMED )
		end
		in_( ",." ) bgn_ // look ahead for ellipsis
			on_( '.' )	do_( ",.." )
			on_other	do_( "." )	REENTER
							s_add( "." )
			end
			in_( ",.." ) bgn_
				on_( '.' ) if ( is_f(SUB_EXPR) ) {
						do_( "expr" )	s_add( "..." )
								f_set( INFORMED ) }
					else if ( *type&DO && f_actionable(stack) ) {
						do_( ",..." ) }
				on_other	do_( "expr" )	REENTER
								s_add( ".." )
								f_set( INFORMED )
				end
			in_( ",..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' )	do_( ",...)" )	f_pop( stack, 0 )
								f_tag( stack, PROTECTED )
				end
			in_( ",...)" ) bgn_
				ons( " \t" )	do_( same )
				on_( ',' )	do_( ",...)," )
				on_( ':' )	do_( "(:" )	// LIST
								s_add( "...):" )
				end
			in_( ",...)," ) bgn_
				ons( " \t" )	do_( same )
				on_( '<' )	do_( "expr" )	s_add( "...),<" )
								f_reset( ELLIPSIS, 0 )
								f_push( stack )
								f_reset( FIRST|VECTOR, 0 )
				on_other 	do_( "expr" )	REENTER
								s_add( "...)," )
								f_reset( ELLIPSIS, 0 )
				end
		in_( ",?" ) bgn_ // look ahead for ?:...
			ons( " \t" )	do_( same )
			on_( ':' )	do_( ",?:" )
			on_other	do_( "expr" )	REENTER
							s_add( "?" )
							f_set( MARKED|INFORMED )
			end
			in_( ",?:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '.' )	do_( ",?:." )
				on_other	do_( "expr" )	REENTER
								s_add( "?:" )
								f_set( MARKED )
				end
			in_( ",?:." ) bgn_
				on_( '.' )	do_( ",?:.." )
				on_other	do_( "." )	REENTER
								s_add( "?:." )
								f_set( MARKED )
				end
			in_( ",?:.." ) bgn_
				on_( '.' )	do_( ",?:..." )
				on_other	do_( "expr" )	REENTER
								s_add( "?:.." )
								f_set( MARKED|INFORMED )
				end
			in_( ",?:..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' )	do_( "expr" )	REENTER
								s_add( "?:..." )
								f_set( INFORMED )
				end
	in_( ":" ) bgn_	// Assumption: ASSIGN is NOT set
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '/' )	do_( "/" )	s_add( ":/" )
		on_( '<' ) if ( !is_f(LEVEL|SUB_EXPR|SET) ) {
				do_( "_," )	REENTER }
		on_( '"' ) if ( *type&DO && !is_f(LEVEL|SUB_EXPR|SET) ) {
				do_( "_," )	REENTER }
		on_( ')' ) if ( are_f(TERNARY|PRIMED) ) {
				do_( "expr" )	REENTER
						s_add( ":" )
						f_set( INFORMED ) }
			else {	do_( "expr" )	REENTER
						s_add( ":" ) }
		on_( '?' )	// err
		on_other	do_( "expr" )	REENTER
						s_add( ":" )
		end
		in_( ":~" ) bgn_
			ons( " \t" )	do_( same )
			on_( '~' )	do_( ":" )
			on_other	do_( "~" )	REENTER
							s_add( ":" )
			end
	in_( ":_" ) bgn_ // Assumption: ASSIGN is set, may or may not be PRIMED
		ons( " \t" )	do_( same )
		on_( '"' ) if ( *type&DO && is_f(PRIMED) ) {
				do_( "\"" )	s_take }
		on_( '~' )	do_( "~" )
		on_( '<' )	do_( ":<")	s_take	// Special: getchar()
		on_( '!' ) if ( is_f(PRIMED) ) {
				do_( "!" )	s_take }
		ons( "/\")" )	; // err
		on_other	do_( "expr" )	REENTER
		end
		in_( ":<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' ) if ( *type&DO && !is_f(PRIMED) ) {
					do_( "expr" )	REENTER
							*type = (*type&ELSE)|INPUT;
							f_clr( ASSIGN )
							f_set( INFORMED ) }
			on_other	do_( "expr" )	REENTER
							f_push( stack )
							f_reset( FIRST|VECTOR, PROTECTED|PIPED )
			end
	in_( "." ) bgn_
		ons( " \t" )	do_( same )
		on_( '.' )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_( '(' )	do_( "expr" )	REENTER
		on_( '?' ) if ( is_f(SUB_EXPR) ) {
				do_( "expr" )	REENTER }
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "_term" )	s_take
		end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		on_( '*' )	do_( same )	s_take
		ons( ".%(?" )	do_( "expr" )	REENTER
		ons( "~{" )	; //err
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "_term" )	s_take
		end
	in_( "_term" ) bgn_
		on_( '~' ) 	do_( "@" )	s_take
						f_set( INFORMED )
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other 	do_( same )	s_take
		end
	in_( "term" ) bgn_
		on_( '~' ) 	do_( "term~" )	s_take
						f_set( INFORMED )
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other 	do_( same )	s_take
		end
		in_( "term~" ) bgn_
			ons( " \t" )	do_( same )
			on_( '<' )	do_( "@<" )	s_take
							bm_parse_caution( data, WarnUnsubscribe, mode );
			on_( '?' )	; // err
			on_other	do_( "expr" )	REENTER
							f_tag( stack, PROTECTED )
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
	in_( "(_?" ) bgn_ // Assumption: TERNARY is set, not INFORMED
		ons( " \t\n" )	do_( same )
		on_( ':' )	do_( "(_?:" )	s_take
						f_set( PRIMED )
		on_other	do_( "expr" )	REENTER
		end
		in_( "(_?:" ) bgn_
			ons( " \t\n" )	do_( same )
			on_( ')' )	; // err
			on_other	do_( "expr" )	REENTER
			end
	in_( "(_?_:" ) bgn_ // Assumption: TERNARY|PRIMED are set, not INFORMED
		ons( " \t\n" )	do_( same )
		on_( ':' )	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( "expr" )	REENTER
		end
	in_( "(:" ) bgn_
		ons( "(\n" )	; // err
		on_( ')' ) if ( is_f(LITERAL) ) { // otherwise list
				do_( "expr" )	s_take
						f_tag( stack, PROTECTED )
						f_set( INFORMED ) }
		on_( '\\' )	do_( "(:\\" )	s_take
		on_( '%' )	do_( "(:%" )	s_take
		on_( ':' ) if ( is_f(LITERAL) ) { // otherwise list
				do_( same )	s_take }
			else {	do_( "(::" )	s_take }
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
			on_other	do_( "(:%$" )	s_take
			end
		in_( "(:%$" ) bgn_
			on_( '\'' )	do_( "(:%$'" )	s_take
			on_separator	do_( "(:" )	REENTER
			on_other	do_( same )	s_take
			end
		in_( "(:%$'" ) bgn_
			on_separator	; // err
			on_other	do_( "(:" )	REENTER
			end
		in_( "(::" ) bgn_
			on_( ')' )	do_( "expr" )	REENTER
							f_tag( stack, PROTECTED )
							f_set( INFORMED )
			on_other	do_( "(:" )	REENTER
			end
	in_( "!" ) bgn_
		on_( '!' )	do_( "!!" )	s_take
						f_clr( ASSIGN|PRIMED )
		end
		in_( "!!" ) bgn_
			ons( " \t" )	do_( same )
			on_( '|' )	do_( "expr" )	s_take
			on_separator	; // err
			on_other	do_( "!!$" )	s_take
			end
		in_( "!!$" ) bgn_
			on_separator	do_( "!!$_" )	REENTER
			on_other	do_( same )	s_take
			end
		in_( "!!$_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "expr" )	s_take
							f_push( stack )
							f_reset( FIRST|CARRY, PROTECTED )
			on_other	do_( "expr" )	REENTER
							f_set( INFORMED|PROTECTED )
			end
	in_( "@" ) bgn_
		on_( '<' )	do_( "@<" )	s_take
		end
		in_( "@<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	do_( "expr" )	REENTER
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
	in_( ">" ) bgn_
		ons( " \t" )	do_( same )
		on_( '&' )	do_( ">&" )	s_take
		on_( ':' )	do_( ">:" )	s_take
		on_( '\"' )	do_( ">\"" )	s_take
		end
		in_( ">&" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( ">:" )	s_take
			on_( '"' )	do_( ">\"" )	s_take
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
			on_( '"' )	do_( ">_" )	s_take
			on_other	do_( same )	s_take
			end
			in_( ">\"%" ) bgn_
				ons( "%$s_" )	do_( ">\"" )	s_take
				on_other	do_( ">\"" )	s_add( "_" )
								bm_parse_caution( data, WarnOutputFormat, mode );
				end
			in_( ">\"\\" ) bgn_
				on_any		do_( ">\"" )	s_take
				end
			in_( ">_" ) bgn_
				ons( " \t" )	do_( same )
				on_( ':' )	do_( ">_:" )	s_take
				on_( '\n' )	do_( "expr" )	REENTER
								f_set( INFORMED )
				end
			in_( ">_:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '<' )	do_( "expr" )	s_take
								f_push( stack )
								f_reset( FIRST|VECTOR, PROTECTED )
				on_other	do_( "expr" )	REENTER
				end
	in_( "_," ) bgn_
		ons( " \t" )	do_( same )
		on_( '<' )	do_( "_<" )	s_take
		on_( '"' )	do_( "_,\"" )	s_add( ",\"" )
		on_other	; // err
		end
		in_( "_,\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '%' )	do_( "_,\"%" )	s_take
			on_( '\\' )	do_( "_,\"\\" )	s_take
			on_( '"' )	do_( "_,_" )	s_take
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
	in_( "_\\n" ) bgn_ // \nl allowed inside {} () <>
		ons( " \t\n" )	do_( same )
		ons( "})>" )	do_( "expr" )	REENTER
		on_( ',' )	; // err
		on_other // allow newline to act as comma
			if ( is_f(INFORMED) && !is_f(LEVEL|SUB_EXPR) ) {
				do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
			else {	do_( "expr" )	REENTER }
		end
	in_( "{_," ) bgn_ // Assumption: INFORMED is set
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "_\\n" )	// allow \nl inside <> {} and carry() as comma
		ons( "})>" )	do_( "expr" )	REENTER // ignore trailing comma
		on_other	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED )
		end
	//----------------------------------------------------------------------
	// bm_parse_expr:	Expression End
	//----------------------------------------------------------------------
	in_( "expr_" )
CB_( ExpressionTake, mode, data )
		bgn_ on_any
		if ( mode==BM_STORY ) {
			do_( "base" )	f_reset( FIRST, 0 );
					TAB_CURRENT = 0;
					data->expr = 0;
					*type = 0; }
		else { 	do_( "" ) }
			end
	//----------------------------------------------------------------------
	// bm_parse_expr:	Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
		on_( EOF )		errnum = ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState; data->state = state;
		in_( ",_" ) 		errnum = ErrInputScheme;
		in_( "_<" ) 		errnum = ErrInputScheme;
		in_( ">" )		errnum = ErrOutputScheme;
		in_( ">_" )		errnum = ErrOutputScheme;
		in_other bgn_
			on_( '\n' ) bgn_
				in_( "expr" )	errnum = ( is_f( MARKED ) ?
							(*type&ON && expr(RELEASED)) ? ErrMarkOn :
							*type&DO ? ErrMarkDo : ErrUnexpectedCR :
							ErrUnexpectedCR );
				in_other	errnum = ErrUnexpectedCR;
				end
			on_( ' ' )	errnum = ErrUnexpectedSpace;
			on_( ')' )	errnum = ( are_f(EENOV|SUB_EXPR|INFORMED) && !is_f(LEVEL) ?
						   ErrEMarked : ErrSyntaxError );
			on_( '?' )	errnum = ( is_f(NEGATED) ? ErrMarkNegated :
						   expr(CONTRARY) ? ErrMarkNegated :
						   is_f(INFORMED) ? ErrMarkGuard :
						   !f_ternary_markable(stack) ? ErrMarkTernary :
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
	// bm_parse_expr:	Parser End
	//----------------------------------------------------------------------
	BMParseEnd
	data->errnum = errnum;
	data->flags = flags;
	if ( errnum ) bm_parse_report( data, mode, line, column );
	return state; }

char *
bm_parse_load( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )
/*
	Assumption: mode==BM_LOAD or mode==BM_INPUT => do not involve
	. VECTOR, CARRY, SUB_EXPR, MARKED, EENOV, ASSIGN, PROTECTED, TERNARY,
	  PRIMED, FILTERED, NEGATED, ELLIPSIS, input, output, wildcards
*/
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

	BMParseBegin( "bm_parse_load", state, event, line, column )
	//----------------------------------------------------------------------
	// bm_parse_load:	Expression states
	//----------------------------------------------------------------------
	in_( "base" ) bgn_
		on_( EOF )	do_( "" )
		ons( " \t\n" )	do_( same )
		on_other	do_( "expr" )	REENTER
		end
	in_( "expr" ) bgn_
		ons( " \t" ) if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	REENTER }
			else {	do_( same ) }
		on_( '\n' ) if ( is_f(SET) ) { // allow \nl inside {}
				do_( "_\\n" ) }
			else if ( is_f(INFORMED) && !is_f(LEVEL) ) {
				do_( "expr_" )	REENTER }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) )
				; // err
			else if ( is_f(LEVEL) ) {
				if ( is_f(FIRST) ) {
					do_( "," )	s_take } }
			else if ( is_f(SET) ) {
				do_( "{_," ) } // do not clear FIRST|INFORMED
		ons( "*%" ) if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_set( INFORMED ) }
		on_( ')' ) if ( are_f(INFORMED|LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, INFORMED ) }
		on_( '{' ) if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_push( stack )
						f_reset( FIRST|SET, 0 ) }
		on_( '}' ) if ( are_f(INFORMED|SET) && !is_f(LEVEL) ) {
				do_( same )	s_take
						f_pop( stack, 0 )
						f_set( INFORMED ) }
		on_( '|' ) if ( is_f(INFORMED) && is_f(LEVEL|SET) ) {
				do_( "|" )	s_take
						f_clr( INFORMED ) }
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_separator	; // err
		on_other if ( !is_f(INFORMED) ) {
				do_( "term" )	s_take }
		end
	//----------------------------------------------------------------------
	// bm_parse_load:	Expression sub-states
	//----------------------------------------------------------------------
	in_( "(" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' )	do_( "(:" )	s_take
						f_set( LITERAL )
		on_( '(' )	do_( "expr" )	REENTER
						f_push( stack )
						f_set( FIRST|LEVEL|CLEAN )
		on_other	do_( "expr" )	REENTER
						f_push( stack )
						f_set( FIRST|LEVEL )
		end
	in_( "," ) bgn_ // Assumption: all flags untouched as before comma
		ons( " \t" )	do_( same )
		on_( '.' ) if ( is_f(CLEAN) ) {
				do_( ",." )	f_clr( FIRST|INFORMED ) }
			else {	do_( "expr" )	REENTER
						f_clr( FIRST|INFORMED ) }
		on_other	do_( "expr" )	REENTER
						f_clr( FIRST|INFORMED )
		end
		in_( ",." ) bgn_ // look ahead for ellipsis
			on_( '.' )	do_( ",.." )
			end
		in_( ",.." ) bgn_
			on_( '.' )	do_( ",..." )
			end
		in_( ",..." ) bgn_
			ons( " \t" )	do_( same )
			on_( ')' )	do_( ",...)" )	f_pop( stack, 0 )
			end
		in_( ",...)" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( "(:" )	s_add( "...):" )
			on_other	do_( "expr" )	REENTER
							s_add( "...)" )
			end
	in_( "term" ) bgn_
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other 	do_( same )	s_take
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
		ons( "(\n" )	; // err
		on_( ')' ) if ( is_f(LITERAL) ) { // otherwise list
				do_( "expr" )	s_take
						f_set( INFORMED ) }
		on_( '\\' )	do_( "(:\\" )	s_take
		on_( '%' )	do_( "(:%" )	s_take
		on_( ':' ) if ( is_f(LITERAL) ) { // otherwise list
				do_( same )	s_take }
			else {	do_( "(::" )	s_take }
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
			on_other	do_( "(:%$" )	s_take
			end
		in_( "(:%$" ) bgn_
			on_( '\'' )	do_( "(:%$'" )	s_take
			on_separator	do_( "(:" )	REENTER
			on_other	do_( same )	s_take
			end
		in_( "(:%$'" ) bgn_
			on_separator	; // err
			on_other	do_( "(:" )	REENTER
			end
		in_( "(::" ) bgn_
			on_( ')' )	do_( "expr" )	REENTER
							f_set( INFORMED )
			on_other	do_( "(:" )	REENTER
			end
	in_( "|" ) bgn_
		ons( " \t" )	do_( same )
		ons( "({" )	do_( "expr" )	REENTER
		end
	in_( "{_," ) bgn_ // Assumption: INFORMED is set
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "_\\n" )	// allow \nl inside {} as comma
		on_( '}' )	do_( "expr" )	REENTER // ignore trailing comma
		on_other	do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED )
		end
	in_( "_\\n" ) bgn_ // \nl allowed inside {}
		ons( " \t\n" )	do_( same )
		ons( "})>" )	do_( "expr" )	REENTER
		on_( ',' )	; // err
		on_other // allow newline to act as comma
			if ( is_f(INFORMED) && !is_f(LEVEL) ) {
				do_( "expr" )	REENTER
						s_add( "," )
						f_clr( INFORMED ) }
			else {	do_( "expr" )	REENTER }
		end
	//----------------------------------------------------------------------
	// bm_parse_load:	Expression End
	//----------------------------------------------------------------------
	in_( "expr_" )
CB_( ExpressionTake, mode, data )
		if ( mode==BM_LOAD ) {
			bgn_ on_any
				do_( "base" )	f_reset( FIRST, 0 )
				end }
		else {	bgn_ on_any
				do_( "" )
				end }
	//----------------------------------------------------------------------
	// bm_parse_load:	Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
		on_( EOF )		errnum = ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState; data->state = state;
		in_other bgn_
			on_( '\n' )	errnum = ErrUnexpectedCR;
			on_( ' ' )	errnum = ErrUnexpectedSpace;
			on_other	errnum = ErrSyntaxError;
			end
	BMParseEnd
	data->errnum = errnum;
	data->flags = flags;
	if ( errnum ) bm_parse_report( data, mode, line, column );
	return state; }

char *
bm_parse_cmd( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )
/*
	Assumption: mode==BM_STORY, data->expr==0
*/
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

	BMParseBegin( "bm_parse_cmd", state, event, line, column )
	//----------------------------------------------------------------------
	// bm_parse_cmd:	Parser Begin
	//----------------------------------------------------------------------
	in_( "base" ) bgn_
		on_( EOF )	do_( "" )
		on_( '\t' )	do_( same )	TAB_CURRENT++;
		on_( '\n' )	do_( same )	TAB_CURRENT = 0;
		on_( '+' ) if ( !TAB_CURRENT ) {
				do_( "+" )	TAB_SHIFT++; }
		on_( '-' ) if ( !TAB_CURRENT ) {
				do_( "-" )	TAB_SHIFT--; }
		on_( '.' ) 	do_( "." )	s_take
						TAB_BASE = column;
		on_( ':' ) if ( !TAB_CURRENT ) {
CB_if_( NarrativeTake, mode, data ) {
				do_( ":" )	TAB_SHIFT = 0; } }
		on_( '%' )	do_( "cmd" )	REENTER
						TAB_BASE = column;
		on_separator	; // err
		on_other	do_( "cmd" )	s_take
						TAB_BASE = column;
		end
	in_( "+" ) bgn_
		on_( '+' )	do_( same )	TAB_SHIFT++;
		on_other	do_( "base" )	REENTER
		end
	in_( "-" ) bgn_
		on_( '-' )	do_( same )	TAB_SHIFT--;
		on_other	do_( "base" )	REENTER
		end
	in_( "." ) bgn_
		on_separator	; // err
		on_other	do_( ".$" )	s_take
		end
		in_ ( ".$" ) bgn_
			ons( " \t" )	do_( ".$_" )
			ons( "\n:" )	do_( ".$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
		in_( ".$_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '.' )	do_( "." )	s_add( " " )
							s_take
							*type = LOCALE;
			on_( '\n' )	do_( "_expr" )	REENTER
							TAB_CURRENT += TAB_SHIFT;
							*type = LOCALE;
							f_set( INFORMED )
			on_( ':' ) if ( !TAB_CURRENT && *type!=LOCALE ) {
CB_if_( NarrativeTake, mode, data ) {
					do_( ".id:" )	s_take
							TAB_SHIFT = 0; } }
			end
	//----------------------------------------------------------------------
	// bm_parse_cmd:	Narrative Declaration
	//----------------------------------------------------------------------
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "def" )	REENTER
		ons( "\"%(" )	do_( ".id:" )	REENTER
						s_add( ".this:" )
		on_separator	; // err
		on_other	do_( "def" )	s_take
		end
	in_( "def" ) bgn_
		ons( " \t\n" )	do_( "def_" )	REENTER
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( ".id:" ) bgn_
		ons( " \t" )	do_( same )
		on_( '"' )	do_( ":\"" )	s_take
		on_( '%' )	do_( ":%" )	s_take
		on_( '(' )	do_( ":_" )	REENTER
		end
		in_( ":\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '\\' )	do_( ":\"\\" )	s_take
			on_( '"' )	do_( "def_" )	s_take
			on_other	do_( same )	s_take
			end
			in_( ":\"\\" ) bgn_
				on_any		do_( "\"" )	s_take
				end
		in_( ":%" ) bgn_
			on_( '(' )	do_( ":_" )	s_take
							f_push( stack )
							f_set( SUB_EXPR|FIRST )
			end
		in_( ":_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' ) if ( is_f(INFORMED) && !is_f(LEVEL) ) {
					do_( "def_" )	REENTER
							f_reset( FIRST, 0 ) }
			on_( '(' ) if ( !is_f(INFORMED) ) {
					do_( same )	s_take
							f_push( stack )
							f_clr( INFORMED )
							f_set( LEVEL|FIRST ) }
			on_( '?' ) if ( is_f(SUB_EXPR) && !is_f(INFORMED|MARKED) ) {
					do_( same )	s_take
							f_set( INFORMED|MARKED ) }
			on_( '.' ) if ( is_f(LEVEL|SUB_EXPR) && !is_f(INFORMED) ) {
					do_( ":." )	}
			on_( ',' ) if ( is_f(LEVEL|SUB_EXPR) && are_f(INFORMED|FIRST) ) {
					do_( same )	s_take
							f_clr( FIRST|INFORMED ) }
			on_( ')' ) if ( is_f(LEVEL|SUB_EXPR) && is_f(INFORMED) ) {
					do_( same )	s_take
							f_pop( stack, INFORMED|MARKED ) }
			on_( '%' ) if ( is_f(LEVEL|SUB_EXPR) && !is_f(INFORMED) ) {
					do_( ":%_" )	s_take }
			on_separator	; // err
			on_other
				if ( is_f(LEVEL|SUB_EXPR) && !is_f(INFORMED) ) {
					do_( ":$" )	s_take
							f_set( INFORMED ) }
			end
		in_( ":$" ) bgn_
			on_separator	do_( ":_" )	REENTER
			on_other	do_( same )	s_take
			end
		in_( ":%_" ) bgn_
			on_( '%' )	do_( ":_" )	s_take
							f_set( INFORMED )
			end
		in_( ":." ) bgn_
			on_( '.' )	do_( ":.." )
			on_separator	do_( ":_" )	REENTER
							s_add( "." )
							f_set( INFORMED )
			on_other if ( is_f(SUB_EXPR) ) {
					do_( ":_" )	REENTER
							s_add( "." )
							f_set( INFORMED ) }
				else {	do_( ":.$" )	REENTER
							s_add( "." ) }
			end
			in_( ":.." ) bgn_
				on_( '.' ) if ( !is_f(FIRST) ) {
						if ( is_f(SUB_EXPR) && !is_f(LEVEL)) {
							do_( ":_" )	s_add( "..." )
									f_set( INFORMED ) }
						else {	do_( ":..." )	s_add( "..." )
									f_pop( stack, 0 )
									f_set( INFORMED ) } }
				on_other	do_( ":_" )	REENTER
								s_add( ".." )
								f_set( INFORMED )
				end
			in_( ":..." ) bgn_
				ons( " \t" )	do_( same )
				on_( ')' ) if ( !is_f(LEVEL) ) {
						do_( ":_" ) 	s_take }
				end
		in_( ":.$" ) bgn_
			ons( " \t:,)" )	do_( ":.$_" )	REENTER
			on_separator	; // err
			on_other	do_( same )	s_take
			end
			in_( ":.$_" ) bgn_
				ons( " \t" )	do_( same )
				on_( ',' ) if ( is_f(FIRST) ) {
						do_( ":.$," )	s_take
								f_clr( FIRST ) }
				on_( ':' )	do_( ":.$:" )	s_take
				on_( ')' )	do_( ":_" )	REENTER
								f_set( INFORMED )
				end
			in_( ":.$," ) bgn_
				ons( " \t" )	do_( same )
				on_( '.' )	do_( ":.$,." )
				on_other	do_( ":_" )	REENTER
				end
				in_( ":.$,." ) bgn_
					on_( '.' )	do_( ":.$,.." )
					on_separator	do_( ":_" )	REENTER
									s_add( "." )
									f_set( INFORMED )
					on_other	do_( ":.$" )	REENTER
									s_add( "." )
					end
				in_( ":.$,.." ) bgn_
					on_( '.' )	do_( ":_" )	s_add( "..." )
									f_set( INFORMED )
					on_other	do_( ":_" )	REENTER
									s_add( ".." )
									f_set( INFORMED )
					end
			in_( ":.$:" ) bgn_
				ons( " \t" )	do_( same )
				on_( '(' )	do_( ":_" )	REENTER
				on_separator	; // err
				on_other	do_( ":_" )	REENTER
				end
	in_( "def_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
CB_if_( ProtoSet, mode, data ) {
				do_( "base" )	TAB_CURRENT = 0;
						TAB_LAST = -1; }
		end
	//----------------------------------------------------------------------
	// bm_parse_cmd:	Narrative en / in / on / do / per / else
	//----------------------------------------------------------------------
	in_( "cmd" ) bgn_
		on_( '%' )	do_( "%_" )	s_take
		on_( '\n' ) if ( !s_cmp( "else" ) && !(*type&ELSE) ) {
				do_( "else" )	REENTER
						*type = ELSE; }
		on_separator if ( !s_cmp( "en" ) ) {
				do_( "cmd_" )	REENTER
						*type |= EN; }
			else if ( !s_cmp( "in" ) ) {
				do_( "cmd_" )	REENTER
						*type |= IN; }
			else if ( !s_cmp( "on" ) ) {
				do_( "cmd_" )	REENTER
						*type |= ON; }
			else if ( !s_cmp( "do" ) ) {
				do_( "cmd_" )	REENTER
						*type |= DO; }
			else if ( !s_cmp( "per" ) ) {
				do_( "cmd_" )	REENTER
						*type |= PER|ON; }
			else if ( !s_cmp( "else" ) && !(*type&ELSE) ) {
				do_( "else" )	REENTER
						*type = ELSE; }
		on_other	do_( same )	s_take
		end
		in_( "%_" ) bgn_
			on_( '(' )	do_( "_expr" )	s_take
							f_push( stack )
							f_reset( SUB_EXPR|FIRST, 0 )
							*type |= EN;
							TAB_CURRENT += TAB_SHIFT;
			end
		in_( "cmd_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	; // err
			on_other	do_( "_expr" )	REENTER
							TAB_CURRENT += TAB_SHIFT;
							s_reset( CNStringAll )
			end
		in_( "else" ) bgn_
			ons( " \t" )	do_( same )
			on_( '.' )	do_( "else." )	REENTER
							TAB_CURRENT += TAB_SHIFT;
							s_reset( CNStringAll )
			on_( '\n' )	do_( "_expr" )	REENTER
							TAB_CURRENT += TAB_SHIFT;
							s_reset( CNStringAll )
							f_set( INFORMED )
			on_separator	; // err
			on_other	do_( "cmd" )	REENTER
							s_reset( CNStringAll )
			end
		in_( "else." )
CB_if_( OccurrenceAdd, mode, data ) {
			bgn_ on_any
				do_( "." )	s_add( "." )
						TAB_LAST = TAB_CURRENT;
						TAB_CURRENT -= TAB_SHIFT;
						TAB_CURRENT++;
						TAB_BASE++;
				end }
	in_( "_expr" )
CB_if_( OccurrenceAdd, mode, data ) {
		bgn_ on_any
			do_( "expr" )	TAB_LAST = TAB_CURRENT;
					data->expr = 1;
			end }
	//----------------------------------------------------------------------
	// bm_parse_cmd:	Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
		on_( EOF )		errnum = ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState; data->state = state;
		in_( "def" )		errnum = data->errnum ? data->errnum : ErrSyntaxError;
		in_( "def_" )		errnum = data->errnum ? data->errnum : ErrSyntaxError;
		in_( ".id:" )		errnum = data->errnum ? data->errnum : ErrSyntaxError;
		in_( ":..." ) bgn_
			on_( ')' )	errnum = ErrEllipsisLevel;
			on_other	errnum = ErrSyntaxError;
			end
		in_( "cmd" )		errnum = ErrUnknownCommand;
		in_( "_expr" )		errnum = ErrIndentation; column=TAB_BASE;
		in_other		errnum = ErrSyntaxError;
	//----------------------------------------------------------------------
	// bm_parse_cmd:	Parser End
	//----------------------------------------------------------------------
	BMParseEnd
	data->errnum = errnum;
	data->flags = flags;
	if ( errnum ) bm_parse_report( data, mode, line, column );
	return state; }

char *
bm_parse_ui( int event, BMParseMode mode, BMParseData *data, BMParseCB cb )
{
	CNIO *io = data->io;

	// shared by reference
	listItem **	stack	= &data->stack.flags;
	CNString *	s	= data->string;
	int *		tab	= data->tab;
	int *		type	= &data->type;

	// shared by copy
	char *		state	= data->state;
	int		errnum	= 0;
	int		line	= io->line;
	int		column	= io->column;

	BMParseBegin( "bm_parse_ui", state, event, line, column )
	//----------------------------------------------------------------------
	// bm_parse_ui:		Parser Begin
	//----------------------------------------------------------------------
	in_( "base" ) bgn_
		on_( EOF )	do_( "" )
		on_( '\n' )	do_( "" )
		ons( " \t" )	do_( same )
		on_( '/' )	do_( "/" )	s_take
		on_separator	; // err
		on_other	do_( "cmd" )	s_take
		end
	in_( "/" ) bgn_
		on_( '\n' )	do_( "" )
		end
	in_( "cmd" ) bgn_
		ons( " \t" ) if ( !s_cmp("do" ) ) {
				do_( "expr" )	s_reset( CNStringAll )
						data->expr = 1; }
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	//----------------------------------------------------------------------
	// bm_parse_ui:		Error Handling
	//----------------------------------------------------------------------
	BMParseDefault
		on_( EOF )		errnum = ErrUnexpectedEOF;
		in_none_sofar		errnum = ErrUnknownState; data->state = state;
		in_( "cmd" ) bgn_
			on_( '\n' )	errnum = !s_cmp("do") ? ErrUnexpectedCR : ErrUnknownCommand;
			ons( " \t" )	errnum = ErrUnknownCommand;
			on_other	errnum = ErrSyntaxError;
			end
		in_other bgn_
			on_( '\n' )	errnum = ErrUnexpectedCR;
			on_( ' ' )	errnum = ErrUnexpectedSpace;
			on_other	errnum = ErrSyntaxError;
			end
	BMParseEnd
	data->errnum = errnum;
	if ( errnum ) bm_parse_report( data, mode, line, column );
	return state; }

//===========================================================================
//	bm_parse_report
//===========================================================================
#define err_case( err, msg ) \
	case err: fprintf( stderr, msg
#define _( arg ) \
	, arg ); break;
#define err_default( msg ) \
	default: fprintf( stderr, msg
#define _narg \
	); break;
void
bm_parse_report( BMParseData *data, BMParseMode mode, int l, int c )
{
	CNIO *io = data->io;
	char *src = (mode==BM_CMD) ? "B%" :
		    (mode==BM_STORY) ? "bm_parse" :
		    (mode==BM_LOAD) ? "bm_load" : "bm_read";
	char *stump = StringFinish( data->string, 0 );

	fprintf( stderr, "\x1B[1;33m" );
	if ( data->errnum==ErrUnknownState )
		fprintf( stderr, ">>>>> B%%::%s: ", src );
	else if ( mode!=BM_CMD )
		fprintf( stderr, "Error: %s: ", src );
	else {
		for (int i=1; i<c; i++ ) fprintf( stderr, "~" );
		fprintf( stderr, data->errnum==ErrSyntaxError ?
			"^\nB%%: " : "^\nB%%: error: " ); }

	if ( mode==BM_INPUT ) {
		switch ( data->errnum ) {
		err_case( ErrUnknownState, "unknown state \"%s\" <<<<<<\n" )_( data->state )
		err_case( ErrUnexpectedEOF, "in expression '%s' unexpected EOF\n" )_( stump )
		err_case( ErrUnexpectedCR, "in expression '%s' unexpected \\cr\n" )_( stump )
		err_case( ErrUnexpectedSpace, "unexpected ' '\n" )_narg
		err_default( "in expression '%s' syntax error\n" )_( stump ) } }
	else {
		if (( io->path )) fprintf( stderr, "in %s, ", io->path );
		if ( mode!=BM_CMD ) fprintf( stderr, "l%dc%d: ", l, c );
		switch ( data->errnum ) {
		err_case( ErrUnknownState, "unknown state \"%s\" <<<<<<\n" )_( data->state )
		err_case( ErrUnexpectedEOF, "unexpected EOF\n" )_narg
		err_case( ErrUnexpectedCR, "statement incomplete\n" )_narg
		err_case( ErrUnexpectedSpace, "unexpected ' '\n" )_narg
		err_case( ErrNarrativeEmpty, "narrative empty\n" )_narg
		err_case( ErrNarrativeNoEntry, "sub-narrative has no story entry\n" )_narg
		err_case( ErrNarrativeDoubleDef, "narrative already defined\n" )_narg
		err_case( ErrEllipsisLevel, "ellipsis usage not supported\n" )_narg
		err_case( ErrIndentation, "indentation error\n" )_narg
		err_case( ErrExpressionSyntaxError, "syntax error in expression\n" )_narg
		err_case( ErrInputScheme, "unsupported input scheme\n" )_narg
		err_case( ErrOutputScheme, "unsupported output scheme\n" )_narg
		err_case( ErrMarkDo, "do expression is marked\n" )_narg
		err_case( ErrMarkOn, "on ~( expression is marked\n" )_narg
		err_case( ErrMarkGuard, "'?' marked expression used as ternary guard\n" )_narg
		err_case( ErrMarkTernary, "'?' requires %%(_) around ternary expression\n" )_narg
		err_case( ErrMarkMultiple, "'?' already informed\n" )_narg
		err_case( ErrMarkNegated, "'?' negated\n" )_narg
		err_case( ErrEMarked, "'!' unspecified\n" )_narg
		err_case( ErrEMarkMultiple, "'!' already informed\n" )_narg
		err_case( ErrEMarkNegated, "'!' negated\n" )_narg
		err_case( ErrUnknownCommand, "unknown command '%s'\n" )_( stump  )
		err_case( ErrPerContrary, "per contrary not supported\n" )_narg
		err_default( "syntax error\n" )_narg } }

	fprintf( stderr, "\x1B[0m" ); }

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
	int l=io->line, c=io->column;
	fprintf( stderr, "\x1B[1;33m" );
	if ( mode!=BM_CMD ) {
		fprintf( stderr, "Warning: bm_parse: " );
		if (( io->path )) fprintf( stderr, "in %s, ", io->path );
		fprintf( stderr, "l%dc%d: ", l, c ); }
	else {
		for (int i=1; i<c; i++ ) fprintf( stderr, "~" );
		fprintf( stderr, data->errnum==ErrSyntaxError ?
			"^\nB%%: " : "^\nB%%: warning: " ); }
	switch ( errnum ) {
	err_case( WarnOutputFormat, "unsupported output format - using default \"%%_\"\n" )_narg
	err_case( WarnInputFormat, "unsupported input format - using default \"%%_\"\n" )_narg;
	err_case( WarnUnsubscribe, "'do term~<' results in instantiation\n" )_narg;
	default: break; }
	fprintf( stderr, "\x1B[0m" ); }

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
	data->expr = 0; }

void
bm_parse_exit( BMParseData *data )
{
	freeString( data->string );
	data->expr = 0; }

