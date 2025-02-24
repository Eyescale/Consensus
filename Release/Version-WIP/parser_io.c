#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define DEBUG
#include "parser_macros.h"
#include "parser_io.h"

//===========================================================================
//	io_init / io_exit
//===========================================================================
static int io_pop( CNIO * );

void
io_init( CNIO *io, void *stream, char *path, IOType type, listItem *flags ) {
	memset( io, 0, sizeof(CNIO) );
	Registry *registry = newRegistry( IndexedByName );
	for ( listItem *i=flags; i!=NULL; i=i->next ) {
		char *flag = strdup((char *) i->ptr );
		registryRegister( registry, flag, NULL ); }
	io->control.registry = registry;
	io->term.string = newString();
	io->string = newString();
	io->text = newString();
	io->stream = stream;
	io->path = path;
	io->type = type;
	io->state = "";
	io->line = 1; }

static freeRegistryCB free_CB;
void
io_exit( CNIO *io ) {
	freeRegistry( io->control.registry, free_CB );
	freeListItem( &io->control.stack );
	freeString( io->term.string );
	freeListItem( &io->buffer );
	freeString( io->string );
	freeString( io->text );
	while (( io->stack )) io_pop( io ); }

static void
free_CB( Registry *registry, Pair *entry ) {
	free((char *) entry->name );
	if (( entry->value ))
		free((char *) entry->value ); }

void
io_reset( CNIO *io, int l, int c ) {
	io->line=l; io->column=c; }

//===========================================================================
//	io_getc
//===========================================================================
int
io_getc( CNIO *io, int last_event ) {
	if ( last_event=='\n' ) { io->line++; io->column = 0; }
	int event = fgetc( io->stream );
	io->column++;
	return event; }

//===========================================================================
//	io_read
//===========================================================================
#define BGN { for ( ; ; ) {
#define END } }
static inline int read_event( CNIO * );
static IOEventAction preprocess( CNIO *, int event );
static int io_push( CNIO *, IOType, char * );
static int io_pragma_execute( CNIO * );

int
io_read( CNIO *io, int last_event ) BGN
/*
	filters out comments and \cr line continuation from input
*/
	listItem **buffer;
	int event, action;
	char *term;
	do {
		if (( io->term.ptr )) {
			event = io->term.ptr[0];
			if ( event ) {
				io->term.ptr++;
				return event; }
			event = io->term.restore;
			io->term.ptr = NULL;
			StringReset( io->term.string, CNStringAll ); }
		else {
			if ( last_event=='\n' ) { io->line++; io->column = 0; }
			event = read_event( io ); }
		action = preprocess( io, event );
		if ( io->errnum ) return event; // err
		switch ( action ) {
		case IOEventTake:
			if ( event!=EOF || io_pop( io ) )
				action = IOEventDone;
			else last_event = 0;
			break;
		case IOEventTerm:
			StringAppend( io->term.string, event );
			last_event = event;
			io->column++;
			break;
		case IOEventTranslate:
			io->term.restore = event;
			term = StringFinish( io->term.string, 0 );
			Pair *entry = registryLookup( io->control.registry, term );
			io->term.ptr = (entry) ?
				(entry->value) ? entry->value : "" :
				term;
			break;
		case IOEventPragma:
			if ( io_pragma_execute( io ) )
				return event; // err
			last_event = event; // presumably '\n'
			break;
		case IOEventPass:
			// update io->line and io->column wrt io->buffer
			if (( io->buffer )) {
				buffer = &io->buffer;  // flush
				reorderListItem( buffer );
				while (( last_event=pop_item( buffer ) )) {
					if ( last_event=='\n' ) {
						io->line++; io->column=0; }
					else io->column++; } }
			// update io->column wrt event
			if ( event!='\n' ) io->column++;
			last_event = event;
			break;
		case IOEventQueue:
			add_item( &io->buffer, event );
			last_event = 0;
			break;
		case IOEventRestore:
			buffer = &io->buffer;
			add_item( buffer, event );
			reorderListItem( buffer );
			io_push( io, IOBuffer, NULL );
			last_event = 0;
			break; }
	} while ( action != IOEventDone );
	if ( event==EOF ) {
		if ( io->control.mode & ~IO_UNPOUND )
			io->errnum = IOErrUnterminatedIf; }
	else {
		if ( event!='\n' ) io->column++;
		if (( io->buffer )) io_push( io, IOBuffer, NULL );
		if ( io->control.mode & IO_SILENT ) {
			last_event = event;
			event = 0; } }
	if ( event )
		return event;
	END

