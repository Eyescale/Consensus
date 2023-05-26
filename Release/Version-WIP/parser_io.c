#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser_io.h"

//===========================================================================
//	io_init / io_exit
//===========================================================================
static int io_pop( CNIO * );

void
io_init( CNIO *io, void *stream, IOType type )
{
	memset( io, 0, sizeof(CNIO) );
	io->control.registry = newRegistry( IndexedByName );
	io->string = newString();
	io->stream = stream;
	io->type = type;
	io->state = "";
	io->line = 1;
}

void
io_exit( CNIO *io )
{
	freeRegistry( io->control.registry, NULL );
	freeListItem( &io->control.stack );
	free( io->control.param );
	freeListItem( &io->buffer );
	freeString( io->string );
	while (( io->stack )) io_pop( io );
}

//===========================================================================
//	io_getc
//===========================================================================
static int io_push( CNIO *, IOType, char * );
static IOEventAction preprocess( CNIO *, int event );
static int io_pragma_execute( CNIO * );

int
io_getc( CNIO *io, int last_event )
/*
	filters out comments and \cr line continuation from input
*/
{
	listItem **buffer;
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
				event = pop_item((listItem **) &io->stream );
				// Assumption: EOF can only be last in IOBuffer
				if ( !event || event==EOF ) // pop IOBuffer
					io_pop( io ); // while passing event
				break; }
		} while ( !event );

		int action = preprocess( io, event );
		if ( io->errnum ) goto RETURN;

		switch ( action ) {
		case IOEventPragma:
			if ( io_pragma_execute( io ) ) { 
				last_event = event; // presumably '\n'
				event = 0; }
			else goto RETURN;
			break;
		case IOEventTake:
			if ( event==EOF )
				event = io_pop( io );
			else if ( io->control.mode & IO_SILENT ) {
				last_event = event;
				event = 0; }
			break;
		case IOEventPass:
			// update io->line and io->column wrt io->buffer
			if (( io->buffer )) {
				buffer = &io->buffer;
				reorderListItem( buffer );
				while (( last_event=pop_item( buffer ) )) {
					if ( last_event=='\n' ) {
						io->line++; io->column=0; }
					else io->column++; } }
			// update io->line and io->column wrt event
			if ( event=='\n' ) last_event = event;
			else io->column++;
			event = 0; break;
		case IOEventQueue:
			add_item( &io->buffer, event );
			event = 0; break;
		case IOEventFlush:
			buffer = &io->buffer;
			add_item( buffer, event );
			reorderListItem( buffer );
			io_push( io, IOBuffer, NULL );
			event = 0; break; }
	} while ( !event );

	if ( event!='\n' && event!=EOF )
		io->column++;
	if (( io->buffer )) io_push( io, IOBuffer, NULL );
RETURN:
	return event;
}

