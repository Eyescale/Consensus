#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_io.h"

static int io_push( CNIO *, IOType );
static int io_pop( CNIO * );

//===========================================================================
//	io_init / io_exit
//===========================================================================
void
io_init( CNIO *io, void *stream, IOType type )
{
	memset( io, 0, sizeof(CNIO) );
	io->string = newString();
	io->stream = stream;
	io->type = type;
	io->state = "";
	io->line = 1;
}

void
io_exit( CNIO *io )
{
	freeListItem( &io->buffer );
	freeString( io->string );
	while (( io->stack )) io_pop( io );
}

//===========================================================================
//	io_getc
//===========================================================================
static void io_report( CNIO *, int errnum, int l, int c );
static IOEventAction preprocess( CNIO *io, int event );

int
io_getc( CNIO *io, int last_event )
/*
	filters out comments and \cr line continuation from input
*/
{
	int event;
	do {
		if ( last_event=='\n' ) {
			last_event = 0;
			io->line++; io->column = 0; }
		do {
			switch ( io->type ) {
			case IOStreamFile:
			case IOStreamStdin:
				event = fgetc( io->stream );
				break;
			case IOBuffer:
				event = ( io->stream ) ?
					pop_item((listItem **) &io->stream ) :
					io_pop( io );
				// Assumption: EOF can only be last in buffer
				if ( event==EOF && ( io->stack ))
					io_pop( io );
				break; }
			if ( event==EOF && ( io->stack ))
				event = io_pop( io );
		} while ( !event );

		int action = preprocess( io, event );

		listItem **buffer = &io->buffer;
		switch ( action ) {
		case IOEventTake:
			break;
		case IOEventPass:
			if (( *buffer )) {
				reorderListItem( buffer );
				while (( last_event=pop_item( buffer ) )) {
					if ( last_event=='\n' ) {
						io->line++; io->column=0; }
					else io->column++; } }
			if ( event!='\n' ) { io->column++; }
			else { last_event = event; }
			event = 0; break;
		case IOEventPush:
			add_item( buffer, event );
			event = 0; break;
		case IOEventPop:
			add_item( buffer, event );
			reorderListItem( buffer );
			io_push( io, IOBuffer );
			event = 0; break; }
	} while ( !event );

	if ( event!=EOF && event!='\n' )
		io->column++;

	if (( io->buffer )) io_push( io, IOBuffer );
	return event;
}

//---------------------------------------------------------------------------
//	preprocess
//---------------------------------------------------------------------------
// #define DEBUG
#include "parser_macros.h"

