#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_io.h"
#include "parser_macros.h"

//===========================================================================
//	newIO / freeIO
//===========================================================================
void
io_init( CNIO *io, void *stream, IOStreamType type )
{
	memset( io, 0, sizeof(CNIO) );
	io->stream = stream;
	io->type = type;
	io->state = "";
	io->line = 1;
}

void
io_exit( CNIO *io )
{
}

//===========================================================================
//	io_getc
//===========================================================================
static int preprocess( CNIO *io, int event, int *buffer, int *skipped );

int
io_getc( CNIO *io, int last_event )
/*
	filters out comments and \cr line continuation from input
*/
{
	int buffer = io->buffer;
	if ( last_event=='\n' ) {
		io->column = 0;
		io->line++; }
	int event, skipped[ 2 ] = { 0, 0 };
	do {
		if ( buffer ) {
			event=buffer; buffer=0; }
		else {
			switch ( io->type ) {
			case IOStreamFile:
				event = fgetc( io->stream );
				break; }
			event = preprocess( io, event, &buffer, skipped ); }
	} while ( !event );
	io->buffer = buffer;
	if ( skipped[ 1 ] ) {
		io->line += skipped[ 1 ];
		io->column = skipped[ 0 ]; }
	else {
		io->column += skipped[ 0 ]; }
	if (!( event==EOF || event=='\n' )) {
		io->column++; }
	return event;
}

//---------------------------------------------------------------------------
//	preprocess
//---------------------------------------------------------------------------
// #define DEBUG

static int
preprocess( CNIO *io, int event, int *buffer, int *skipped )
{
	char *state = io->state;
	int output = 0, caught;
#ifdef DEBUG
	fprintf( stderr, "preprocess:l%dc%d: in \"%s\" on ", io->line, io->column, state );
	switch ( event ) {
	case EOF: fprintf( stderr, "EOF" ); break;
	case '\n': fprintf( stderr, "'\\n'" ); break;
	case '\t': fprintf( stderr, "'\\t'" ); break;
	default: fprintf( stderr, "'%c'", event );
	}
	fprintf( stderr, "\n" );
#endif
	bgn_
	on_( EOF )	output = event;
	in_( "" ) bgn_
		on_( '\\' )	do_( "\\" )
		on_( '"' )	do_( "\"" )	output = event;
		on_( '\'' )	do_( "'" )	output = event;
		on_( '/' )	do_( "/" )
		on_( '#' )	do_( "#" )	skipped[ 0 ] = 1;
		on_other	do_( same )	output = event;
		end
	in_( "\\" ) bgn_
		on_( '\n' )	do_( "" )	skipped[ 1 ] = 1;
		on_other	do_( "" )	output = '\\';
						*buffer = event;
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
		on_( '*' )	do_( "/*" )	skipped[ 0 ] = 2;
		on_( '/' )	do_( "//" )	skipped[ 0 ] = 2;
		on_( '\\' )	do_( "\\" )	output = '/';
						*buffer = event;
		on_other	do_( "" )	output = '/';
						*buffer = event;
		end
		in_( "/*" ) bgn_
			on_( '*' )	do_( "/**" )	skipped[ 0 ]++;
			on_( '\n' )	do_( same )	skipped[ 1 ]++;
							skipped[ 0 ] = 0;
			on_other	do_( same )	skipped[ 0 ]++;
			end
			in_( "/**" ) bgn_
				on_( '/' )	do_( "" )	skipped[ 0 ]++;
				on_( '\n' )	do_( "/*" )	skipped[ 1 ]++;
								skipped[ 0 ] = 0;
				on_other	do_( "/*" )	skipped[ 0 ]++;
				end
		in_( "//" ) bgn_
			on_( '\n' )	do_( "" )	output = event;
			on_other	do_( same )	skipped[ 0 ]++;
			end
	in_( "#" ) bgn_
		on_( '\n' )	do_( "" )	output = event;
		on_other	do_( same )	skipped[ 0 ]++;
		end
	end
	io->state = state;
	return output;
}