static inline int
read_event( CNIO *io ) BGN
	int event;
	switch ( io->type ) {
	case IOStreamFile:
	case IOStreamInput:
		event = fgetc( io->stream );
		break;
	case IOBuffer:
		event = pop_item((listItem **) &io->stream );
		// Assumption: EOF can only be last in IOBuffer
		if ( !event || event==EOF ) // pop IOBuffer
			io_pop( io ); } // while passing event
	if ( event )
		return event;
	END

//---------------------------------------------------------------------------
//	preprocess
//---------------------------------------------------------------------------
static inline IOControlPragma pragma_type( CNIO * );
#define t_take	StringAppend( t, event );

static IOEventAction
preprocess( CNIO *io, int event ) {
	CNString *s = io->string;
	CNString *t = io->text;
	char *	state = io->state;
	int	line = io->line,
		column = io->column,
		action = 0,
		errnum = 0;

	BMParseBegin( "preprocess", state, event, line, column )
	on_( EOF ) bgn_
		in_( "/" )	do_( "pop" ) 	action = IOEventRestore;
		in_( "\\" )	do_( "pop" ) 	action = IOEventRestore;
		in_( "\"\"" )	do_( "pop" ) 	action = IOEventRestore;
		in_( "\"\"\\" ) do_( "pop" )	action = IOEventRestore;
		in_other	do_( "" )	action = IOEventTake;
						errnum = s_empty ? 0 : IOErrUnexpectedEOF;
		end
		in_( "pop" ) bgn_
			on_any	do_( "" )	action = IOEventTake;
			end
	in_( "" ) bgn_
		on_( '#' )
			if ( column || io->type==IOStreamInput ) {
				do_( "//" )	action = IOEventPass; }
			else {	do_( "#" )	action = IOEventPass; }
		on_( '/' )	do_( "/" )	action = IOEventQueue;
		on_( '\\' )	do_( "\\" )	action = IOEventQueue;
		on_( '"' )	do_( "\"" )	action = IOEventTake;
		on_( '\'' )	do_( "'" )	action = IOEventTake;
		on_separator	do_( same )	action = IOEventTake;
		on_other if ( io->control.mode & IO_SILENT ) {
				do_( same )	action = IOEventTake; }
			else {	do_( "term" )	action = IOEventTerm; }
		end
	in_( "term" ) bgn_
		on_separator	do_( "" )	action = IOEventTranslate;
		on_other	do_( same )	action = IOEventTerm;
		end
	in_( "\\" ) bgn_
		on_( '\n' )	do_( "\\_" )	action = IOEventPass;
		on_other	do_( "pop" )	action = IOEventRestore;
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
			on_other		do_( "pop" )	action = IOEventRestore;
			end
		in_( "\"\"\\" ) bgn_
			on_( '\n' )	do_( "\"\"" )	action = IOEventQueue;
			on_other	do_( "pop" )	action = IOEventRestore;
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
		on_other	do_( "pop" )	action = IOEventRestore;
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
		on_( '#' )	do_( "//" )	action = IOEventPass;
						io->control.mode &= ~IO_UNPOUND;
		on_( '~' )	do_( "#~" )	action = IOEventPass;
		on_separator if ( io->control.mode & IO_UNPOUND ) {
				do_( "" )	REENTER } // unpounding
			else {	do_( "#?" )	REENTER }
		on_other	do_( "#?" )	REENTER
		end
	in_( "#~" ) bgn_
		on_( '#' )	do_( "" )	action = IOEventPass;
						io->control.mode |= IO_UNPOUND;
		on_other	do_( "//" )	REENTER
		end
	in_( "#?" ) bgn_
		ons( " \t" ) switch ( pragma_type( io ) ) {
			case IOPragma_include:
				do_( "#<" )	action = IOEventPass;
						break;
			case IOPragma_define:
				do_( "#D" )	action = IOEventPass;
						break;
			case IOPragmaNary:
				do_( "#$" )	action = IOEventPass;
						break;
			case IOPragmaUnary:
				do_( "#_" )	action = IOEventPass;
						break; 
			default:
				do_( "//" ) 	action = IOEventPass;
						break; }
		on_( '\n' ) switch ( pragma_type( io ) ) {
			case IOPragma_include:
			case IOPragma_define:
			case IOPragmaNary:
				do_( "" )	errnum = IOErrPragmaIncomplete;
						break;
			case IOPragmaUnary:
				do_( "" )	action = IOEventPragma;
						break;
			default:
				do_( "//" )	REENTER
						break; }
		on_separator 	do_( "//" )	s_reset( CNStringAll )
						action = IOEventPass;
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
		in_( "#D" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_separator	do_( "" )	errnum = IOErrPragmaIncomplete;
			on_other	do_( "#D?" )	s_take
							action = IOEventPass;
			end
			in_( "#D?" ) bgn_
				ons( " \t" )	do_( "#D$" )	action = IOEventPass;
				on_( '\n' )	do_( "" )	action = IOEventPragma;
				on_separator	do_( "" )	errnum = IOErrTrailingChar;
				on_other	do_( same )	s_take
								action = IOEventPass;
			end
			in_( "#D$" ) bgn_
				ons( " \t" )	do_( same )	action = IOEventPass;
				on_( '\n' )	do_( "" )	action = IOEventPragma;
				on_other	do_( "#D_" )	t_take
								action = IOEventPass;
				end
			in_( "#D_" ) bgn_
				on_( '\n' )	do_( "" )	action = IOEventPragma;
				on_other	do_( same )	t_take
								action = IOEventPass;
				end
		in_( "#$" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_separator	do_( "" )	errnum = IOErrPragmaIncomplete;
			on_other	do_( "#$_" )	s_take
							action = IOEventPass;
			end
			in_( "#$_" ) bgn_
				on_separator	do_( "#_" )	REENTER
				on_other	do_( same )	s_take
								action = IOEventPass;
				end
		in_( "#_" ) bgn_
			ons( " \t" )	do_( same )	action = IOEventPass;
			on_( '/' )	do_( "#_/" )	action = IOEventPass;
			on_( '\n' )	do_( "" )	action = IOEventPragma;
			on_other	do_( "" )	errnum = IOErrTrailingChar;
			end
			in_( "#_/" ) bgn_
				on_( '/' )	do_( "#_//" )	action = IOEventPass;
				on_other	do_( "" )	errnum = IOErrTrailingChar;
				end
			in_( "#_//" ) bgn_
				on_( '\n' )	do_( "" )	action = IOEventPragma;
				on_other	do_( same )	action = IOEventPass;
				end
	BMParseEnd

	io->errnum = errnum;
	io->state = state;
	return action; }

static inline IOControlPragma
pragma_type( CNIO *io ) {
	char *p;
	CNString *s = io->string;
	IOControlPragma type =
		!s_cmp( "include" )	? IOPragma_include :
		!s_cmp( "define" )	? IOPragma_define :
		!s_cmp( "undef" )	? IOPragma_undef :
		!s_cmp( "ifdef" )	? IOPragma_ifdef :
		!s_cmp( "ifndef" )	? IOPragma_ifndef :
		!s_cmp( "eldef" )	? IOPragma_eldef :
		!s_cmp( "elndef" )	? IOPragma_elndef :
		!s_cmp( "else" )	? IOPragma_else :
		!s_cmp( "endif" )	? IOPragma_endif :
		IOPragmaNone;

	switch (( io->control.pragma = type )) {
	case IOPragma_include:
	case IOPragma_define:
		break;
	case IOPragma_else:
	case IOPragma_endif:
		type = IOPragmaUnary;
		break;
	case IOPragma_undef:
	case IOPragma_ifdef:
	case IOPragma_ifndef:
	case IOPragma_eldef:
	case IOPragma_elndef:
		type = IOPragmaNary;
		break;
	case IOPragmaNone:
	default: if (( p=StringFinish(s,0) ))
			fprintf( stderr, "B%%: bm_preprocess: Warning: "
			"unknown preprocessing directive #%s\n", p ); }
	s_reset( CNStringAll )
	return type; }

//---------------------------------------------------------------------------
//	io_pragma_execute
//---------------------------------------------------------------------------
static int
io_pragma_execute( CNIO *io ) {
	union { void *ptr; int value; } parent_mode;
	CNString *s = io->string;
	CNString *t = io->text;
	char *param = StringFinish( s, 0 );
	int pragma = io->control.pragma;
	int mode = io->control.mode;
	int errnum = 0;
	Pair *entry;
	if ( mode & IO_SILENT ) {
		switch ( pragma ) {
		case IOPragma_include:
		case IOPragma_define:
		case IOPragma_undef:
			break;
		case IOPragma_ifdef:
		case IOPragma_ifndef:
			add_item( &io->control.stack, mode );
			io->control.mode = IO_SILENT;
			break;
		case IOPragma_eldef:
			parent_mode.ptr = io->control.stack->ptr;
			if ( mode & IO_ELSE )
				errnum = IOErrElseAgain;
			else if ( mode & IO_ELIF ) {}
			else if ( parent_mode.value & IO_SILENT ) {}
			else if (( registryLookup( io->control.registry, param ) ))
				io->control.mode = IO_ACTIVE|IO_ELIF;
			break;
		case IOPragma_elndef:
			parent_mode.ptr = io->control.stack->ptr;
			if ( mode & IO_ELSE )
				errnum = IOErrElseAgain;
			else if ( mode & IO_ELIF ) {}
			else if ( parent_mode.value & IO_SILENT ) {}
			else if ( !registryLookup( io->control.registry, param ) )
				io->control.mode = IO_ACTIVE|IO_ELIF;
			break;
		case IOPragma_else:
			if ( mode & IO_ELSE )
				errnum = IOErrElseAgain;
			else if ( mode & IO_ELIF )
				io->control.mode = IO_SILENT|IO_ELSE;
			else {
				parent_mode.ptr = io->control.stack->ptr;
				io->control.mode = ( parent_mode.value & IO_SILENT ) ?
					IO_SILENT|IO_ELSE : IO_ACTIVE|IO_ELSE; }
			break;
		case IOPragma_endif:
			io->control.mode = pop_item( &io->control.stack );
			break; } }
	else {
		switch ( pragma ) {
		case IOPragma_include:
			errnum = io_push( io, IOStreamFile, param );
			if ( !errnum ) StringReset( s, CNStringMode );
			break;
		case IOPragma_define:
			if (( !registryLookup( io->control.registry, param ) )) {
				char *args = StringFinish( t, 0 );
				StringReset( t, CNStringMode );
				registryRegister( io->control.registry, param, args );
				StringReset( s, CNStringMode ); }
			else errnum = IOErrAlreadyDefined;
			break;
		case IOPragma_undef:
			registryDeregister( io->control.registry, param );
			break;
		case IOPragma_ifdef:
			entry = registryLookup( io->control.registry, param );
			add_item( &io->control.stack, mode );
			io->control.mode = ((entry) ? IO_ACTIVE : IO_SILENT );
			break;
		case IOPragma_ifndef:
			entry = registryLookup( io->control.registry, param );
			add_item( &io->control.stack, mode );
			io->control.mode = ((entry) ? IO_SILENT : IO_ACTIVE );
			break;
		case IOPragma_eldef:
		case IOPragma_elndef:
			if ( !mode )
				errnum = IOErrElseWithoutIf;
			else if ( mode & IO_ELSE )
				errnum = IOErrElseAgain;
			else
				io->control.mode = IO_SILENT|IO_ELIF;
			break;
		case IOPragma_else:
			if ( !mode )
				errnum = IOErrElseWithoutIf;
			else if ( mode & IO_ELSE )
				errnum = IOErrElseAgain;
			else
				io->control.mode = IO_SILENT|IO_ELSE;
			break;
		case IOPragma_endif:
			if ( mode ) io->control.mode = pop_item( &io->control.stack );
			else errnum = IOErrEndifWithoutIf;
			break; } }
	if ( !errnum ) {
		StringReset( s, CNStringAll );
		StringReset( t, CNStringAll ); }
	io->control.pragma = 0;
	io->errnum = errnum;
	return errnum; }

//---------------------------------------------------------------------------
//	io_push / io_pop
//---------------------------------------------------------------------------
static FILE * io_open( CNIO *io, char **path );

static int
io_push( CNIO *io, IOType type, char *path ) {
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
		FILE *stream = io_open( io, &path );
		if ( !stream )
			return IOErrFileNotFound;
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
		return 0; } }

static FILE *
io_open( CNIO *io, char **path )
/*
	make path relative to current file path
*/ {
	if ( **path!='/' ) {
		char *p = io->path;
		if ( io->type!=IOStreamFile ) {
			p = NULL;
			for ( listItem *i=io->stack; i!=NULL; i=i->next->next ) {
				union { IOType value; void *ptr; } type;
				type.ptr = ((Pair *) i->ptr )->name;
				if ( type.value==IOStreamFile ) {
					p = ((Pair *) i->next->ptr )->value;
					break; } } }
		if (( p )) {
			char *q = p;
			for ( char *r=p; *r; r++ )
				switch ( *r ) {
				case '\\': r++; break;
				case '/': q=r; break; }
			if ( *q=='/' ) {
				CNString *s = newString();
				s_append( p, q )
				s_put( '/' )
				s_add( *path )
				p = StringFinish( s, 0 );
				FILE *file = fopen( p, "r" );
				if (( file )) {
					free( *path ); *path = p;
					s_reset( CNStringMode ) }
				freeString( s );
				return file; } } }
	return fopen( *path, "r" ); }

static int
io_pop( CNIO *io )
/*
	Assumption: io->stack != NULL
*/ {
	if ( !io->stack ) return EOF;
	listItem **stack = &io->stack;

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
	io->type = cast_i( pair->name );
	if ( previous!=IOBuffer ) {
		io->line = cast_i( pair->value );
		io->column = 0; }
	freePair( pair );

	pair = popListItem( stack );
	io->stream = pair->name;
	io->path = pair->value;
	freePair( pair );
	return 0; }

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
io_report( CNIO *io ) {
	CNString *s = io->string;
	CNString *t = io->text;
	char *stump = StringFinish( s, 0 );
	fprintf( stderr, "Error: bm_preprocess: " );
	if (( io->path )) fprintf( stderr, "in %s, ", io->path );
	fprintf( stderr, "l%dc%d: ", io->line, io->column );

	switch ( io->errnum ) {
	err_case( IOErrUnexpectedEOF, "unexpected EOF\n" )_narg
	err_case( IOErrFileNotFound, "could not open file: \"%s\"\n" )_( stump )
	err_case( IOErrIncludeFormat, "unknown include format\n" )_narg
	err_case( IOErrTrailingChar, "trailing characters\n" )_narg
	err_case( IOErrPragmaIncomplete, "pragma directive #%s incomplete\n" )_( stump );
	err_case( IOErrPragmaUnknown, "pragma directive #%s unknown\n" )_( stump );
	err_case( IOErrAlreadyDefined, "%s macro redefined - use #undef\n" )_( stump )
	err_case( IOErrElseWithoutIf, "#else without #if\n" )_narg
	err_case( IOErrEndifWithoutIf, "#endif without #if\n" )_narg
	err_case( IOErrElseAgain, "#else after #else\n" )_narg
	err_case( IOErrUnterminatedIf, "EOF: unterminated #if\n" )_narg
	err_default( "syntax error\n" )_narg }

	StringReset( s, CNStringAll );
	StringReset( t, CNStringAll );
	return io->errnum; }

