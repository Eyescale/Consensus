#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "string_util.h"
#include "narrative.h"
#include "narrative_private.h"

//===========================================================================
//	input
//===========================================================================
enum {
	COMMENT = 0,
	BACKSLASH,
	STRING,
	QUOTE
};
static int preprocess( int event, int *mode, int *buffer, int *skipped );
static int
input( FILE *file, int *mode, int *buffer, int *c, int *l )
/*
	filters out comments and \cr line continuation from input
*/
{
	int event, skipped[ 2 ] = { 0, 0 };
	do {
		if ( *buffer ) { event = *buffer; *buffer = 0; }
		else event = fgetc( file );
		event = preprocess( event, mode, buffer, skipped );
	}
	while ( !event );
	*c = ( skipped[ 1 ] ? skipped[ 0 ] : *c + skipped[ 0 ] );
	*l += skipped[ 1 ];
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
				skipped[ 0 ] += 2;
			}
			else if ( mode[ COMMENT ] == 4 ) {
				mode[ COMMENT ] = 0;
				skipped[ 0 ]++;
			}
			else skipped[ 0 ]++;
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
				skipped[ 0 ] += 2;
			}
			else if ( mode[ COMMENT ] == 3 ) {
				mode[ COMMENT ] = 4;
				skipped[ 0 ]++;
			}
			else if ( mode[ COMMENT ] == 4 ) {
				mode[ COMMENT ] = 3;
				skipped[ 0 ]++;
			}
			else skipped[ 0 ]++;
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
		case '\n':
			skipped[ 0 ] = 0;
			output = event;
		default:
			output = event;
	}
	return output;
}

//===========================================================================
//	readNarrative
//===========================================================================
#define FILTERED 2
static void add_item( listItem **, int );