static int
io_pragma_execute( CNIO *io )
{
	union { void *ptr; int value; } parent_mode;
	int pragma = io->control.pragma;
	char *param = io->control.param;
	int mode = io->control.mode;
	int errnum = 0;
	if ( mode & IO_SILENT ) {
		switch ( pragma ) {
		case IOIncludeFile:
		case IODefine:
		case IOUndef:
			free( param );
			break;
		case IOIfdef:
		case IOIfndef:
			free( param );
			add_item( &io->control.stack, mode );
			io->control.mode = IO_SILENT;
			break;
		case IOElse:
			switch ( mode ) {
			case IO_SILENT_ELSE:
				errnum = IOErrElseAgain;
				break;
			default: // IO_SILENT
				parent_mode.ptr = io->control.stack->ptr;
				switch ( parent_mode.value ) {
				case 0:
				case IO_ACTIVE:
				case IO_ACTIVE_ELSE:
					io->control.mode = IO_ACTIVE_ELSE;
					break;
				default:
					io->control.mode = IO_SILENT_ELSE; } }
			break;
		case IOEndif:
			io->control.mode = pop_item( &io->control.stack );
			break; } }
	else {
		Pair *entry;
		switch ( pragma ) {
		case IOIncludeFile:
			errnum = io_push( io, IOStreamFile, param );
			if ( errnum ) free( param );
			break;
		case IODefine:
			entry = registryRegister( io->control.registry, param, NULL );
			if ( !entry ) {
				errnum = IOErrAlreadyDefined;
				free( param ); }
			break;
		case IOUndef:
			registryDeregister( io->control.registry, param );
			free( param );
			break;
		case IOIfdef:
			entry = registryLookup( io->control.registry, param );
			add_item( &io->control.stack, mode );
			io->control.mode = ((entry) ? IO_ACTIVE : IO_SILENT );
			free( param );
			break;
		case IOIfndef:
			entry = registryLookup( io->control.registry, param );
			add_item( &io->control.stack, mode );
			io->control.mode = ((entry) ? IO_SILENT : IO_ACTIVE );
			free( param );
			break;
		case IOElse:
			switch ( mode ) {
			case 0: errnum = IOErrElseWithoutIf; break;
			case IO_ACTIVE_ELSE: errnum = IOErrElseAgain; break;
			case IO_ACTIVE: io->control.mode = IO_SILENT_ELSE; break; }
			break;
		case IOEndif:
			switch ( io->control.mode ) {
			case 0: errnum = IOErrEndifWithoutIf; break;
			default: io->control.mode = pop_item( &io->control.stack ); }
			break; } }
	io->control.param = NULL;
	io->control.pragma = 0;
	io->errnum = errnum;
	return errnum;
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
		in_( "/" )	do_( "pop" ) 	action = IOEventFlush;
		in_( "\\" )	do_( "pop" ) 	action = IOEventFlush;
		in_( "\"\"" )	do_( "pop" ) 	action = IOEventFlush;
		in_( "\"\"\\" ) do_( "pop" )	action = IOEventFlush;
		in_other	do_( "" )	action = IOEventTake;
						errnum = s_empty ? 0 : IOErrUnexpectedEOF;
		end
		in_( "pop" ) bgn_
			on_any	do_( "" )	action = IOEventTake;
			end
	in_( "" ) bgn_
		on_( '#' ) if ( !column && io->type==IOStreamFile ) {
				do_( "#" )	action = IOEventPass; }
			else {	do_( "//" )	action = IOEventPass; }
		on_( '/' )	do_( "/" )	action = IOEventQueue;
		on_( '\\' )	do_( "\\" )	action = IOEventQueue;
		on_( '"' )	do_( "\"" )	action = IOEventTake;
		on_( '\'' )	do_( "'" )	action = IOEventTake;
		on_other	do_( same )	action = IOEventTake;
		end
	in_( "\\" ) bgn_
		on_( '\n' )	do_( "\\_" )	action = IOEventPass;
		on_other	do_( "pop" )	action = IOEventFlush;
		end
		in_( "\\_" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '\\' )	do_( "\\" )	action = IOEventQueue;
			on_other	do_( "" )	action = IOEventTake;
			end
	in_( "\"" ) bgn_
		on_( '"' )	do_( "\"\"" )	action = IOEventQueue;
		on_( '\\' )	do_( "\"\\" )	action = IOEventTake;
		on_other	do_( same )	action = IOEventTake;
		end
		in_( "\"\\" )	bgn_
			on_any	do_( "\"" )	action = IOEventTake;
			end
		in_( "\"\"" ) bgn_
			ons( " \t\n" )		do_( same )	action = IOEventQueue;
			on_( '\\' )		do_( "\"\"\\" )	action = IOEventQueue;
			on_( '"' )		do_( "\"" )	action = IOEventPass;
			on_other		do_( "pop" )	action = IOEventFlush;
			end
		in_( "\"\"\\" ) bgn_
			on_( '\n' )	do_( "\"\"" )	action = IOEventQueue;
			on_other	do_( "pop" )	action = IOEventFlush;
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
				on_any	do_( "'\\x_" )	action = IOEventTake;
				end
			in_( "'\\x_" ) bgn_
				on_any	do_( "'_" )	action = IOEventTake;
				end
	in_( "/" ) bgn_
		on_( '*' )	do_( "/*" )	action = IOEventPass;
		on_( '/' )	do_( "//" )	action = IOEventPass;
		on_other	do_( "pop" )	action = IOEventFlush;
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
		ons( " \t" )
			if ( !s_cmp( "include" ) ) {
				do_( "#<" )	io->control.pragma = IOIncludeFile;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "define" ) ) {
				do_( "#$" )	io->control.pragma = IODefine;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "undef" ) ) {
				do_( "#$" )	io->control.pragma = IOUndef;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "ifdef" ) ) {
				do_( "#$" )	io->control.pragma = IOIfdef;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "ifndef" ) ) {
				do_( "#$" )	io->control.pragma = IOIfndef;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "else" ) ) {
				do_( "#_" )	io->control.pragma = IOElse;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else if ( !s_cmp( "endif" ) ) {
				do_( "#_" )	io->control.pragma = IOEndif;
						s_reset( CNStringAll )
						action = IOEventPass; }
			else {	do_( "//" )	s_reset( CNStringAll )
						action = IOEventPass; }
		on_( '\n' ) if ( !s_cmp( "else" ) ) {
				do_( "" )	io->control.pragma = IOElse;
						s_reset( CNStringAll )
						action = IOEventPragma; }
			else if ( !s_cmp( "endif" ) ) {
				do_( "" )	io->control.pragma = IOEndif;
						s_reset( CNStringAll )
						action = IOEventPragma; }
			else {	do_( "//" )	REENTER
						s_reset( CNStringAll ) }
		on_separator {	do_( "//" )	s_reset( CNStringAll )
						action = IOEventPass; }
		on_other	do_( same )	s_take
						action = IOEventPass;
		end
		in_( "#$" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_separator	do_( "" )	errnum = IOErrPragmaIncomplete;
			on_other	do_( "#$_" )	s_take
							action = IOEventPass;
			end
			in_( "#$_" ) bgn_
				on_( '\n' )	do_( "#_//" )	REENTER
				on_separator	do_( "#_" )	action = IOEventPass;
				on_other	do_( same )	s_take
								action = IOEventPass;
			end
		in_( "#<" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '"' )	do_( "#<\"" )	action = IOEventPass;
			on_other	do_( "" )	errnum = IOErrIncludeFormat;
			end
			in_( "#<\"" ) bgn_
				on_( '"' )	do_( "#_" )	action = IOEventPass;
				on_( '\\' )	do_( "#<\"\\" )	action = IOEventPass;
				on_( '\n' )	do_( "" )	errnum = IOErrIncludeFormat;
				on_other	do_( same )	s_take
								action = IOEventPass;
			end
			in_( "#<\"\\" ) bgn_
				on_any		do_( "#<\"" )	s_add( "\\" )
								s_take
								action = IOEventPass;
				end
		in_( "#_" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '/' )	do_( "#_/" )	action = IOEventPass;
			on_( '\n' )	do_( "#_//" )	REENTER
			on_other	do_( "" )	errnum = IOErrTrailingChar;
			end
			in_( "#_/" ) bgn_
				on_( '/' )	do_( "#_//" )	action = IOEventPass;
				on_other	do_( "" )	errnum = IOErrTrailingChar;
				end
			in_( "#_//" ) bgn_
				on_( '\n' )	do_( "" )	action = IOEventPragma;
								io->control.param = StringFinish( s, 0 );
								s_reset( CNStringMode )
				on_other	do_( same )	action = IOEventPass;
				end
	BMParseEnd

	io->errnum = errnum;
	io->state = state;
	return action;
}

//---------------------------------------------------------------------------
//	io_push / io_pop
//---------------------------------------------------------------------------
static int
io_push( CNIO *io, IOType type, char *path )
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
		FILE *stream = fopen( path, "r" );
		if ( !stream ) {
			return IOErrFileNotFound; }
		else {
			io->line++;
			io->column = 0;
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
	if ( !io->stack ) return EOF;
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
	if ( previous!=IOBuffer ) {
		icast.ptr = pair->value;
		io->line = icast.value;
		io->column = 0; }

	pair = popListItem( stack );
	io->stream = pair->name;
	io->path = pair->value;
	return 0;
}

//===========================================================================
//	io_report
//===========================================================================
#define err_case( err, msg ) \
	case err: fprintf( stderr, msg
#define _( arg ) \
	, arg ); break;
#define err_default( msg ) \
	default: fprintf( stderr, msg
#define _narg \
	); break;
IOErr
io_report( CNIO *io )
{
	CNString *s = io->string;
	char *stump = StringFinish( s, 0 );
	fprintf( stderr, "Error: bm_parse: " );
	if (( io->path )) fprintf( stderr, "in %s, ", io->path );
	fprintf( stderr, "l%dc%d: ", io->line, io->column );

	switch ( io->errnum ) {
	err_case( IOErrUnexpectedEOF, "unexpected EOF\n" )_narg
	err_case( IOErrFileNotFound, "could not open file: \"%s\"\n" )_( stump )
	err_case( IOErrIncludeFormat, "unknown include format\n" )_narg
	err_case( IOErrTrailingChar, "trailing characters\n" )_narg
	err_case( IOErrPragmaIncomplete, "pragma directive #%s incomplete\n" )_( stump );
	err_case( IOErrPragmaUnknown, "pragma directive #%s unknown\n" )_( stump );
	err_case( IOErrAlreadyDefined, "macro already defined - use #undef\n" )_narg
	err_case( IOErrElseWithoutIf, "#else without #if\n" )_narg
	err_case( IOErrEndifWithoutIf, "#endif without #if\n" )_narg
	err_case( IOErrElseAgain, "#else after #else\n" )_narg
	err_case( IOErrIfUnterminated, "unterminated #if\n" )_narg
	err_default( "syntax error\n" )_narg }

	StringReset( s, CNStringAll );
	return io->errnum;
}
