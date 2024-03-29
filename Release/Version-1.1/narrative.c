#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "pair.h"
#include "narrative.h"
#include "narrative_private.h"

#define FIRST		1
#define DIRTY		2
#define FILTERED	4
#define SUB_EXPR	8
#define MARKED		16
#define PIPED		32

//===========================================================================
//	readStory
//===========================================================================
static int filterable( CNString * );

CNStory *
readStory( char *path )
/*
	creates narrative structure
		occurrence:=[ type, [ char *expression, sub:={ %occurrence } ] ]
	where expression format is
			: %e
		e	: %term
			| ( %e )
			| ( %e , %e )
			| ~ %e
			| * %e
			| %e : %e
			| \% ( %e )
			| \% ( %e , %e )
			| > %fmt : %e
			| %e : %fmt <
		term	: %/(\*|\.|\?|[A-Za-z0-9_]+)/
		fmt	: %/".*"/
*/
{
	FILE *file = fopen( path, "r" );
	if ( file == NULL ) {
		fprintf( stderr, "B%%: %s: no such file or directory\n", path );
		return NULL;
	}

	struct { listItem *level, *first, *occurrence; } stack; 
	memset( &stack, 0, sizeof(stack));
	CNOccurrence *occurrence, *sibling;

	int	tab = 0,
		last_tab = -1,
		tab_base = 0,
		indent,
		type,
		typelse = 0,
		informed = 0,
		sub_bgn = 0,
		level = 0;

	int	first = FIRST,	// level-dependent information, all
		dirty = 0,	// transported together on stack.first
		filtered = 0,
		sub_expr = 0,
		marked = 0,
		piped = 0;

	CNStory *story = NULL;
	CNNarrative *narrative = newNarrative();
	addItem( &stack.occurrence, narrative->root );
	CNString *s = newString(); // sequence to be informed

	CNParserData parser;
	memset( &parser, 0, sizeof(CNParserData) );
	parser.state = "base";
	parser.file = file;
	parser.line = 1;

	CNParserBegin( &parser )
	in_( "base" ) bgn_
		on_( '#' ) if ( column == 1 ) {	do_( "#" ) }
		on_( '+' ) if ( column == 1 ) {	do_( "+" ) tab_base++; }
		on_( '-' ) if ( column == 1 ) {	do_( "-" ) tab_base--; }
		on_( ':' ) if ( column == 1 ) {	do_( "^:" ) }
		on_( '\n' )	do_( same )	tab = 0;
		on_( '\t' )	do_( same )	tab++;
		on_( '.' )	do_( "^." )	indent = column;
		on_( '%' )	do_( "^%" )	indent = column;
		on_( 'i' )	do_( "i" )	indent = column;
		on_( 'o' )	do_( "o" )	indent = column;
		on_( 'd' )	do_( "d" )	indent = column;
		on_( 'e' )	do_( "e" )	indent = column;
		end
		in_( "#" ) bgn_
			on_( '\n' )	do_( "base" )
			on_other	do_( same )
			end
		in_( "+" ) bgn_
			on_( '+' )	do_( same )	tab_base++;
			on_other	do_( "base" )	REENTER
			end
		in_( "-" ) bgn_
			on_( '-' )	do_( same )	tab_base--;
			on_other	do_( "base" )	REENTER
			end
		in_( "^:" ) bgn_
			ons( " \t" )	do_( same )
			ons( "(\n" )	do_( "^:_" )	REENTER
							freeListItem( &stack.first );
							if ( add_narrative( &story, narrative ) ) {
								freeListItem( &stack.occurrence );
								narrative = newNarrative();
								addItem( &stack.occurrence, narrative->root );
							}
							type = PROTO;
			end
			in_( "^:_" ) bgn_
				on_( '\n' )	do_( "expr_" )	REENTER
								informed = 1;
				on_other	do_( "expr" )	REENTER
				end
		in_( "^." ) bgn_
			on_separator	; // err
			on_other	do_( "^._" )	StringAppend( s, '.' );
							StringAppend( s, event );
			end
			in_ ( "^._" ) bgn_
				ons( " \t" )	do_( "^.." ) 	StringAppend( s, ' ' );
				on_( '\n' )	do_( "^.." )	REENTER
				on_separator	; // err
				on_other	do_( same )	StringAppend( s, event );
				end
				in_( "^.." ) bgn_
					ons( " \t" )	do_( same )
					on_( '.' )	do_( "^." )
					on_( '\n' )	do_( "_expr" )	REENTER
									informed = 1;
									tab += tab_base;
									type = LOCAL;
					end
		in_( "^%" ) bgn_
			ons( " \t" )	do_( same )
			on_( '(' )	do_( "_expr" )	REENTER
							StringAppend( s, '%' );
							sub_bgn = 1;
							tab += tab_base;
							type = EN;
			end
		in_( "i" ) bgn_
			on_( 'n' )	do_( "in" )
			end
			in_( "in" ) bgn_
				ons( " \t" )	do_( "in_" )
				end
				in_( "in_" ) bgn_
					ons( " \t" )	do_( same )
					on_( '\n' )	; // err
					on_other	do_( "_expr" )	REENTER
									tab += tab_base;
									type = IN;
					end
		in_( "o" ) bgn_
			on_( 'n' )	do_( "on" )
			end
			in_( "on" ) bgn_
				ons( " \t")	do_( "on_" )
				end
				in_( "on_" ) bgn_
					ons( " \t" )	do_( same )
					on_( '\n' )	; // err
					on_other	do_( "_expr" )	REENTER
									tab += tab_base;
									type = ON;
					end
		in_( "d" ) bgn_
			on_( 'o' )	do_( "do" )
			end
			in_( "do" ) bgn_
				ons( " \t")	do_( "do_" )
				end
				in_( "do_" ) bgn_
					ons( " \t" )	do_( same )
					on_( '\n' )	; // err
					on_other	do_( "_expr" )	REENTER
									tab += tab_base;
									type = DO;
					end
		in_( "e" ) bgn_
			on_( 'l' )	do_( "el" )
			end
			in_( "el" ) bgn_
				on_( 's' )	do_( "els" )
				end
				in_( "els" ) bgn_
					on_( 'e' )	do_( "else" )
					end
					in_( "else" ) bgn_
						ons( " \t")	do_( "else_" )
						on_( '\n' )	do_( "_expr" )	REENTER
										informed = 1;
										tab += tab_base;
										type = ELSE;
						end
						in_( "else_" ) bgn_
							ons( " \t" )	do_( same )
							on_( '\n' )	do_( "_expr" )	REENTER
											informed = 1;
											tab += tab_base;
											type = ELSE;
							on_( '%' )	do_( "^%" )	typelse = 1;
							on_( 'i' )	do_( "i" )	typelse = 1;
							on_( 'o' )	do_( "o" )	typelse = 1;
							on_( 'd' )	do_( "d" )	typelse = 1;
							end
	in_( "_expr" ) REENTER
		if ( last_tab == -1 ) {
			// very first occurrence
			if ( !typelse && type!=ELSE ) {
				do_( "build" )
			}
		}
		else if ( tab == last_tab + 1 ) {
			CNOccurrence *parent = stack.occurrence->ptr;
			if ( !typelse && type!=ELSE &&
			    ( parent->type==ROOT || parent->type==ELSE ||
			      parent->type==IN || parent->type==ELSE_IN ||
			      parent->type==ON || parent->type==ELSE_ON )) {
				do_( "build" )
			}
		}
		else if ( tab <= last_tab ) {
			do_( "sibling" )	sibling = popListItem( &stack.occurrence );
						last_tab--;
		}

		in_( "sibling" ) REENTER
			if ( sibling->type == ROOT ) ; // err
			else if ( tab <= last_tab ) {
				do_( same )	sibling = popListItem( &stack.occurrence );
						last_tab--;
			}
			else if (( !typelse && type!=ELSE ) ||
				  sibling->type==IN || sibling->type==ELSE_IN ||
				  sibling->type==ON || sibling->type==ELSE_ON ) {
				do_( "build" )
			}

		in_( "build" ) REENTER
			do_( "add" )	last_tab = tab;
					occurrence = newOccurrence( typelse ?
						type==IN ? ELSE_IN :
						type==ON ? ELSE_ON :
						type==DO ? ELSE_DO :
						type==EN ? ELSE_EN :
						ROOT : type );
		in_( "add" ) REENTER
			do_( "expr" )	CNOccurrence *parent = stack.occurrence->ptr;
					addItem( &parent->data->sub, occurrence );
					addItem( &stack.occurrence, occurrence );
	in_( "expr" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
			if ( stack.level ) {
				do_( "expr^" )
			}
			else {	do_( "expr_" )	REENTER }
		on_( '>' )
			if ( type==DO && !StringInformed(s) ) {
				do_( ">" )	StringAppend( s, event );
						CNOccurrence *occurrence = stack.occurrence->ptr;
						occurrence->type = typelse ? ELSE_OUTPUT : OUTPUT;
			}
		on_( '{' )
			if ( type==DO && (level||(stack.level)) && !informed && !filtered && !sub_expr ) {
				do_( same )	StringAppend( s, event );
						add_item( &stack.level, level );
						if ( piped ) first |= PIPED;
						add_item( &stack.first, first );
						first = FIRST; level=piped=0;
			}
		on_( '}' )
			if ( informed && !level && (stack.level) ) {
				do_( "}" )	StringAppend( s, event );
						level = pop_item( &stack.level );
						first = pop_item( &stack.first );
						piped = first & PIPED;
						first &= FIRST;
			}
		on_( '|' )
			if ( type==DO && (level||(stack.level)) && !piped && informed && !filtered && !sub_expr ) {
				do_( same )	StringAppend( s, event );
						add_item( &stack.level, level );
						level = informed = 0;
						add_item( &stack.first, first );
						first = FIRST; piped = PIPED;
			}
		on_( '(' )
			if ( !informed ) {
				do_( same )	level++;
						StringAppend( s, event );
						first |= dirty | filtered | sub_expr | marked | piped;
						add_item( &stack.first, first );
						dirty = piped = 0;
						for ( ; sub_bgn; sub_bgn=0 ) {
							sub_expr = SUB_EXPR;
							marked = 0;
						}
						first = FIRST; informed = 0;
			}
		on_( ',' )
			if ( level && first && informed ) {
				do_( same )	StringAppend( s, event );
						first = informed = 0;
			}
			else if ( stack.level && first && informed && !piped ) {
				do_( same )	StringAppend( s, event );
						informed = 0;
			}
		on_( ':' )
			if ( informed && filterable(s) ) {
				do_( ":" )	StringAppend( s, event );
						filtered = FILTERED; informed = 0;
			}
		on_( ')' )
			if ( informed && piped ) {
				do_( same )	REENTER
						level = pop_item( &stack.level );
						first = pop_item( &stack.first );
						piped = 0;
			}
			else if ( level && informed ) {
				do_( same )	level--;
						StringAppend( s, event );
						first = pop_item( &stack.first );
						dirty = first & DIRTY;
						if ( dirty ) dirty_go( &dirty, s );
						filtered = first & FILTERED;
						sub_expr = first & SUB_EXPR;
						marked = first & MARKED;
						piped = first & PIPED;
						first &= FIRST;
			}
		on_( '/' )
			if ( type!=DO && !informed ) {
				do_( "/" )	StringAppend( s, event );
			}
		on_( '~' ) if ( !informed ) {	do_( "~" ) }
		on_( '?' ) if ( !informed ) {	do_( "?" ) }
		on_( '.' ) if ( !informed ) {	do_( "." ) }
		on_( '*' ) if ( !informed ) {	do_( "*" )	StringAppend( s, event ); }
		on_( '%' ) if ( !informed ) {	do_( "%" )	StringAppend( s, event ); }
		on_( '\'' ) if ( !informed ) {	do_( "char" )	StringAppend( s, event ); }
		on_separator	; // err
		on_other
			if ( !informed ) {	do_( "term" )	StringAppend( s, event ); }
		end
	in_( "expr^" ) bgn_
		on_( '#' )	do_( "_^#" )
		on_other	do_( "expr" )	REENTER
		end
		in_( "_^#" ) bgn_
			on_( '\n' )	do_( "expr" )
			on_other	do_( same )
			end
	in_( ">" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' )	do_( ">:" )	StringAppend( s, event );
		on_( '\"' )	do_( ">\"" )	StringAppend( s, event );
		end
		in_( ">:" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\n' )	do_( "expr_" )	REENTER
							informed = 1;
			ons( "{}" )	; //err
			on_other	do_( "expr" )	REENTER
			end
		in_( ">\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '\\' )	do_( ">\"\\" )	StringAppend( s, event );
			on_( '\"' )	do_( ">_" )	StringAppend( s, event );
			on_other	do_( same )	StringAppend( s, event );
			end
		in_( ">\"\\" ) bgn_
			on_any		do_( ">\"" )	StringAppend( s, event );
			end
		in_( ">_" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( ":" )	StringAppend( s, event );
			on_( '\n' )	do_( "expr_" )	REENTER
							informed = 1;
			end
	in_( "}" ) bgn_
		ons( " \t" )	do_( same )
		ons( ",)" )	do_( "expr" )	REENTER
		end
	in_( "~" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( "expr" )
		ons( "{}" )	; //err
		on_other	do_( "expr" )	REENTER
						StringAppend( s, '~' );
		end
	in_( "%" ) bgn_
		ons( " \t" )	do_( "%_" )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						informed = 1;
		ons( "?!" )	do_( "expr" )	StringAppend( s, event );
						informed = 1;
		on_( '(' )	do_( "expr" )	REENTER
						sub_bgn=1;
		end
		in_( "%_" ) bgn_
			ons( " \t" )	do_( same )
			ons( ":,)\n" )	do_( "expr" )	REENTER
							informed = 1;
			end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						informed = 1;
		ons( "{}" )	; //err
		on_other	do_( "expr" )	REENTER
		end
	in_( "?" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' )
			if ( !StringInformed(s) && ( type==IN || type==ON )) {
				do_( "expr" )	s_add( "?:" )
			}
			else if ( sub_expr && !marked ) {
				do_( "expr" )	REENTER
						StringAppend( s, '?' );
						marked = MARKED; informed = 1;
			}
		ons( ",)\n" )
			if ( sub_expr && !marked ) {
				do_( "expr" )	REENTER
						StringAppend( s, '?' );
						marked = MARKED; informed = 1;
			}
		end
	in_( ":" ) bgn_
		ons( " \t" )	do_( same )
		on_( '~' )	do_( ":~" )
		on_( '<' )
			if ( type==DO && !level ) {
				do_( ":_" )	REENTER
			}
		on_( '\"' )
			if ( type==DO && !level ) {
				do_( ":\"" )	StringAppend( s, event );
			}
		on_( '/' ) 	do_( "/" )	StringAppend( s, event );
		on_other	do_( "expr" )	REENTER
		end
		in_( ":~" ) bgn_
			ons( " \t" )	do_( same )
			on_( '~' )	do_( ":" )
			on_other	do_( "expr" )	REENTER
							StringAppend( s, '~' );
			end
		in_( ":\"" ) bgn_
			on_( '\t' )	do_( same )	s_add( "\\t" )
			on_( '\n' )	do_( same )	s_add( "\\n" )
			on_( '\\' )	do_( ":\"\\" )	StringAppend( s, event );
			on_( '\"' )	do_( ":_" )	StringAppend( s, event );
			on_other	do_( same )	StringAppend( s, event );
			end
		in_( ":\"\\" ) bgn_
			on_any		do_( ":\"" )	StringAppend( s, event );
			end
		in_( ":_" ) bgn_
			ons( " \t" )	do_( same )
			on_( '<' )	do_( "expr_" )	StringAppend( s, event );
							CNOccurrence *occurrence = stack.occurrence->ptr;
							occurrence->type = typelse ? ELSE_INPUT : INPUT;
							informed = 1;
			end
	in_( "." ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)\n" )	do_( "expr" )	REENTER
						StringAppend( s, '.' );
						informed = 1;
		on_( '(' )
			if ( type==PROTO ) ; // err
			else if (( narrative->proto )) {
				do_( "expr" )	REENTER
						dirty_set( &dirty, s );
			}
			else {	do_( "expr" )	REENTER }
		on_separator	; // err
		on_other
			if ( type==PROTO && !sub_expr && strmatch( "(,", StringAt(s) ) ) {
				do_( "expr" )	REENTER
						StringAppend( s, '.' );
			}
			else if (( narrative->proto )) {
				do_( "expr" )	REENTER
						dirty_set( &dirty, s );
			}
			else {	do_( "expr" )	REENTER }
		end
	in_( "/" ) bgn_
		on_( '/' )	do_( "expr" )	StringAppend( s, event );
						informed = 1;
		on_( '\\' )	do_( "/\\" )	StringAppend( s, event );
		on_( '[' )	do_( "/[" )	StringAppend( s, event );
		on_printable	do_( same )	StringAppend( s, event );
		end
		in_( "/\\" ) bgn_
			ons( "nt0\\./[]" ) do_( "/" )	StringAppend( s, event );
			on_( 'x' )	do_( "/\\x" )	StringAppend( s, event );
			end
		in_( "/\\x" ) bgn_
			on_xdigit	do_( "/\\x_" )	StringAppend( s, event );
			end
		in_( "/\\x_" ) bgn_
			on_xdigit	do_( "/" )	StringAppend( s, event );
			end
		in_( "/[" ) bgn_
			on_( '\\' )	do_( "/[\\" )	StringAppend( s, event );
			on_( ']' )	do_( "/" )	StringAppend( s, event );
			on_printable	do_( same )	StringAppend( s, event );
			end
		in_( "/[\\" ) bgn_
			ons( "nt0\\[]" ) do_( "/[" )	StringAppend( s, event );
			on_( 'x' )	do_( "/[\\x" )	StringAppend( s, event );
			end
		in_( "/[\\x" ) bgn_
			on_xdigit	do_( "/[\\x_" ) StringAppend( s, event );
			end
		in_( "/[\\x_" ) bgn_
			on_xdigit	do_( "/[" )	StringAppend( s, event );
			end
	in_( "char" ) bgn_
		on_( '\\' )	do_( "char\\" )	StringAppend( s, event );
		on_printable	do_( "char_" )	StringAppend( s, event );
		end
		in_( "char\\" ) bgn_
			on_escapable	do_( "char_" )	StringAppend( s, event );
			on_( 'x' )	do_( "char\\x" ) StringAppend( s, event );
			end
		in_( "char\\x" ) bgn_
			on_xdigit	do_( "char\\x_" ) StringAppend( s, event );
			end
		in_( "char\\x_" ) bgn_
			on_xdigit	do_( "char_" )	StringAppend( s, event );
			end
		in_( "char_" ) bgn_
			on_( '\'' )	do_( "expr" )	StringAppend( s, event );
							informed = 1;
			end
	in_( "term" ) bgn_
		on_separator	do_( "expr" )	REENTER
						dirty_go( &dirty, s );
						informed = 1;
		on_other	do_( same )	StringAppend( s, event );
		end
	in_( "expr_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
			if ( level || !informed ) ; // err
			else if ( type==PROTO && proto_set( narrative, s )) {
				do_( "base" )	informed = filtered = 0;
						tab_base = 0; last_tab = -1;
			}
			else {
				do_( "base" )	occurrence_set( stack.occurrence->ptr, s );
						informed = filtered = 0;
						typelse = tab = 0;
			}
		end

	CNParserDefault
		in_( "EOF" )
			if ( level ) {
				do_( "err" )	REENTER errnum = ErrUnexpectedEOF;
			}
			else {	do_( "" )	occurrence_set( stack.occurrence->ptr, s ); }

		in_( "err" )	do_( "" )	err_report( errnum, line, column );
						freeNarrative( narrative );
						narrative = NULL;
		on_( EOF ) bgn_
			in_( "base" )	do_( "" )	// occurrence's sequence has been set
			in_( "expr" )	do_( "EOF" )	REENTER
			in_( "term" )	do_( "EOF" )	REENTER
			in_( "term_" )	do_( "EOF" )	REENTER
			in_( ">_" )	do_( "EOF" )	REENTER
			in_( "_<" )	do_( "EOF" )	REENTER
			in_other	do_( "err" )	REENTER errnum = ErrUnexpectedEOF;
			end
		on_other bgn_
			in_none_sofar		fprintf( stderr, ">>>>>> B%%::CNParser: "
						"unknown state \"%s\" <<<<<<\n", state );
						errnum = ErrUnknownState;
			in_( "do >" )		errnum = ErrOutputScheme;
			in_( "do >_" )		errnum = ErrOutputScheme;
			in_( "_expr" )		errnum = ErrIndentation; column = indent;
			in_( "sibling" )	errnum = ErrIndentation; column = indent;
			in_( "?" )		errnum = ( marked ? ErrMarkMultiple : ErrMarkNoSub );
			in_( "expr" ) bgn_
				on_( '<' )	errnum = ErrInputScheme;
				on_other	errnum = ErrSequenceSyntaxError;
				end
			in_other bgn_
				on_( '\n' )	errnum = type == PROTO ? level ?
							ErrUnexpectedCR : ErrProtoRedundantArg :
							ErrUnexpectedCR;
				on_( ' ' )	errnum = ErrSpace;
				on_other	errnum = ErrSyntaxError;
				end
			end
			do_( "err" )	REENTER
	CNParserEnd
	fclose( file );

	freeListItem( &stack.occurrence );
	freeListItem( &stack.first );
	freeListItem( &stack.level );
	if ( narrative == NULL ) {
		while (( narrative = popListItem( &story ) ))
			freeNarrative( narrative );
	}
	else if ( add_narrative( &story, narrative ) ) {
		reorderListItem( &story );
	}
	else {
		if ( story == NULL ) {
			fprintf( stderr, "Warning: read_narrative: no narrative\n" );
		}
		freeNarrative( narrative );
	}
	freeString( s );
	return story;
}

static int
filterable( CNString *string )
/*
	Assumption: string is informed
	For now we don't allow expressions involving { } to be filtered
	as this would require bm_verify() to handle { }
*/
{
	int level = 0;
	// Reminder: we walk string backwards
	for ( listItem *i=string->data; i!=NULL; i=i->next ) {
		int event = (int) i->ptr;
		switch ( event ) {
		case ')': level++; break;
		case '(':
			if ( !level ) return 1;
			level--; break;
		case '|':
		case ':':
		case ',':
			if ( !level ) return 1;
			break;
		case '}':
			return 0;
		}
	}
	return 1;
}

//===========================================================================
//	parser_getc
//===========================================================================
static int preprocess( int event, int *mode, int *buffer, int *skipped );

int
parser_getc( CNParserData *data )
/*
	filters out comments and \cr line continuation from input
*/
{
	int event, skipped[ 2 ] = { 0, 0 };
	int buffer = data->buffer;
	do {
		if ( buffer ) { event=buffer; buffer=0; }
		else event = fgetc( data->file );
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

//===========================================================================
//	err
//===========================================================================
static void
err_report( CNNarrativeError errnum, int line, int column )
{
	switch ( errnum ) {
	case ErrUnknownState:
		fprintf( stderr, "Error: read_narrative: l%dc%d: state unknown\n", line, column );
		break;
	case ErrUnexpectedEOF:
		fprintf( stderr, "Error: read_narrative: l%d: unexpected EOF\n", line );
		break;
	case ErrSpace:
		fprintf( stderr, "Error: read_narrative: l%dc%d: unexpected ' '\n", line, column );
		break;
	case ErrUnexpectedCR:
		fprintf( stderr, "Error: read_narrative: l%dc%d: statement incomplete\n", line, column );
		break;
	case ErrIndentation:
		fprintf( stderr, "Error: read_narrative: l%dc%d: indentation error\n", line, column );
		break;
	case ErrSyntaxError:
		fprintf( stderr, "Error: read_narrative: l%dc%d: syntax error\n", line, column );
		break;
	case ErrSequenceSyntaxError:
		fprintf( stderr, "Error: read_narrative: l%dc%d: syntax error in expression\n", line, column );
		break;
	case ErrInputScheme:
		fprintf( stderr, "Error: read_narrative: l%dc%d: unsupported input scheme\n", line, column );
		break;
	case ErrOutputScheme:
		fprintf( stderr, "Error: read_narrative: l%dc%d: unsupported output scheme\n", line, column );
		break;
	case ErrMarkMultiple:
		fprintf( stderr, "Error: read_narrative: l%dc%d: '?' already informed\n", line, column );
		break;
	case ErrMarkNoSub:
		fprintf( stderr, "Error: read_narrative: l%dc%d: '?' out of scope\n", line, column );
		break;
	case ErrProtoRedundantArg:
		fprintf( stderr, "Error: read_narrative: l%dc%d: redundant .arg in narrative identifier\n", line, column );
		break;
	}
}

//===========================================================================
//	story
//===========================================================================
void
freeStory( CNStory *story )
{
	CNNarrative *n;
	while (( n = popListItem( &story ) ))
		freeNarrative( n );
}
int
story_output( FILE *stream, CNStory *story )
{
	for ( listItem *i=story; i!=NULL; i=i->next ) {
		CNNarrative *n = i->ptr;
		narrative_output( stream, n, 0 );
		fprintf( stream, "\n" );
	}
	return 1;
}
static int
add_narrative( listItem **story, CNNarrative *narrative )
{
	if (( narrative->root->data->sub )) {
		narrative_reorder( narrative );
		addItem( story, narrative );
		return 1;
	}
	char *proto = narrative->proto;
	if (( proto )) {
		fprintf( stderr, "Warning: read_narrative: "
			"narrative:\"%s\" empty - ignoring\n", proto );
		free( proto );
		narrative->proto = NULL;
	}
	return 0;
}

//===========================================================================
//	narrative
//===========================================================================
static CNNarrative *
newNarrative( void )
{
	return (CNNarrative *) newPair( NULL, newOccurrence( ROOT ) );
}
static void
freeNarrative( CNNarrative *narrative )
{
	if (( narrative )) {
		free( narrative->proto );
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative );
	}
}
static int
proto_set( CNNarrative *narrative, CNString *s )
/*
	validates that all .arg names are unique in proto
*/
{
	char *p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	narrative->proto = p;
	if ( p == NULL ) return 1;
	int success = 1;
	listItem *list = NULL;
	for ( ; ; p=p_prune( PRUNE_IDENTIFIER, p )) {
		for ( ; *p!='.' || is_separator(p[1]); p++ )
			if ( *p == '\0' ) goto RETURN;
		p++;
		for ( listItem *i=list; i!=NULL; i=i->next ) {
			if ( !strcomp( i->ptr, p, 1 ) ) {
				success = 0;
				goto RETURN;
			}
		}
		addItem( &list, p );
	}
RETURN:
	freeListItem( &list );
	return success;
}
static void
narrative_reorder( CNNarrative *narrative )
{
	if ( narrative == NULL ) return;
	CNOccurrence *occurrence = narrative->root;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->data->sub;
		if (( j )) { addItem( &stack, i ); i = j; }
		else {
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					reorderListItem( &occurrence->data->sub );
				}
				else {
					freeItem( i );
					return;
				}
			}
		}
	}
}
static int
narrative_output( FILE *stream, CNNarrative *narrative, int level )
{
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0;
	}
	if ( narrative->proto ) {
		fprintf( stream, ": %s\n", narrative->proto );
	}
	else fprintf( stream, ":\n" );
	CNOccurrence *occurrence = narrative->root;

	listItem *i = newItem( occurrence ), *stack = NULL;
#define TAB( level ) \
	for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );
	for ( ; ; ) {
		occurrence = i->ptr;
		CNOccurrenceType type = occurrence->type;
		char *expression = occurrence->data->expression;

		TAB( level );
		switch ( type ) {
		case ELSE:
			fprintf( stream, "else\n" );
			break;
		case ELSE_IN:
		case ELSE_ON:
		case ELSE_DO:
		case ELSE_EN:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "else " );
			break;
		default:
			break;
		}
		switch ( type ) {
		case ROOT:
		case ELSE:
			break;
		case IN:
		case ELSE_IN:
			fprintf( stream, "in %s\n", expression );
			break;
		case ON:
		case ELSE_ON:
			fprintf( stream, "on %s\n", expression );
			break;
		case EN:
		case ELSE_EN:
		case LOCAL:
			fprintf( stream, "%s\n", expression );
			break;
		case DO:
		case ELSE_DO:
			fprintf( stream, "do " );
			listItem *base=NULL; int count;
			for ( char *p=expression; *p; p++ ) {
				switch ( *p ) {
				case '{':
					add_item( &base, level );
					level++; count=0;
					fprintf( stream, "{\n" );
					TAB( level );
					break;
				case '}':
					level = pop_item( &base );
					fprintf( stream, "}" );
					break;
				case '(':
					count++;
					fprintf( stream, "%c", *p );
					break;
				case ')':
					count--;
					fprintf( stream, "%c", *p );
					break;
				case ',':
					fprintf( stream, "%c", *p );
					if ( base && !count ) {
						fprintf( stream, "\n" );
						TAB( level );
					}
					break;
				default:
					fprintf( stream, "%c", *p );
				}
			}
			fprintf( stream, "\n" );
			break;
		case INPUT:
		case OUTPUT:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "do %s\n", expression );
			break;
		}
		listItem *j = occurrence->data->sub;
		if (( j )) { addItem( &stack, i ); i = j; level++; }
		else {
			for ( ; ; ) {
				if (( i->next )) {
					i = i->next;
					occurrence = i->ptr;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					level--;
				}
				else {
					freeItem( i );
					return 0;
				}
			}
		}
	}
}

//===========================================================================
//	occurrence
//===========================================================================
static CNOccurrence *
newOccurrence( CNOccurrenceType type )
{
	union { int value; void *ptr; } icast;
	icast.value = type;
	Pair *pair = newPair( NULL, NULL );
	return (CNOccurrence *) newPair( icast.ptr, pair );
}
static void
freeOccurrence( CNOccurrence *occurrence )
{
	if ( occurrence == NULL ) return;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->data->sub;
		if (( j )) { addItem( &stack, i ); i = j; }
		else {
			for ( ; ; ) {
				char *expression = occurrence->data->expression;
				if (( expression )) free( expression );
				freePair((Pair *) occurrence->data );
				freePair((Pair *) occurrence );
				if (( i->next )) {
					i = i->next;
					break;
				}
				else if (( stack )) {
					i = popListItem( &stack );
					occurrence = i->ptr;
					freeListItem( &occurrence->data->sub );
				}
				else {
					freeItem( i );
					return;
				}
			}
		}
	}
}
static void
occurrence_set( CNOccurrence *occurrence, CNString *s )
{
	occurrence->data->expression = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
}

//===========================================================================
//	sequence
//===========================================================================
static void
dirty_set( int *dirty, CNString *s )
{
	*dirty = DIRTY;
	s_add( "(this," )
}
static void
dirty_go( int *dirty, CNString *s )
{
	if ( *dirty ) {
		StringAppend( s, ')' );
		*dirty = 0;
	}
}