CNNarrative *
readNarrative( char *path )
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
	if ( file == NULL ) return NULL;

	listItem *sequence = NULL;
	struct {
		listItem *occurrence;
		listItem *position;
		listItem *counter;
		listItem *marked;
	} stack = { NULL, NULL, NULL, NULL };
	CNOccurrence *occurrence, *sibling;

	int	tabmark,
		tab = 0,
		last_tab = -1,
		type,
		typelse = 0,
		first = 1,
		informed = 0,
		level = 0,
		counter = 0,
		marked = 0;

	CNNarrative *narrative = newNarrative();
	addItem( &stack.occurrence, narrative->root );

	CNParserBegin( file, input )
	in_( "base" ) bgn_
		on_( '#' )
			if ( tab == 0 ) {
				do_( "#" )
			}
		on_( '+' )
			if ( tab == 0 ) {
				do_( "+/-" )	tab++;
			}
		on_( '-' )
			if ( tab == 0 ) {
				do_( "+/-" )	tab--;
			}
		on_( '\n' )	do_( same )	tab = 0;
		on_( '\t' )	do_( same )	tab++;
		on_( 'i' )	do_( "i" )	tabmark = column;
		on_( 'o' )	do_( "o" )	tabmark = column;
		on_( 'd' )	do_( "d" )	tabmark = column;
		on_( 'e' )	do_( "e" )	tabmark = column;
		end
		in_( "#" ) bgn_
			on_( '\n' )	do_( "base" )
			on_other	do_( same )
			end
		in_( "+/-" ) bgn_
			on_( '+' )	do_( same )	tab++;
			on_( '-' )	do_( same )	tab--;
			on_other	do_( "base" )	REENTER
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
										type = ELSE;
						end
						in_( "else_" ) bgn_
							ons( " \t" )	do_( same )
							on_( '\n' )	do_( "_expr" )	REENTER
											type = ELSE;
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
			    ( parent->type==ROOT ||
			      parent->type==IN || parent->type==ON ||
			      parent->type==ELSE_IN || parent->type==ELSE_ON ||
			      parent->type==ELSE )) {
				do_( "build" )
			}
		}
		else if ( tab <= last_tab ) {
			do_( "sibling" )	sibling = popListItem( &stack.occurrence );
						last_tab--;
		}

		in_( "sibling" ) REENTER
			if ( sibling->type == ROOT )
				; // err
			else if ( tab <= last_tab ) {
				do_( same )	sibling = popListItem( &stack.occurrence );
						last_tab--;
			}
			else if (( !typelse && type!=ELSE ) ||
				  sibling->type==IN || sibling->type==ON ||
				  sibling->type==ELSE_IN || sibling->type==ELSE_ON ) {
				do_( "build" )
			}

		in_( "build" ) REENTER
			do_( "add" )	last_tab = tab;
					occurrence = newOccurrence( typelse ?
						type==IN ? ELSE_IN :
						type==ON ? ELSE_ON :
						type==DO ? ELSE_DO :
						ROOT : type );
		in_( "add" ) REENTER
			do_( "expr" )	CNOccurrence *parent = stack.occurrence->ptr;
					addItem( &parent->data->sub, occurrence );
					addItem( &stack.occurrence, occurrence );
	in_( "expr" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )	do_( "expr_" )	REENTER
		on_( '*' )	do_( "*" )
		on_( '%' )	do_( "%" )
		on_( '~' )	do_( same )	add_item( &sequence, event );
		on_( '>' )
			if (( type == DO ) && ( sequence == NULL )) {
				do_( ">" )	add_item( &sequence, event );
						CNOccurrence *occurrence = stack.occurrence->ptr;
						occurrence->type = typelse ? ELSE_OUTPUT : OUTPUT;
			}
		on_( '<' )
			if (( first & FILTERED ) && ( type == DO ) && ( level == 0 )) {
				do_( "expr_" )	add_item( &sequence, event );
						CNOccurrence *occurrence = stack.occurrence->ptr;
						occurrence->type = typelse ? ELSE_INPUT : INPUT;
			}
		on_( '(' )
			if ( !informed ) {
				do_( same )	level++; counter++;
						add_item( &sequence, event );
						add_item( &stack.position, first );
						first = 1; informed = 0;
			}
		on_( ',' )
			if ( first && informed && ( level > 0 )) {
				do_( same )	add_item( &sequence, event );
						first = 0; informed = 0;
			}
		on_( ':' )
			if ( informed ) {
				do_( same )	add_item( &sequence, event );
						if ( first ) first |= FILTERED;
						informed = 0;
			}
		on_( ')' )
			if ( informed && ( level > 0 )) {
				do_( same )	level--;
						add_item( &sequence, event );
						if ( counter ) counter--;
						else {
							counter = (int) popListItem( &stack.counter );
							marked = (int) popListItem( &stack.marked );
						}
						first = (int) popListItem( &stack.position );
			}
		on_( '?' )
			if ( !informed && !marked && (stack.counter)) {
				do_( "?." )	add_item( &sequence, event );
						marked = 1; informed = 1;
			}
		on_( '.' )
			if ( !informed ) {
				do_( "?." )	add_item( &sequence, event );
						informed = 1;
			}
		on_separator	; // err
		on_other
			if ( !informed ) {
				do_( "term" )	add_item( &sequence, event );
						informed = 1;
			}
		end
	in_( ">" ) bgn_
		ons( " \t" )	do_( same )
		on_( ':' )	do_( "expr" )	add_item( &sequence, event );
		on_( '\"' )	do_( ">\"" )	add_item( &sequence, event );
		end
		in_( ">\"" ) bgn_
			on_( '\t' )	do_( same )	add_item( &sequence, '\\' );
							add_item( &sequence, 't' );
			on_( '\n' )	do_( same )	add_item( &sequence, '\\' );
							add_item( &sequence, 'n' );
			on_( '\"' )	do_( ">_" )	add_item( &sequence, event );
			on_other	do_( same )	add_item( &sequence, event );
			end
		in_( ">_" ) bgn_
			ons( " \t" )	do_( same )
			on_( ':' )	do_( "expr" )	add_item( &sequence, event );
			on_( '\n' )	do_( "expr_" )	REENTER
			end
	in_( "%" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)" )	do_( "expr" )	REENTER
						add_item( &sequence, '%' );
						informed = 1;
		on_( '\n' )	do_( "expr_" )	REENTER
						add_item( &sequence, '%' );
		on_( '(' )	do_( "expr" )	REENTER
						add_item( &sequence, '%' );
						add_item( &stack.counter, counter );
						add_item( &stack.marked, marked );
						counter = marked = 0;
		end
	in_( "*" ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)" )	do_( "expr" )	REENTER
						add_item( &sequence, '*' );
						informed = 1;
		on_other	do_( "expr" )	REENTER
						add_item( &sequence, '*' );
		end
	in_( "?." ) bgn_
		ons( " \t" )	do_( same )
		ons( ":,)" )	do_( "expr" )	REENTER
		on_( '\n' )	do_( "expr_" )	REENTER
		end
	in_( "term" ) bgn_
		on_separator	do_( "expr" )	REENTER
		on_other	do_( same )	add_item( &sequence, event );
		end
	in_( "expr_" ) bgn_
		ons( " \t" )	do_( same )
		on_( '\n' )
			if ( level == 0 ) {
				do_( "base" )	occurrence_set( stack.occurrence->ptr, &sequence );
						typelse = tab = informed = 0;
			}
		end

	CNParserDefault
		in_( "EOF" )
			if ( level ) {
				do_( "err" )	REENTER errnum = ErrUnexpectedEOF;
			}
			else {	do_( "" )	occurrence_set( stack.occurrence->ptr, &sequence ); }

		in_( "err" )	do_( "" )	narrative_report( errnum, line, column, tabmark );
						freeListItem( &sequence );
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
			in_none_sofar		fprintf( stderr, ">>>>>> B%%::read_narrative: "
						"unknown state \"%s\" <<<<<<\n", state );
						errnum = ErrUnknownState;
			in_( "do >" )		errnum = ErrOutputScheme;
			in_( "do >_" )		errnum = ErrOutputScheme;
			in_( "_expr" )		errnum = ErrIndentation;
			in_( "sibling" )	errnum = ErrIndentation;
			in_( "expr" ) bgn_
				on_( '?' )	errnum = ( marked ? ErrMarkMultiple : ErrMarkNoSub );
				on_( '<' )	errnum = ErrInputScheme;
				on_other	errnum = ErrSequenceSyntaxError;
				end
			in_other bgn_
				on_( '\n' )	errnum = ErrUnexpectedCR;
				on_( ' ' )	errnum = ErrSpace;
				on_other	errnum = ErrSyntaxError;
				end
			end
			do_( "err" )	REENTER
	CNParserEnd
	fclose( file );
	freeListItem( &stack.occurrence );
	freeListItem( &stack.position );
	freeListItem( &stack.counter );
	freeListItem( &stack.marked );
	if ( narrative == NULL ) return NULL;
	else if ( narrative->root->data->sub == NULL )
		fprintf( stderr, "Warning: read_narrative: narrative empty\n" );
	else narrative_reorder( narrative );
	return narrative;
}
static void
add_item( listItem **stack, int value )
{
	union { int value; char *ptr; } icast;
	icast.value = value;
	addItem( stack, icast.ptr );
}