static IOEventAction
preprocess( CNIO *io, int event )
{
	CNString *s = io->string;
	char *	state = io->state;
	int	line = io->line,
		column = io->column,
		action = 0,
		errnum = 0;

	BMParseBegin( "preprocess", state, event, line, column )
	on_( EOF ) bgn_
		in_( "\"\"" )	do_( "pop" ) 	action = IOEventPop;
		in_other	do_( "" )	action = IOEventTake;
		end
		in_( "pop" ) bgn_
			on_any	do_( "" )	action = IOEventTake;
			end
	in_( "" ) bgn_
		on_( '#' )	do_( column ? "//" : ( io->type==IOStreamFile ? "#" : "//" ) )
						action = IOEventPass;
		on_( '\\' )	do_( "\\" )	action = IOEventPush;
		on_( '"' )	do_( "\"" )	action = IOEventTake;
		on_( '\'' )	do_( "'" )	action = IOEventTake;
		on_( '/' )	do_( "/" )	action = IOEventPush;
		on_other	do_( same )	action = IOEventTake;
		end
	in_( "\\" ) bgn_
		on_( '\n' )	do_( "\\_" )	action = IOEventPass;
		on_other	do_( "pop" )	action = IOEventPop;
		end
		in_( "\\_" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '\\' )	do_( "\\" )	action = IOEventPush;
			on_other	do_( "" )	action = IOEventTake;
			end
	in_( "\"" ) bgn_
		on_( '"' )	do_( "\"\"" )	action = IOEventPush;
		on_( '\\' )	do_( "\"\\" )	action = IOEventTake;
		on_other	do_( same )	action = IOEventTake;
		end
		in_( "\"\\" )	bgn_
			on_any	do_( "\"" )	action = IOEventTake;
			end
		in_( "\"\"" ) bgn_
			ons( " \t\n" )		do_( same )	action = IOEventPush;
			on_( '\\' )		do_( "\"\"\\" )	action = IOEventPush;
			on_( '"' )		do_( "\"" )	action = IOEventPass;
			on_other		do_( "pop" )	action = IOEventPop;
			end
		in_( "\"\"\\" ) bgn_
			on_( '\n' )	do_( "\"\"" )	action = IOEventPush;
			on_other	do_( "pop" )	action = IOEventPop;
			end
	in_( "'" ) bgn_
		on_( '\'' )	do_( "" )	action = IOEventTake;
		on_( '\\' )	do_( "'\\" )	action = IOEventTake;
		on_other	do_( "'_" )	action = IOEventTake;
		end
		in_( "'_" ) bgn_
			on_any	do_( "" )	action = IOEventTake;
			end
		in_( "'\\" ) bgn_
			on_( 'x' )	do_( "'\\x" )	action = IOEventTake;
			on_other	do_( "'_" )	action = IOEventTake;
			end
			in_( "'\\x" ) bgn_
				on_any		do_( "'\\x_" )	action = IOEventTake;
				end
			in_( "'\\x_" ) bgn_
				on_any		do_( "'_" )	action = IOEventTake;
				end
	in_( "/" ) bgn_
		on_( '*' )	do_( "/*" )	action = IOEventPass;
		on_( '/' )	do_( "//" )	action = IOEventPass;
		on_other	do_( "pop" )	action = IOEventPop;
		end
		in_( "/*" ) bgn_
			on_( '*' )	do_( "/**" )	action = IOEventPass;
			on_other	do_( same )	action = IOEventPass;
			end
			in_( "/**" ) bgn_
				on_( '/' )	do_( "" )	action = IOEventPass;
				on_other	do_( "/*" )	action = IOEventPass;
				end
		in_( "//" ) bgn_
			on_( '\n' )	do_( "" )	action = IOEventTake;
			on_other	do_( same )	action = IOEventPass;
			end
	in_( "#" ) bgn_
		ons( " \t" ) if ( !s_cmp( "include" ) ) {
				do_( "#$" )	s_reset( CNStringAll )
						action = IOEventPass; }
			else {	do_( "//" )	errnum = s_empty ? 0 : IOErrPragmaUnknown;
						action = IOEventPass; }
		on_separator	do_( "//" )	REENTER
						errnum = s_empty ? 0 : IOErrSyntaxError;
		on_other	do_( same )	action = IOEventPass;
						s_take
		end

	in_( "#$" ) bgn_
		ons( " \t" )	do_( same )	action = IOEventPass;
		on_( '<' )	do_( "#<" )	action = IOEventPass;
		on_other	do_( "//" )	REENTER
						errnum = IOErrIncludeFormat;
		end

	in_( "#<" ) bgn_
		ons( " \t" )	do_( same )	action = IOEventPass;
		on_( '\\' )	do_( "#<\\" )	action = IOEventPass;
		ons( ">\n" )	do_( "//" )	REENTER
						errnum = IOErrIncludeFormat;
		on_other	do_( "#<_" )	action = IOEventPass;
						s_take
		end
		in_( "#<\\" ) bgn_
			on_( '\n' )	do_( "//" )	REENTER
							errnum = IOErrIncludeFormat;
			on_other	do_( "#<_" )	action = IOEventPass;
							s_add( "\\" )
							s_take
			end
		in_( "#<_" ) bgn_
			ons( " \t\n" )	do_( "#<_ " )	REENTER
			on_( '\\' )	do_( "#<\\" )	action = IOEventPass;
			on_( '>' )	do_( "#<>" )	action = IOEventPass;
			on_other	do_( same )	action = IOEventPass;
							s_take
			end
			in_( "#<_ " ) bgn_
				ons( " \t" )	do_( same )	action = IOEventPass;
				on_( '>' )	do_( "#<>" )	action = IOEventPass;
				on_other	do_( "//" )	REENTER
								errnum = IOErrIncludeFormat;
				end
		in_( "#<>" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '/' )	do_( "#<>/" )	action = IOEventPass;
			on_( '\n' )	do_( "#<>_" )	REENTER
			on_other	do_( "#<>_" )	action = IOEventPass;
							errnum = IOErrTrailingChar;
			end
			in_( "#<>/" ) bgn_
				on_( '/' )	do_( "#<>_" )	action = IOEventPass;
				on_other	do_( "#<>_" )	REENTER
								errnum = IOErrTrailingChar;
				end
		in_( "#<>_" ) bgn_
			on_( '\n' )	do_( "" )	action = IOEventTake;
							errnum = io_push( io, IOStreamFile );
			on_other	do_( same )	action = IOEventPass;
			end
	BMParseEnd

	if ( errnum ) {
		if ( event!='\n' ) column += 1;
		io_report( io, errnum, line, column ); }

	io->state = state;
	return action;
}

