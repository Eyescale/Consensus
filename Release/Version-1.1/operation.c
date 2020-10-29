#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "operation.h"
#include "narrative.h"
#include "traversal.h"

// #define DEBUG

static int in_condition( char *, CNDB * );
static int on_event( char *, CNDB *, int init );
static int do_action( char *, CNDB * );
static int do_input( char *, CNDB * );
static int do_output( char *, CNDB * );

//===========================================================================
//	cnOperate
//===========================================================================
#define ctrl(e)	case e:	if ( passed ) { j = NULL; break; }

int
cnOperate( CNNarrative *narrative, CNDB *db )
/*
	lets db process one narrative frame
	returns 0 upon narrative exit, 1 otherwise
*/
{
#ifdef DEBUG
fprintf( stderr, "=============================\n" );
#endif
	if ( narrative == NULL ) return 0;

	CNOccurrence *occurrence = narrative->root;
	if ( occurrence->data->sub == NULL ) return 0;

	int init = narrative->init;
	narrative->init = 0;

	int passed = 1, active = 1;
	listItem *i = newItem( occurrence ), *stack = NULL;
	for ( ; ; ) {
		occurrence = i->ptr;
		listItem *j = occurrence->data->sub;
		switch ( occurrence->type ) {
		ctrl(ELSE) case ROOT:
			passed = 1;
			break;
		ctrl(ELSE_IN) case IN:
			passed = in_condition( occurrence->data->expression, db );
			break;
		ctrl(ELSE_ON) case ON:
			passed = on_event( occurrence->data->expression, db, init );
			break;
		ctrl(ELSE_DO) case DO:
			active &= do_action( occurrence->data->expression, db );
			if ( !active ) goto RETURN;
			break;
		ctrl(ELSE_INPUT) case INPUT:
			do_input( occurrence->data->expression, db );
			break;
		ctrl(ELSE_OUTPUT) case OUTPUT:
			do_output( occurrence->data->expression, db );
			break;
		}
		if (( j && passed )) {
			addItem( &stack, i );
			i = j; continue;
		}
		for ( ; ; ) {
			if (( i->next )) {
				i = i->next;
				occurrence = i->ptr;
				break;
			}
			else if (( stack )) {
				i = popListItem( &stack );
				passed = 1; // otherwise we would not be here
			}
			else goto RETURN;
		}
	}
RETURN:
	if ( !active ) narrative->init = 1;
	while (( stack )) {
		i = popListItem( &stack );
	}
	freeItem( i );
	return active;
}

//===========================================================================
//	cnUpdate
//===========================================================================
void
cnUpdate( CNDB *db )
{
	db_update( db );
}

//===========================================================================
//	in_condition
//===========================================================================
static int
in_condition( char *expression, CNDB *db )
{
	// fprintf( stderr, "in condition: %s\n", expression );
	return db_feel( expression, db, DB_CONDITION );
}

//===========================================================================
//	on_event
//===========================================================================
static int test_release( char ** );

static int
on_event( char *expression, CNDB *db, int init )
{
	// fprintf( stderr, "on_event: %s\n", expression );
	if ( !strcmp( expression, "init" ) )
		return init;
	else if ( test_release( &expression ) )
		return db_feel( expression, db, DB_RELEASED );
	else
		return db_feel( expression, db, DB_INSTANTIATED );

	return 0;
}
static int
test_release( char **expression )
{
	char *s = *expression;
	if ( s[ 0 ]=='~' && s[1]=='(' ) {
		*expression += 1;
		return 1;
	}
	return 0;
}

//===========================================================================
//	do_action
//===========================================================================
static int
do_action( char *expression, CNDB *db )
{
	// fprintf( stderr, "do_action: do %s\n", expression );
	if ( !strcmp( expression, "exit" ) )
		return 0;
	else if ( test_release( &expression ) )
		bm_release( expression, db );
	else
		bm_substantiate( expression, db );

	return 1;
}

//===========================================================================
//	do_input
//===========================================================================
static int
do_input( char *expression, CNDB *db )
/*
	Assuming expression is in the form
		variable : format <
	. reads input from stdin according to format
	. instantiates ((*,variable),input)

	Note: this implementation only processes identifiers in the form
	[A-Z-a-z0-9_]+ treating all other characters as input separators
*/
{
	char *format = NULL, *p;

	// generate ((*,variable), from expression
	CNString *s = newString();
	StringAppend( s, '(' );
	StringAppend( s, '(' );
	StringAppend( s, '*' );
	StringAppend( s, ',' );
	for ( p=expression; *p && ((*p!=':')||p_filtered(p+1)); p++ ) {
		StringAppend( s, *p );
	}
	StringAppend( s, ')' );
	StringAppend( s, ',' );

	format = p+1;	// not used in this version

	// complete ((*,variable),input) with input, where
	// input is read (blocking) according to format
	// Here format is assumed to be default
	int event, newline = 1;
	do {
		switch (( event = getchar() )) {
		case '#':
			if ( newline ) do { event = getchar(); }
			while (( event != EOF ) && ( event != '\n' ));
			break;
		case '\n':
			newline = 1;
			break;
		default:
			newline = 0;
		}
	}
	while (( event != EOF ) && is_separator( event ));
	if ( event == EOF ) {
		// release (*,variable)
		StringAppend( s, ')' );
		expression = StringFinish( s, 0 );
		bm_release( &expression[ 1 ], db );
	}
	else {
		// read & instantiate ((*,expression),input)
		do {
			StringAppend( s, event );
			event = getchar();
		}
		while ( !is_separator( event ) );
		StringAppend( s, ')' );
		expression = StringFinish( s, 0 );
		bm_substantiate( expression, db );
	}
	freeString( s );
	return 0;
}

//===========================================================================
//	do_output
//===========================================================================
static char * skip_format( char *format );

static int
do_output( char *expression, CNDB *db )
/*
	Assuming expression is in the form
		> format : expression
	then outputs expression to stdout according to format
*/
{
	// skip the leading '>'
	char *format = expression + 1;
	switch ( *format ) {
	case ':':
		// no format: output expression results
		expression = format + 1;
		if ( *expression ) {
			bm_output( expression, db );
		}
		else printf( "\n" );
		break;
	default:
		expression = skip_format( format );
		switch ( *expression ) {
		case ':':
			expression++;
		}
		bm_outputf( format, expression, db );
	}
	return 0;
}
static char *
skip_format( char *format )
{
	int escaped = 0;
	for ( char *p=format; *p; p++ ) {
		switch ( *p ) {
		case '\\':
			switch ( escaped ) {
			case 0:
			case 2:
				escaped++;
				break;
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		case '\"':
			switch ( escaped ) {
			case 0 :
				escaped = 2;
				break;
			case 2:
				return p+1;
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		default:
			switch ( escaped ) {
			case 1:
			case 3:
				escaped--;
				break;
			}
			break;
		}
	}
	return format;
}