//===========================================================================
//	newNarrative / freeNarrative
//===========================================================================
CNNarrative *
newNarrative( void )
{
	union { int value; void *ptr; } icast;
	icast.value = 1; // init state
	return (CNNarrative *) newPair( icast.ptr, newOccurrence( ROOT ) );
}
void
freeNarrative( CNNarrative *narrative )
{
	if (( narrative )) {
		freeOccurrence( narrative->root );
		freePair((Pair *) narrative );
	}
}

//===========================================================================
//	narrative_reorder
//===========================================================================
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

//===========================================================================
//	readNarrative occurrence utilities
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
occurrence_set( CNOccurrence *occurrence, listItem **sequence )
{
	occurrence->data->expression = l2s( sequence, 0 );
}

//===========================================================================
//	narrative_report / narrative_output	(debug)
//===========================================================================
static void
narrative_report( CNNarrativeError errnum, int line, int column, int tabmark )
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
		fprintf( stderr, "Error: read_narrative: l%dc%d: indentation error\n", line, tabmark );
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
	}
}
int
narrative_output( FILE *stream, CNNarrative *narrative, int level )
{
	if ( narrative == NULL ) {
		fprintf( stderr, "Error: narrative_output: No narrative\n" );
		return 0;
	}
	CNOccurrence *occurrence = narrative->root;

	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		int type = occurrence->type;
		char *expression = occurrence->data->expression;

		for ( int k=0; k<level; k++ ) fprintf( stream, "\t" );
		switch ( type ) {
		case ELSE:
			fprintf( stream, "else\n" );
			break;
		case ELSE_IN:
		case ELSE_ON:
		case ELSE_DO:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "else " );
			break;
		default:
			break;
		}
		switch ( type ) {
		case IN:
		case ELSE_IN:
			fprintf( stream, "in %s\n", expression );
			break;
		case ON:
		case ELSE_ON:
			fprintf( stream, "on %s\n", expression );
			break;
		case DO:
		case INPUT:
		case OUTPUT:
		case ELSE_DO:
		case ELSE_INPUT:
		case ELSE_OUTPUT:
			fprintf( stream, "do %s\n", expression );
			break;
		default:
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
