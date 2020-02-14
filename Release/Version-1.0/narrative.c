#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "string_util.h"
#include "narrative.h"
#include "narrative_private.h"

//===========================================================================
//	readNarrative
//===========================================================================
#define FILTERED 2

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

	Note: narrative statements ending with '*' are not accepted
*/
{
	FILE *file = fopen( path, "r" );
	if ( file == NULL ) return NULL;

	Sequence *sequence = NULL;
	struct {
		listItem *occurrence;
		listItem *sequence;
		listItem *position;
		listItem *sub_expression;
	} stack = { NULL, NULL, NULL, NULL };

	int	tabmark,
		tab = 0,
		last_tab = -1,
		type,
		typelse = 0,
		first = 1,
		informed = 0,
		level = 0;

	CNNarrative *narrative = newNarrative();
	addItem( &stack.occurrence, narrative->root );

	CNParserBegin( file )
	in_( "err" )	do_( "" )	narrative_report( errnum, line, column, tabmark );
					freeSequence( &sequence );
					freeNarrative( narrative );
					narrative = NULL;
					errnum = 0;
	in_( "out" )
		if ( level == 0 ) {
			do_( "" )	occurrence_set( stack.occurrence->ptr, &sequence );
		}
		else {	do_( "err" )	errnum = ErrUnexpectedEOF; }
	on_( EOF ) bgn_
		in_( "base" )	do_( "" )	// occurrence's sequence has been set
		in_( "/*" )	do_( "" )	// idem
		in_( "/**" )	do_( "" )	// idem
		in_( "/**/" )	do_( "" )	// idem
		in_( "expr" )	do_( "out" )	REENTER
		in_( "term" )	do_( "out" )	REENTER
		in_( "term_" )	do_( "out" )	REENTER
		in_( ">_" )	do_( "out" )	REENTER
		in_( "_<" )	do_( "out" )	REENTER
		in_( "//" )	do_( "out" )	REENTER
		in_( "expr_//" ) do_( "out" )	REENTER
		in_other	do_( "err" )	errnum = ErrUnexpectedEOF;
		end
	in_( "base" ) bgn_
		on_( '#' )
			if ( tab ) {
				do_( "err" )	errnum = ErrSyntaxError;
			} else	do_( "#" )
		on_( '+' )
			if ( tab ) {
				do_( "err" )	errnum = ErrSyntaxError;
			}
			else {	do_( "+/-" )	tab++; }
		on_( '-' )
			if ( tab ) {
				do_( "err" )	errnum = ErrSyntaxError;
			}
			else {	do_( "+/-" )	tab--; }
		on_( '\n' )	do_( same )	tab = 0;
		on_( '\t' )	do_( same )	tab++;
		on_( ' ' )	do_( "err" )	errnum = ErrSpace;
		on_( '/' )	do_( "/" )	tab = 0;
		on_( 'i' )	do_( "i" )	tabmark = column;
		on_( 'o' )	do_( "o" )	tabmark = column;
		on_( 'd' )	do_( "d" )	tabmark = column;
		on_( 'e' )	do_( "e" )	tabmark = column;
		on_other	do_( "err" )	errnum = ErrSyntaxError;
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
		in_( "/" ) bgn_
			on_( '/' )	do_( "//" )
			on_( '*' )	do_( "/*" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "//" ) bgn_
				on_( '\n' )	do_( "base" )
				on_other	do_( same )
				end
			in_( "/*" ) bgn_
				on_( '*' )	do_( "/**" )
				on_other	do_( same )
				end
				in_( "/**" ) bgn_
					on_( '/' )	do_( "/**/" )
					on_other	do_( "/*" )
					end
					in_( "/**/" ) bgn_
						on_( '\n' )	do_( "base" )
						on_( '\t' )	do_( same )
						on_( ' ' )	do_( same )
						on_other	do_( "err" )	errnum = ErrSyntaxError;
						end
		in_( "i" ) bgn_
			on_( 'n' )	do_( "in" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "in" ) bgn_
				on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
				on_( '\t' )	do_( "in_" )
				on_( ' ' )	do_( "in_" )
				on_other	do_( "err" )	errnum = ErrSyntaxError;
				end
				in_( "in_" ) bgn_
					on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
					on_( '/' )	do_( "err" )	errnum = ErrSyntaxError;
					on_( '\t' )	do_( same )
					on_( ' ' )	do_( same )
					on_other	do_( "_expr" )	REENTER
									type = IN;
					end
		in_( "o" ) bgn_
			on_( 'n' )	do_( "on" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "on" ) bgn_
				on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
				on_( '\t' )	do_( "on_" )
				on_( ' ' )	do_( "on_" )
				on_other	do_( "err" )	errnum = ErrSyntaxError;
				end
				in_( "on_" ) bgn_
					on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
					on_( '/' )	do_( "err" )	errnum = ErrSyntaxError;
					on_( '\t' )	do_( same )
					on_( ' ' )	do_( same )
					on_other	do_( "_expr" )	REENTER
									type = ON;
					end
		in_( "d" ) bgn_
			on_( 'o' )	do_( "do" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "do" ) bgn_
				on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
				on_( '\t' )	do_( "do_" )
				on_( ' ' )	do_( "do_" )
				on_other	do_( "err" )	errnum = ErrSyntaxError;
				end
				in_( "do_" ) bgn_
					on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
					on_( '/' )	do_( "err" )	errnum = ErrSyntaxError;
					on_( '\t' )	do_( same )
					on_( ' ' )	do_( same )
					on_( '>' )	do_( "do >" )	add_item( &sequence, event );
					on_other	do_( "_expr" )	REENTER
									type = DO;
					end
				in_( "do >" ) bgn_
					on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
					on_( '\t' )	do_( same )
					on_( ' ' )	do_( same )
					on_( ':' )	do_( "_expr" )	type = OUTPUT;
									add_item( &sequence, event );
					on_( '\"' )	do_( "do >\"" )	add_item( &sequence, event );
					on_other	do_( "err" )	errnum = ErrOutputScheme;
					end
					in_( "do >\"" ) bgn_
						on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
						on_( '\t' )	do_( same )	add_item( &sequence, '\\' );
										add_item( &sequence, 't' );
						on_( '\"' )	do_( "do >_" )	add_item( &sequence, event );
						on_( '\\' )	do_( "do >\"\\" ) add_item( &sequence, event );
						on_other	do_( same )	add_item( &sequence, event );
						end
						in_( "do >\"\\" ) bgn_
							on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
							on_other	do_( "do >\"" )	add_item( &sequence, event );
							end
					in_( "do >_" ) bgn_
						on_( '\n' )	do_( "_expr" )	REENTER
										type = OUTPUT;
						on_( ':' )	do_( "_expr" )	type = OUTPUT;
										add_item( &sequence, event );
						on_( '\t' )	do_( same )
						on_( ' ' )	do_( same )
						on_other	do_( "err" )	errnum = ErrOutputScheme;
						end
		in_( "e" ) bgn_
			on_( 'l' )	do_( "el" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "el" ) bgn_
				on_( 's' )	do_( "els" )
				on_other	do_( "err" )	errnum = ErrSyntaxError;
				end
				in_( "els" ) bgn_
					on_( 'e' )	do_( "else" )
					on_other	do_( "err" )	errnum = ErrSyntaxError;
					end
					in_( "else" ) bgn_
						on_( '\n' )	do_( "_expr" )	REENTER
										type = ELSE;
						on_( '\t')	do_( "else_" )
						on_( ' ')	do_( "else_" )
						on_( '/')	do_( "else/" )
						on_other	do_( "err" )	errnum = ErrSyntaxError;
						end
						in_( "else_" ) bgn_
							on_( '\n' )	do_( "_expr" )	REENTER
											type = ELSE;
							on_( '\t' )	do_( same )
							on_( ' ' )	do_( same )
							on_( 'i' )	do_( "i" )	typelse = 1;
							on_( 'o' )	do_( "o" )	typelse = 1;
							on_( 'd' )	do_( "d" )	typelse = 1;
							on_( '/' )	do_( "else/" )
							on_other	do_( "err" )	errnum = ErrSyntaxError;
							end
						in_( "else/" ) bgn_
							on_( '/' )	do_( "else//" )
							on_other	do_( "err" )	errnum = ErrSyntaxError;
							end
							in_( "else//" ) bgn_
								on_( '\n' )	do_( "_expr" )	REENTER
												type = ELSE;
								on_other	do_( same )
								end
	in_( "_expr" )	if ( narrative_build( &stack.occurrence, type, typelse, &last_tab, tab ) ) {
				do_( "expr" )	REENTER
			}
			else {	do_( "err" )	errnum = ErrIndentation; }
	in_( "expr" ) bgn_
		on_( '\n' )	do_( "expr_" )	REENTER
		on_( '/' )	do_( "expr_" )	REENTER
		on_( '\t' )	do_( same )
		on_( ' ' )	do_( same )
		on_( '*' )	do_( "*" )
		on_( '%' )	do_( "%" )
		on_( '~' )	do_( same )	add_item( &sequence, event );
		on_( '(' )
			if ( !informed ) {
				do_( same )	level++;
						push_position( &stack.position, first );
						push_sub( stack.sub_expression );
						add_item( &sequence, event );
						first = 1; informed = 0;
			}
			else {	do_( "err" )    errnum = ErrSequenceSyntaxError; }
		on_( ',' )
			if ( first && informed && ( level > 0 )) {
				do_( same )	add_item( &sequence, event );
						first = 0; informed = 0;
			}
			else {	do_( "err" )	errnum = ErrSequenceSyntaxError; }
		on_( ':' )
			if ( informed ) {
				do_( same )	add_item( &sequence, event );
						if ( first ) first |= FILTERED;
						informed = 0;
			}
			else {	do_( "err" )	errnum = ErrSequenceSyntaxError; }
		on_( '<' )
			if (( first & FILTERED ) && ( type == DO ) && ( level == 0 )) {
				do_( "expr_" )	add_item( &sequence, event );
						CNOccurrence *occurrence = stack.occurrence->ptr;
						occurrence->type = typelse ? ELSE_INPUT : INPUT;
			}
			else {	do_( "err" )	errnum = ErrInputScheme; }
		on_( ')' )
			if ( informed && ( level > 0 )) {
				do_( same )	level--;
						add_item( &sequence, event );
						pop_sub( &stack.sub_expression );
						first = (int) popListItem( &stack.position );
			}
			else {	do_( "err" )	errnum = ErrSequenceSyntaxError; }
		on_( '?' )
			if ( !informed && tag_sub( stack.sub_expression ) ) {
				do_( "?." )	add_item( &sequence, event );
						informed = 1;
			}
			else {	do_( "err" )	errnum = (( stack.sub_expression ) ? ErrMarkMultiple : ErrMarkNoSub ); }
		on_( '.' )
			if ( !informed ) {
				do_( "?." )	add_item( &sequence, event );
						informed = 1;
			}
			else {	do_( "err" )	errnum = ErrSequenceSyntaxError; }
		on_( '/' )	do_( "expr_" )	REENTER
		on_separator	do_( "err" )	errnum = ErrSequenceSyntaxError;
		on_other
			if ( !informed ) {
				do_( "term" )	add_item( &sequence, event );
						informed = 1;
			}
			else {	do_( "err" )	errnum = ErrSequenceSyntaxError; }
		end
		in_( "%" ) bgn_
			on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
			on_( '(' )	do_( "expr" )	REENTER
							addItem( &stack.sub_expression, NULL );
							add_item( &sequence, '%' );
			on_other	do_( "err" )	errnum = ErrSequenceSyntaxError;
			end
		in_( "*" ) bgn_
			on_( '\n' )	do_( "err" )	errnum = ErrUnexpectedCR;
			on_( '\t' )	do_( same )
			on_( ' ' )	do_( same )
			ons( ":,)" )	do_( "expr" )	REENTER
							add_item( &sequence, '*' );
							informed = 1;
			on_other	do_( "expr" )	REENTER
							add_item( &sequence, '*' );
			end
		in_( "?." ) bgn_
			on_( '\n' )	do_( "expr_" )	REENTER
			on_( '/' )	do_( "expr_" )	REENTER
			on_( '\t' )	do_( same )
			on_( ' ' )	do_( same )
			ons( ":,)" )	do_( "expr" )	REENTER
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
		in_( "term" ) bgn_
			on_separator	do_( "expr" )	REENTER
			on_other	do_( same )	add_item( &sequence, event );
			end
	in_( "expr_" ) bgn_
		on_( '\n' )
			if ( level == 0 ) {
				do_( "base" )	occurrence_set( stack.occurrence->ptr, &sequence );
						typelse = tab = informed = 0;
			}
			else {	do_( "err" )	errnum = ErrUnexpectedCR; }
		on_( '/' )
			if ( level == 0 ) {
				do_( "expr_/" )
			}
			else {	do_( "err" )	errnum = ErrSyntaxError; }
		on_( '\t' )	do_( same )
		on_( ' ' )	do_( same )
		on_other	do_( "err" )	errnum = ErrSyntaxError;
		end
		in_( "expr_/" ) bgn_
			on_( '/' )	do_( "expr_//" )
			on_other	do_( "err" )	errnum = ErrSyntaxError;
			end
			in_( "expr_//" ) bgn_
				on_( '\n' )	do_( "expr_" )	REENTER
				on_other	do_( same )
				end
	CNParserEnd
	fclose( file );
	freeListItem( &stack.occurrence );
	freeListItem( &stack.sequence );
	freeListItem( &stack.position );
	freeListItem( &stack.sub_expression );
	if ( narrative == NULL ) return NULL;
	else if ( narrative->root->data->sub == NULL )
		fprintf( stderr, "Warning: read_narrative: narrative empty\n" );
	else narrative_reorder( narrative );
	return narrative;
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
//	narrative_build
//===========================================================================
static int is_action( int positive, int type );
static int true_type( int type, int typelse );

#define POSITIVE 1
#define ANY 0

static int
narrative_build( listItem **stack, int type, int typelse, int *prev, int tabs )
{
	int success = 0;
#ifdef DEBUG
	fprintf( stderr, "narrative_build: level %d->%d\n", *prev, tabs );
#endif
	if ( *prev < 0 ) {
		if ( typelse ) return 0;
		switch ( type ) {
		case IN:
		case ON:
			break;
		default:
			if ( !is_action( POSITIVE, type ) )
				return 0;
		}
		success = 1;
	}
	else {
		int delta = tabs - *prev;
		if ( delta <= 0 ) {
			CNOccurrence *last = popListItem( stack );
			while ( delta++ < 0 ) {
				last = popListItem( stack );
				if ( last->type == ROOT )
					return 0;
			}
			if (( last->type == ELSE ) || is_action( ANY, last->type )) {
				if ( typelse || ( type == ELSE ))
					return 0;
			}
			else type = true_type( type, typelse );
			success = 1;
		}
		else if (( delta == 1 ) && !(( type == ELSE ) || typelse )) {
			CNOccurrence *last = (*stack)->ptr;
			success = !is_action( ANY, last->type );
		}
	}
	if ( success ) {
		*prev = tabs;
		CNOccurrence *last = (*stack)->ptr;
		CNOccurrence *occurrence = newOccurrence( type );
		addItem( &last->data->sub, occurrence );
		addItem( stack, occurrence );
	}
	return success;
}

static int
is_action( int positive, int type )
{
	switch ( type ) {
	case ELSE_DO:
	case ELSE_INPUT:
	case ELSE_OUTPUT:
		if ( positive == POSITIVE )
			break;
	case DO:
	case INPUT:
	case OUTPUT:
		return 1;
	}
	return 0;
}
static int
true_type( int type, int typelse )
{
	if ( typelse )
		switch ( type ) {
		case IN: type = ELSE_IN; break;
		case ON: type = ELSE_ON; break;
		case DO: type = ELSE_DO; break;
		case INPUT: type = ELSE_INPUT; break;
		case OUTPUT: type = ELSE_OUTPUT; break;
		}
	return type;
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
occurrence_set( CNOccurrence *occurrence, Sequence **sequence )
{
	occurrence->data->expression = l2s( sequence, 0 );
}

//===========================================================================
//	readNarrative sequence utilities
//===========================================================================
static void
push_position( listItem **stack, int first )
{
	union { int value; char *ptr; } icast;
	icast.value = first;
	addItem( stack, icast.ptr );
}
static void
add_item( Sequence **sequence, int event )
{
	union { int value; char *ptr; } icast;
	icast.value = event;
	addItem( sequence, icast.ptr );
}
static void
freeSequence( Sequence **sequence )
{
	freeListItem( sequence );
}
static void
push_sub( listItem *stack )
{
	union { int value; void *ptr; } icast;
	if (( stack )) {
		int level = (int) stack->ptr;
		if ( level < 0 ) level--;
		else level++;
		icast.value = level;
		stack->ptr = icast.ptr;
	}
}
static void
pop_sub( listItem **stack )
{
	union { int value; void *ptr; } icast;
	if (( *stack )) {
		int level = (int) (*stack)->ptr;
		if ( level < 0 ) level++;
		else level--;
		if ( level ) {
			icast.value = level;
			(*stack)->ptr = icast.ptr;
		}
		else popListItem( stack );
	}
}
static int
tag_sub( listItem *stack )
/*
	allows only one '?' per sub-expression
*/
{
	union { int value; void *ptr; } icast;
	if (( stack )) {
		int level = (int) stack->ptr;
		if ( level < 0 ) return 0;
		icast.value = -level;
		stack->ptr = icast.ptr;
		return 1;
	}
	return 0;
}

//===========================================================================
//	narrative_report / narrative_output	(debug)
//===========================================================================
static void
narrative_report( CNNarrativeError errnum, int line, int column, int tabmark )
{
	switch ( errnum ) {
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
