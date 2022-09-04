#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "db_io.h"

// #define DEBUG

//===========================================================================
//	db_input
//===========================================================================
char *
db_input( FILE *stream, int authorized )
/*
	authorized is set upon cnLoad, allowing (:_) in expressions
*/
{
	if ( stream == NULL ) return NULL;

	struct { listItem *first, *level; } stack = { NULL, NULL };
	int	first=1, informed=0, level=0, special = 0;

	CNString *s = newString();

	DBInputBegin( stream, fgetc )
	on_( EOF )
		freeString( s );
		return NULL;
	in_( "base" ) bgn_
		on_( '#' )	do_( "#" )
		ons( " \t\n" )	do_( same )
		on_( '(' )	do_( "expr" )	REENTER
		on_( '\'' )	do_( "char" )	StringAppend( s, event );
		on_separator	do_( same )	fprintf( stderr, "B%%::db_input: Warning: skipping "
							"extraneous character: '%c'\n", event );
		on_other	do_( "term" )	StringAppend( s, event );
		end
	in_( "#" ) bgn_
		on_( '\n' )	do_( "base" )
		on_other	do_( same )
		end
	in_( "expr" ) bgn_
		on_( '#' )	do_( "_#" );
		ons( " \t" )	do_( same )
		on_( '(' )
			if ( !informed ) {
				do_( "(" )	StringAppend( s, event );
						level++;
						add_item( &stack.first, first );
						first = 1; informed = 0;
			}
		on_( ')' )
			if ( informed && level ) {
				do_( "pop" )	REENTER
						level--;
						first = pop_item( &stack.first );
						StringAppend( s, event );
			}
		on_( '{' )
			if ( !informed ) {
				do_( same )	add_item( &stack.level, level );
						add_item( &stack.first, first );
						StringAppend( s, event );
						first = 1; level = 0;
						special = 1;
			}
		on_( '}' )
			if ( informed && !level && (stack.level) ) {
				do_( "pop" )	REENTER
						level = pop_item( &stack.level );
						first = pop_item( &stack.first );
						StringAppend( s, event );
			}
		on_( ',' )
			if ( informed && level && first ) {
				do_( same )	StringAppend( s, event );
						first = informed = 0;
			}
			else if ( informed && !level && (stack.level) ) {
				do_( same )	StringAppend( s, event );
						informed = 0;
			}
		on_( '\n' )
			if (( stack.level )) {
				do_( same )
			}
		on_( '\'' )
			if ( !informed ) {
				do_( "char" )	StringAppend( s, event );
			}
		ons( "*%" )
			if ( !informed ) {
				do_( same )	StringAppend( s, event );
						informed = 1;
			}
		on_separator	; // fail
		on_other
			if ( !informed ) {
				do_( "term" )	StringAppend( s, event );
			}
		end
	in_( "_#" ) bgn_
		on_( '\n' )	do_( "expr" )
		on_other	do_( same )
		end
	in_( "(" ) bgn_
		on_( ':' )	do_( "(:" )	StringAppend( s, event );
						special = 1;
		on_other	do_( "expr" )	REENTER
		end
	in_( "(:" ) bgn_
		on_( ')' )	do_( "expr" )	REENTER
						informed = 1;
		on_( '\\' )	do_( "(:\\" )	StringAppend( s, event );
		on_( '%' )	do_( "(:%" )	StringAppend( s, event );
		on_other	do_( same )	StringAppend( s, event );
		end
		in_( "(:\\" ) bgn_
			on_( 'x' )	do_( "(:\\x" )	StringAppend( s, event );
			on_other	do_( "(:" )	StringAppend( s, event );
			end
		in_( "(:\\x" ) bgn_
			on_xdigit	do_( "(:\\x_" ) StringAppend( s, event );
			end
		in_( "(:\\x_" ) bgn_
			on_xdigit	do_( "(:" )	StringAppend( s, event );
			end
		in_( "(:%" ) bgn_
			on_( '%' )	do_( "(:" )	StringAppend( s, event );
			on_( ' ' )	do_( "(:" )	StringAppend( s, event );
			on_separator	; // fail
			on_other	do_( "(:" )	StringAppend( s, event );
			end
	in_( "pop" )
		if ((stack.level) || level ) {
			do_( "expr" )
		}
		else if ( authorized || !special ) {
			do_( "" )
		}
		else {	do_( "base" )	StringReset( s, CNStringAll );
					freeListItem( &stack.first );
					first = 1; informed=special=level=0;
		}
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
			on_( '\'' )
				if ( level || (stack.level)) {
					do_( "expr" )	StringAppend( s, event );
							informed = 1;
				}
				else {	do_( "" ) }
			end
	in_( "term" ) bgn_
		on_separator
			if ( level || (stack.level)) {
				do_( "expr" )	REENTER
						informed = 1;
			}
			else {	do_( "" )	ungetc( event, stream ); }
		on_other	do_( same )	StringAppend( s, event );
		end

	DBInputDefault
		in_none_sofar	do_( "" )	fprintf( stderr, ">>>>>> B%%::db_input: Error: "
							"unknown state \"%s\" <<<<<<\n", state );
						StringReset( s, CNStringAll );
		in_other	do_( "#" )	char *p = StringFinish( s, 0 );
						fprintf( stderr, "B%%::db_input: Warning: "
							"expression incomplete: \"%s\"\n", p );
						StringReset( s, CNStringAll );
						freeListItem( &stack.first );
						freeListItem( &stack.level );
						first = 1; informed=special=level=0;
						REENTER
	DBInputEnd

	freeListItem( &stack.first );
	freeListItem( &stack.level );
	char *p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
#ifdef DEBUG
	fprintf( stderr, "DB_INPUT: returning %s\n", p );
#endif
	return p;
}

//===========================================================================
//	db_output
//===========================================================================
int
db_output( FILE *stream, char *format, CNInstance *e, CNDB *db )
{
	if ( e == NULL ) return 0;
	if ( *format == 's' ) {
		if (( e->sub[ 0 ] ))
			fprintf( stream, "\\" );
		else {
			char *p = db_identifier( e, db );
			if (( p )) fprintf( stream, "%s", p );
			return 0;
		}
	}
	listItem *stack = NULL;
	int ndx = 0;
	for ( ; ; ) {
		if (( e->sub[ ndx ] )) {
			fprintf( stream, ndx==0 ? "(" : "," );
			add_item( &stack, ndx );
			addItem( &stack, e );
			e = e->sub[ ndx ];
			ndx = 0;
			continue;
		}
		char *p = db_identifier( e, db );
		if (( p )) {
			if (( *p=='*' ) || ( *p=='%' ) || !is_separator(*p))
				fprintf( stream, "%s", p );
			else {
				switch (*p) {
				case '\0': fprintf( stream, "'\\0'" ); break;
				case '\t': fprintf( stream, "'\\t'" ); break;
				case '\n': fprintf( stream, "'\\n'" ); break;
				case '\'': fprintf( stream, "'\\''" ); break;
				case '\\': fprintf( stream, "'\\\\'" ); break;
				default:
					if ( is_printable(*p) ) fprintf( stream, "'%c'", *p );
					else fprintf( stream, "'\\x%.2X'", *(unsigned char *)p );
				}
			}
		}
		for ( ; ; ) {
			if (( stack )) {
				e = popListItem( &stack );
				ndx = pop_item( &stack );
				if ( ndx==0 ) { ndx = 1; break; }
				else fprintf( stream, ")" );
			}
			else goto RETURN;
		}
	}
RETURN:
	return 0;
}
