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
bm_parse( int event, CNParserData *parser, BMReadMode mode )
{
	BMStoryData *data = parser->user_data;

	// shared by reference
	CNStory *story		= data->story;
	CNNarrative *narrative	= data->narrative;
	listItem **stack 	= &data->stack;
	CNString *s		= data->string;
	int *tab		= data->tab;

	// shared by copy
	CNOccurrence *
		occurrence	= data->occurrence;
	int	type		= data->type,
		flags		= data->flags;

	//----------------------------------------------------------------------
	// bm_parse:	Parser Begin
	//----------------------------------------------------------------------
	CNParserBegin( parser )
	in_( "base" )
if ( mode==CN_STORY ) {
		bgn_
			on_( EOF )	do_( "" )
			on_( '\t' )	do_( same )	TAB_CURRENT++;
			on_( '\n' )	do_( same )	TAB_CURRENT = 0;
			ons( ":#+-" )
				if ( column == 1 ) {
				bgn_
					on_( ':' )	do_( "def" )
					on_( '#' )	do_( "#" )
					on_( '+' )	do_( "+" ) TAB_SHIFT++;
					on_( '-' )	do_( "-" ) TAB_SHIFT--;
					end
				}
			on_separator	; // err
			on_other	do_( "cmd" )	s_take
							TAB_BASE = column;
			end
} else { 	bgn_
			on_( EOF )	do_( "" )
			ons( "\t\n" )	do_( same )
			ons( "#" )	do_( "#" )
			on_other	do_( "expr" )	REENTER
			end
}
	in_( "#" ) bgn_
		on_( '\n' )	do_( "base" )
		on_other	do_( same )
		end
	in_( "+" ) bgn_
		on_( '+' )	do_( same )	TAB_SHIFT++;
		on_other	do_( "base" )	REENTER
		end
	in_( "-" ) bgn_
		on_( '-' )	do_( same )	TAB_SHIFT--;
		on_other	do_( "base" )	REENTER
		end

	//----------------------------------------------------------------------
	// bm_parse:	Narrative Definition Begin
	//----------------------------------------------------------------------
	in_( "def" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "def." )	REENTER
		on_separator	; // err
		on_other	do_( "def." )	s_take
		end
	in_( "def." ) bgn_
		ons( " \t\n" )
			if ( story_add( parser->user_data ) ) {
				do_( "def_" )	REENTER
						freeListItem( stack );
						narrative = data->narrative = newNarrative();
						addItem( stack, narrative->root ); }
			else if ( !narrative->proto ) {
				do_( "def_" )	REENTER }
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( "def_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
			if ( proto_set( parser->user_data ) ) {
				do_( "base" )	TAB_BASE = 0;
						TAB_LAST = -1; }
		end

	//----------------------------------------------------------------------
	// bm_parse:	Narrative in / on / do / else Command Begin
	//----------------------------------------------------------------------
	in_( "cmd" ) bgn_
		ons( " \t" ) if ( !s_cmp( "in" ) ) {
				do_( "cmd_" )	type |= IN; }
			else if ( !s_cmp( "on" ) ) {
				do_( "cmd_" )	type |= ON; }
			else if ( !s_cmp( "do" ) ) {
				do_( "cmd_" )	type |= DO; }
			else if ( !s_cmp( "else" ) ) {
				do_( "else" )	type = ELSE; }
		on_( '\n' )
			if ( !s_cmp( "else" ) && !(type&ELSE) ) {
				do_( "else" )	REENTER
						type = ELSE; }
		on_separator	; // err
		on_other	do_( same )	s_take
		end
	in_( "cmd_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	; // err
		on_other	do_( "base_" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						s_clean( CNStringAll )
		end
	in_( "else" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "base_" )	REENTER
						TAB_CURRENT += TAB_SHIFT;
						f_set( INFORMED )
						s_clean( CNStringAll )
		on_other	do_( "cmd" )	REENTER
						s_clean( CNStringAll )
		end
	in_( "base_" ) REENTER
		if ( TAB_LAST == -1 ) {
			// very first occurrence
			if ( type & ELSE ) ; // err
			else {
				do_( "build" )
			} }
		else if ( TAB_CURRENT == TAB_LAST + 1 ) {
			if ( type & ELSE ) ; // err
			else {
				CNOccurrence *parent = data->stack->ptr;
				switch ( parent->data->type ) {
				case ROOT: case ELSE:
				case IN: case ELSE_IN:
			     	case ON: case ELSE_ON:
					do_( "build" );
				default:
					break;	// err
				} } }
		else if ( TAB_CURRENT <= TAB_LAST ) {
			do_( "sibling" )	occurrence = popListItem( stack );
						TAB_LAST--; }
	in_( "sibling" ) REENTER
		if ( occurrence->data->type == ROOT ) ; // err
		else if ( TAB_CURRENT <= TAB_LAST ) {
			do_( same )	occurrence = popListItem( stack );
					TAB_LAST--; }
		else if ( !(type&ELSE) || occurrence->data->type&(IN|ON) ) {
			do_( "build" ) }
	in_( "build" ) REENTER
		do_( "add" )	occurrence = newOccurrence( type );
	in_( "add" ) REENTER
		do_( "expr" )	CNOccurrence *parent = data->stack->ptr;
				addItem( &parent->sub, occurrence );
				addItem( stack, occurrence );
				TAB_LAST = TAB_CURRENT;

	//----------------------------------------------------------------------
	// bm_parse:	Expression Begin
	//----------------------------------------------------------------------
	in_( "expr" ) CND_reset bgn_
CND_if_( mode==CN_STORY, A )
		ons( " \t" )
			if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	type &= ~COMPOUND; }
			else {	do_( same ) }
		ons( "*%" )
			if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_set( INFORMED ) }
A:CND_else_( B )
		ons( " \t" )	do_( same )
		on_( '*' ) if ( !is_f(INFORMED) ) { do_( "*" )	s_take }
		on_( '%' ) if ( !is_f(INFORMED) ) { do_( "%" )	s_take }
		on_( '~' ) if ( !is_f(INFORMED) ) { do_( "~" ) }
		on_( '.' ) if ( !is_f(INFORMED) ) {
				do_( same )	s_take
						f_set( INFORMED ) }
		on_( '/' ) if ( !is_f(INFORMED) && !(type&DO) ) {
				do_( "/" )	s_take }
		on_( '?' ) if ( !is_f(INFORMED|MARKED|NEGATED) &&
				( is_f(SUB_EXPR) || type&(IN|ON) ) ) {
				do_( same )	s_take
						f_set( MARKED|INFORMED ) }
		on_( '>' ) if ( type&DO && s_empty ) {
				do_( ">" )	s_take
						type = (type&ELSE)|OUTPUT; }
		on_( ':' ) if ( s_empty ) {
				do_( "expr" )	s_take
						f_set( ASSIGN ) }
			else if ( are_f(ASSIGN|INFORMED) && !is_f(LEVEL) ) {
				if ( !is_f( FILTERED ) ) {
				do_( "expr" )	s_take
						f_clr( INFORMED )
						f_set( FILTERED ) } }
			else if ( is_f(INFORMED) && !(type&COMPOUND) ) {
				do_( ":" )	s_take
						f_clr( INFORMED )
						f_set( FILTERED ) }
B:CND_endif
		on_( '\n' ) if ( is_f(ASSIGN) && !is_f(FILTERED) )
				; // err
			else if ( is_f(INFORMED) && !is_f(LEVEL|SET) ) {
				do_( "expr_" )	type &= ~COMPOUND; }
			else if ( is_f(SET) ) { // allow \nl inside { }
				do_( "_^" ) }
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "(" )	s_take }
		on_( ',' ) if ( !is_f(INFORMED) )
				; // err
			else if ( are_f(FIRST|LEVEL) ) {
				do_( same )	s_take
						f_clr( FIRST|INFORMED ) }
			else if ( is_f( SET ) ) {
				do_( same )	s_take
						f_clr( INFORMED ) }
		on_( ')' ) if ( are_f( INFORMED|LEVEL ) ) {
				do_( same )	s_take
						f_pop( stack )
						f_set( INFORMED ) }
		on_( '|' ) if ( type&DO && is_f(INFORMED) && is_f(LEVEL|SET) &&
				!is_f(ASSIGN|FILTERED|SUB_EXPR) ) {
				do_( "|" )	s_take
						f_clr( INFORMED ) }
		on_( '{' ) if ( type&DO && is_f(LEVEL|SET) &&
				!is_f(INFORMED|ASSIGN|FILTERED|SUB_EXPR) ) {
				do_( same )	s_take
						f_push( stack )
						f_clr( LEVEL )
						f_set( SET ) }
		on_( '}' ) if ( are_f(INFORMED|SET) && !is_f(LEVEL) ) {
				do_( "}" )	s_take
						f_pop( stack )
						f_set( INFORMED )
						type |= COMPOUND; }
		on_( '\'' ) if ( !is_f(INFORMED) ) {
				do_( "char" )	s_take }
		on_separator	; // err
		on_other if ( !is_f(INFORMED) ) {
				do_( "term" )	s_take }
		end