//---------------------------------------------------------------------------
//	io_push / io_pop
//---------------------------------------------------------------------------
static int
io_push( CNIO *io, IOType type )
{
	switch ( type ) {
	case IOBuffer: ;
		listItem **stack = &io->stack;
		addItem( stack, newPair( io->stream, io->path ) );
		addItem( stack, new_pair( io->type, io->line ) );
		io->type = IOBuffer;
		io->stream = io->buffer;
		io->path = NULL;
		io->buffer = NULL;
		return 0;
	case IOStreamFile: ;
		CNString *s = io->string;
		char *path = StringFinish( s, 0 );
		FILE *stream = fopen( path, "r" );
		if ( !stream ) {
			return IOErrFileNotFound; }
		else {
			io->line++;
			io->column = 0;
			StringReset( s, CNStringMode );
			listItem **stack = &io->stack;
			addItem( stack, newPair( io->stream, io->path ) );
			addItem( stack, new_pair( io->type, io->line ) );
			io->type = IOStreamFile;
			io->stream = stream;
			io->path = path;
			io->line = 0;	// re. upcoming '\n'
			io->column = 0;
			return 0; }
	default:
		return 0; }
}

static int
io_pop( CNIO *io )
/*
	Assumption: io->stack != NULL
*/
{
	listItem **stack = &io->stack;
	union { int value; char *ptr; } icast;

	int previous = io->type;
	switch ( previous ) {
	case IOBuffer:
		freeListItem((listItem **) &io->stream );
		break;
	case IOStreamFile:
		fclose( io->stream );
		free( io->path );
		break;
	default: break; }

	Pair *pair = popListItem( stack );
	icast.ptr = pair->name;
	io->type = icast.value;
	if ( previous==IOStreamFile ) {
		icast.ptr = pair->value;
		io->line = icast.value;
		io->column = 0; }

	pair = popListItem( stack );
	io->stream = pair->name;
	io->path = pair->value;
	return 0;
}

//---------------------------------------------------------------------------
//	io_report
//---------------------------------------------------------------------------
#define err_case( err, msg ) \
	case err: fprintf( stderr, msg
#define _( arg ) \
	, arg ); break;
#define err_default( msg ) \
	default: fprintf( stderr, msg
#define _narg \
	); break;
void
io_report( CNIO *io, int errnum, int l, int c )
{
	CNString *s = io->string;
	char *stump = StringFinish( s, 0 );
	fprintf( stderr, "Warning: bm_parse: " );
	if (( io->path )) fprintf( stderr, "in %s, ", io->path );
	fprintf( stderr, "l%dc%d: ", l, c );

	switch ( errnum ) {
	err_case( IOErrFileNotFound, "could not open file: \"%s\"\n" )_( stump )
	err_case( IOErrIncludeFormat, "unknown include format\n" )_narg
	err_case( IOErrTrailingChar, "trailing characters\n" )_narg
	err_case( IOErrPragmaUnknown, "unknown pragma directive #%s\n" )_( stump );
	err_default( "syntax error\n" )_narg }

	StringReset( s, CNStringAll );
}
