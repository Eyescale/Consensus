#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_io.h"

//===========================================================================
//	io_init / io_exit
//===========================================================================
static int io_push( CNIO *, CNString * );
static void io_pop( CNIO * );

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
	freeString( io->string );
	while (( io->stack ))
		io_pop( io );
}

//===========================================================================
//	io_getc
//===========================================================================
static int preprocess( CNIO *io, int event );

int
io_getc( CNIO *io, int last_event )
/*
	filters out comments and \cr line continuation from input
*/
{
	int event;
	if ( io->buffer ) {
		if ( last_event=='\n' ) {
			io->column = 0; io->line++; }
		event = io->buffer; io->buffer = 0;
		if ( event!=EOF && event!='\n' ) {
			io->column++; }
		return event; }
	else for ( ; ; ) {
		// Note: the test below makes provision for
		// io->buffer holding more than one event
		if ( last_event=='\n' && !io->buffer ) {
			io->column = 0; io->line++; }
		switch ( io->type ) {
		case IOStreamFile:
		case IOStreamStdin:
			event = fgetc( io->stream );
			break; }
		last_event = event;
		event = preprocess( io, last_event );
		// Assumption: buffering starts, if it does,
		// upon the next event
		if ( last_event!=EOF && last_event!='\n' ) {
			if ( !io->buffer ) io->column++; }
		if (( event )) {
			return event; } }
}

//---------------------------------------------------------------------------
//	preprocess
//---------------------------------------------------------------------------
// #define DEBUG
#include "parser_macros.h"

static int
preprocess( CNIO *io, int event )
{
	CNString *s = io->string;
	char *	state = io->state;
	int	line = io->line,
		column = io->column,
		output = 0,
		errnum = 0;

	BMParseBegin( "preprocess", state, event, line, column );
	on_( EOF ) if (( io->stack )) {
			do_( "" )	io_pop( io ); }
		else {	do_( "" )	output = event; }
	in_( "" ) bgn_
		on_( '#' )	do_( column ? "//" : ( io->type==IOStreamFile ? "#" : "//" ) )
		on_( '\\' )	do_( "\\" )
		on_( '"' )	do_( "\"" )	output = event;
		on_( '\'' )	do_( "'" )	output = event;
		on_( '/' )	do_( "/" )
		on_other	do_( same )	output = event;
		end
	in_( "\\" ) bgn_
		on_( '\n' )	do_( "" )
		on_other	do_( "" )	output = '\\';
						io->buffer = event;
		end
	in_( "\"" ) bgn_
		on_( '"' )	do_( "" )	output = event;
		on_( '\\' )	do_( "\"\\" )	output = event;
		on_other	do_( same )	output = event;
		end
		in_( "\"\\" )	bgn_
			on_any	do_( "\"" )	output = event;
			end
	in_( "'" ) bgn_
		on_( '\'' )	do_( "" )	output = event;
		on_( '\\' )	do_( "'\\" )	output = event;
		on_other	do_( "'_" )	output = event;
		end
		in_( "'_" ) bgn_
			on_any	do_( "" )	output = event;
			end
		in_( "'\\" ) bgn_
			on_( 'x' )	do_( "'\\x" )	output = event;
			on_other	do_( "'_" )	output = event;
			end
			in_( "'\\x" ) bgn_
				on_any		do_( "'\\x_" )	output = event;
				end
			in_( "'\\x_" ) bgn_
				on_any		do_( "'_" )	output = event;
				end
	in_( "/" ) bgn_
		on_( '*' )	do_( "/*" )
		on_( '/' )	do_( "//" )
		on_( '\\' )	do_( "\\" )	output = '/';
						io->buffer = event;
		on_other	do_( "" )	output = '/';
						io->buffer = event;
		end
		in_( "/*" ) bgn_
			on_( '*' )	do_( "/**" )
			on_other	do_( same )
			end
			in_( "/**" ) bgn_
				on_( '/' )	do_( "" )
				on_other	do_( "/*" )
				end
		in_( "//" ) bgn_
			on_( '\n' )	do_( "" )	output = event;
			on_other	do_( same )
			end
	in_( "#" ) bgn_
		on_( '<' )	do_( "#<" )
		on_other	do_( "//" )
		end
		in_( "#<" ) bgn_
			ons( " \t" )	do_( same )
			on_( '\\' )	do_( "#<\\" )
			on_( '>' )	do_( "//" )	errnum = IOErrIncludeFormat;
			on_( '\n' )	do_( "//" )	errnum = IOErrIncludeFormat;
			on_other	do_( "#<_" )	s_take
			end
			in_( "#<\\" ) bgn_
				on_any	do_( "#<_" )	s_add( "\\" )
							s_take
				end
		in_( "#<_" ) bgn_
			ons( " \t\n" )	do_( "#<_ " )	REENTER
			on_( '\\' )	do_( "#<\\" )
			on_( '>' )	do_( "#<>" )
			on_other	do_( same )	s_take
			end
			in_( "#<_ " ) bgn_
				ons( " \t" )	do_( same )
				on_( '>' )	do_( "#<>" )
				on_other	do_( "//" )	errnum = IOErrIncludeFormat;
								s_reset( CNStringAll )
				end
		in_( "#<>" ) bgn_
			ons( " \t" )	do_( same )
			on_( '/' )	do_( "#<>/" )
			on_( '\n' )	do_( "#<>_" )	REENTER
			on_other	do_( "#<>_" )	errnum = IOErrTrailingChar;
			end
			in_( "#<>/" ) bgn_
				on_( '/' )	do_( "#<>_" )
				on_( '\n' )	do_( "#<>_" )	REENTER
								errnum = IOErrTrailingChar;
				on_other	do_( "#<>_" )	errnum = IOErrTrailingChar;
				end
		in_( "#<>_" ) bgn_
			on_( '\n' )	do_( "" )	errnum = io_push( io, s );
			on_other	do_( same )
			end
	BMParseEnd

	if ( errnum ) {
		fprintf( stderr, "Warning: bm_parse: " );
		if (( io->path )) fprintf( stderr, "in %s, ", io->path );
		column = ( event=='\n' ) ? io->column : io->column+1;
		fprintf( stderr, "l%dc%d: ", io->line, column );
		switch ( errnum ) {
		case IOErrFileNotFound:
			fprintf( stderr, "could not open file: \"%s\"\n", StringFinish(s,0) );
			StringReset( s, CNStringAll );
			break;
		case IOErrIncludeFormat:
			fprintf( stderr, "unknown include format\n" );
			break;
		case IOErrTrailingChar:
			fprintf( stderr, "trailing characters\n" );
			break;
		default:
			fprintf( stderr, "syntax error\n" );
			break; } }

	io->state = state;
	return output;
}

static int
io_push( CNIO *io, CNString *s )
{
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
}

static void
io_pop( CNIO *io )
/*
	Assumption: io->stack != NULL
*/
{
	listItem **stack = &io->stack;
	union { int value; char *ptr; } icast;

	fclose( io->stream );
	free( io->path );

	Pair *pair = popListItem( stack );
	icast.ptr = pair->name;
	io->type = icast.value;
	icast.ptr = pair->value;
	io->line = icast.value;
	io->column = 0;

	pair = popListItem( stack );
	io->stream = pair->name;
	io->path = pair->value;
}