CND_reset
	//----------------------------------------------------------------------
	// bm_parse:	Expression sub-states
	//----------------------------------------------------------------------
CND_ifn( mode==CN_STORY, C )
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
	in_( "%" ) bgn_
		ons( " \t" )	do_( "%_" )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						f_set( INFORMED )
		ons( "?!" )	do_( "expr" )	s_take
						f_set( INFORMED )
		on_( '(' ) if ( !is_f(INFORMED) ) {
				do_( "expr" )	s_take
						f_push( stack )
						f_clr( MARKED|NEGATED )
						f_set( FIRST|LEVEL|SUB_EXPR ) }
		end
		in_( "%_" ) bgn_
			ons( " \t" )	do_( same )
			ons( ":,)\n" )	do_( "expr" )	REENTER
							f_set( INFORMED )
			end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						f_set( INFORMED )
		ons( "{}" )	; //err
		on_other	do_( "expr" )	REENTER
		end
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '/' ) 	do_( "/" )	s_take
		on_( '<' ) if ( !is_f(ASSIGN) ) {
				do_( ":_<" )	s_take }
		on_( '\"' ) if ( type&DO && !is_f(LEVEL|SET|ASSIGN) ) {
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
							type = (type&ELSE)|INPUT;
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
		on_( ':' )
			if ( type & DO ) {
				do_( "(:" )	s_take }
		on_other	do_( "expr" )	REENTER
						f_push( stack )
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
	in_( "term" ) CND_reset	bgn_
CND_ifn( mode==CN_STORY, D )
		on_( '~' ) if ( type&DO && !is_f(LEVEL|SET|ASSIGN) ) {
				do_( "expr" )	s_take
						f_set( INFORMED ) }
D:CND_endif
		on_separator	do_( "expr" )	REENTER
						f_set( INFORMED )
		on_other	do_( same )	s_take
		end
	in_( "expr_" ) bgn_
		on_any		do_( "base" )	REENTER
						f_reset( FIRST );
						TAB_CURRENT = 0;
if ( mode==CN_STORY ) {				type = 0; }
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
		in_( "base_" )		errnum = ErrIndentation; column=TAB_BASE;
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
		freeStory( story );
		data->story = NULL;
	}
	data->occurrence = occurrence;
	data->type	= type;
	data->flags	= flags;
	return state;
}

//===========================================================================
//	bm_parser_report
//===========================================================================
void
bm_parser_report( BMParserError errnum, CNParserData *p, BMReadMode mode )
{
	BMStoryData *data = p->user_data;
	char *s = StringFinish( data->string, 0 );

	if ( mode==CN_INSTANCE )
	case_( errnum )
	_( ErrUnknownState )
		fprintf( stderr, ">>>>> B%%::CNParser: unknown state \"%s\" <<<<<<\n", p->state );
	_( ErrUnexpectedEOF )
		fprintf( stderr, "Error: bm_read: in expression '%s' unexpected EOF\n", s );
	_( ErrUnexpectedCR )
		fprintf( stderr, "Error: bm_read: in expression '%s' unexpected \\cr\n", s );
	_default
		fprintf( stderr, "Error: bm_read: in expression '%s' syntax error\n", s );
	_end_case

	else {
	char *	src = (mode==CN_INI) ? "bm_load" : "bm_read";
	int 	l = p->line, c = p->column;
	case_( errnum )
	_( ErrUnknownState )
		fprintf( stderr, ">>>>> B%%::CNParser: l%dc%d: unknown state \"%s\" <<<<<<\n", l, c, p->state );
	_( ErrUnexpectedEOF )
		fprintf( stderr, "Error: %s: l%d: unexpected EOF\n", src, l );
	_( ErrSpace )
		fprintf( stderr, "Error: %s: l%dc%d: unexpected ' '\n", src, l, c );
	_( ErrUnexpectedCR )
		fprintf( stderr, "Error: %s: l%dc%d: statement incomplete\n", src, l, c );
	_( ErrIndentation )
		fprintf( stderr, "Error: %s: l%dc%d: indentation error\n", src, l, c );
	_( ErrExpressionSyntaxError )
		fprintf( stderr, "Error: %s: l%dc%d: syntax error in expression\n", src, l, c );
	_( ErrInputScheme )
		fprintf( stderr, "Error: %s: l%dc%d: unsupported input scheme\n", src, l, c );
	_( ErrOutputScheme )
		fprintf( stderr, "Error: %s: l%dc%d: unsupported output scheme\n", src, l, c );
	_( ErrMarkMultiple )
		fprintf( stderr, "Error: %s: l%dc%d: '?' already informed\n", src, l, c );
	_( ErrMarkNegated )
		fprintf( stderr, "Error: %s: l%dc%d: '?' negated\n", src, l, c );
	_( ErrNarrativeEmpty )
		fprintf( stderr, "Error: %s: l%dc%d: empty narrative\n", src, l, c );
	_( ErrNarrativeDouble )
		fprintf( stderr, "Error: %s: l%dc%d: narrative already defined\n", src, l, c );
	_( ErrUnknownCommand )
		fprintf( stderr, "Error: %s: l%dc%d: unknown command '%s'\n", src, l, c, s );
	_default
		fprintf( stderr, "Error: %s: l%dc%d: syntax error\n", src, l, c );
	_end_case
	}
}

//===========================================================================
//	bm_parser_init / bm_parser_exit
//===========================================================================
int
bm_parser_init( CNParserData *parser, BMStoryData *data, FILE *stream, BMReadMode mode )
{
	memset( data, 0, sizeof(BMStoryData) );
	data->string = newString();
	switch ( mode ) {
	case CN_STORY:
		data->TAB_LAST = -1;
		data->flags = FIRST;
		data->narrative = newNarrative();
		data->occurrence = data->narrative->root;
		data->story = newRegistry( IndexedByName );
		addItem( &data->stack, data->occurrence );
		break;
	case CN_INI:
	case CN_INSTANCE:
		data->type = DO;
		data->flags = FIRST;
		break;
	}
	memset( parser, 0, sizeof(CNParserData) );
	parser->user_data = data;
	parser->state = "base";
	parser->stream = stream;
	parser->line = 1;

	return 1;
}

void *
bm_parser_exit( CNParserData *parser, BMReadMode mode )
{
	BMStoryData *data = parser->user_data;
	switch ( mode ) {
	case CN_STORY:
		freeString( data->string );
		freeListItem( &data->stack );
		freeListItem( &data->stack );
		CNStory *story = data->story;
		if ( !story_add( parser->user_data ) ) {
			if (( story )) {
				fprintf( stderr, "Error: read_narrative: unexpected EOF\n" );
				freeStory( story );
				story = NULL;
			}
			freeNarrative( data->narrative );
		}
		return story;
	case CN_INI:
		freeString( data->string );
		return ( parser->errnum ? parser->stream : NULL );
	case CN_INSTANCE: ;
		CNString *s = data->string;
		char *expression = StringFinish( s, 0 );
		StringReset( s, CNStringMode );
		freeString( s );
		return expression;
	}
}

//===========================================================================
//	cnParserGetc
//===========================================================================
static int preprocess( int event, int *mode, int *buffer, int *skipped );

int
cnParserGetc( CNParserData *data )
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

enum {
	COMMENT = 0,
	BACKSLASH,
	STRING,
	QUOTE
};
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
	else switch ( event ) {
		case '\\':
			mode[ BACKSLASH ] = 1;
			break;
		case '/':
			if ( !mode[ STRING ] && !mode[ QUOTE ] )
				mode[ COMMENT ] = 1;
			else
				output = event;
			break;
		case '"':
			if ( !mode[ QUOTE ] )
				mode[ STRING ] = !mode[ STRING ];
			output = event;
			break;
		case '\'':
			if ( !mode[ STRING ] )
				mode[ QUOTE ] = !mode[ QUOTE ];
			output = event;
			break;
		default:
			output = event;
	}
	return output;
}

