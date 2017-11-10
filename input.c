#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "input_util.h"
#include "io.h"
#include "hcn.h"
#include "output.h"
#include "output_util.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	flush_input
---------------------------------------------------------------------------*/
/*
	flush_input is called upon error from e.g. read_command and read_narrative.
	input is assumed to be '\n'-terminated
*/
void
freeInstructionBlock( _context *context )
{
	listItem *i, *next_i;
	for ( i = context->record.instructions; i!=NULL; i=next_i ) {
		next_i = i->next;
		free( i->ptr );
		freeItem( i );
	}
	context->record.instructions = NULL;
}

static void
freeLastInstruction( _context *context )
{
	listItem *instruction = context->record.instructions;
	if ( instruction == NULL ) return;
	listItem *next_i = instruction->next;
	free( instruction->ptr );
	freeItem( instruction );
	context->record.instructions = next_i;
}

int
flush_input( char *state, int event, char **next_state, _context *context )
{
	int do_flush = command_mode( FreezeMode, 0, 0 ) || context->error.flush_input;
	context->error.flush_input = 0;

	if ( do_flush && ( context->input.stack != NULL ) ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		switch ( input->mode ) {
		case InstructionBlock:
		case InstructionOne:
		case InstructionInline:
			do_flush = 0;
			break;
		default:
			break;
		}
	}

	// DO_LATER: there is a better way to flush now that input is bufferized
	if ( do_flush ) do {
		event = read_input( state, 0, NULL, context );
	} while ( event != '\n' );

	switch ( context->command.mode ) {
	case FreezeMode:
		break;
	case InstructionMode:
		free( context->record.string.ptr );
		context->record.string.ptr = NULL;
		freeLastInstruction( context );
		break;
	case ExecutionMode:
		if ( context->record.instructions == NULL )
			break;

		// here we are in the execution part of a loop => abort
		StackVA *stack = (StackVA *) context->command.stack->ptr;
		if (( stack->loop.candidates ))
		{
			output( Error, "aborting loop..." );
			freeListItem( &stack->loop.candidates );
			freeInstructionBlock( context );
			stack->loop.begin = NULL;
			pop_input( state, event, next_state, context );
			pop( state, event, next_state, context );
		}
		break;
	}

	return 0;
}

/*---------------------------------------------------------------------------
	set_instruction		- called from read_command()
---------------------------------------------------------------------------*/
void
set_instruction( listItem *instruction, _context *context )
{
	context->input.instruction = instruction;
	InputVA *input = context->input.stack->ptr;
	input->ptr.string = instruction->ptr;
	input->position = NULL;
}

/*---------------------------------------------------------------------------
	push_input
---------------------------------------------------------------------------*/
static char *
full_path( char *name )
{
	if (( name[ 0 ] == '/' ) ? ( name[ 1 ] != '/' ) : 1 )
		return name;

	struct stat buf;
	char *path;

	asprintf( &path, "%s%s", CN_BASE_PATH, &name[ 2 ] );
	if ( !stat( path, &buf ) ) {
		free( name );
	} else {
		free( path );
		asprintf( &path, "%sdefault/%s", CN_BASE_PATH, name );
	}
	return path;
}
int
push_input( char *identifier, void *src, InputType type, _context *context )
{
	InputVA *input;
#ifdef DEBUG
	if ((src)) outputf( Debug, "push_input: \"%s\"", (char*) src );
	else output( Debug, "push_input: NULL" );
#endif
	switch ( type ) {
	case HCNValueFileInput:
		src = strdup( src );
		// no break
	case PipeInput:
	case HCNFileInput:
	case FileInput:
		identifier = full_path( src );
		// check if stream is already in use
		for ( listItem *i=context->input.stack; i!=NULL; i=i->next )
		{
			input = (InputVA *) i->ptr;
			if ( strcmp( input->identifier, identifier ) )
				continue;
			int event = outputf( Error, "recursion in stream: \"%s\"", identifier );
			free( identifier );
			return event;
		}
		break;
	case ClientInput:
		push( base, 0, &same, context ); // protect variables
		identifier = "@"; // to prevent malicious popping
		break;
	case SessionOutputPipe:
		identifier = "@"; // to prevent malicious popping
		break;
	default:
		break;
	}

	// create and register new active stream
	input = (InputVA *) calloc( 1, sizeof(InputVA) );
	input->identifier = identifier;
	input->mode = type;
	input->level = context->command.level;
	input->restore.prompt = context->output.prompt;
	input->restore.position = context->input.buffer.position;
	input->restore.buffer = context->input.buffer.current;
	input->restore.flush = context->error.flush_input;

	int failed = 0;
	switch ( type ) {
	case HCNFileInput:
	case HCNValueFileInput:
		context->hcn.state = base;
		input->ptr.fd = open( identifier, O_RDONLY );
		failed = ( input->ptr.fd < 0 );
		break;
	case PipeInput:
		input->ptr.file = popen( identifier, "r" );
		failed = ( input->ptr.file == NULL );
		break;
	case FileInput:
		input->ptr.fd = open( identifier, O_RDONLY );
		failed = ( input->ptr.fd < 0 );
		break;
	case SessionOutputPipeInVariator:
		input->restore.record.mode = context->record.mode;
		input->restore.record.instructions = context->record.instructions;
		context->record.mode = RecordInstructionMode;
		context->record.instructions = NULL;
		// no break
	case ClientInput:
		input->client = io_open( IO_TICKET, src );
		input->ptr.string = context->io.input.buffer.ptr;
		input->ptr.string[ 0 ] = (char) 0;
		input->remainder = 0;
		input->position = NULL;
		break;
	case SessionOutputPipe:
	case SessionOutputPipeOut:
		input->client = *(int *) src;
		input->ptr.string = context->io.input.buffer.ptr;
		input->ptr.string[ 0 ] = (char) 0;
		input->remainder = 0;
		input->position = NULL;
		break;
	case InstructionBlock:
		context->record.level = context->command.level;
		listItem *first_instruction = (listItem *) src;
		context->input.instruction = first_instruction;
		input->ptr.string = (char *) first_instruction->ptr;
		input->position = NULL;
		break;
	case InstructionOne:
		input->ptr.string = (char *) src;
		input->position = NULL;
		break;
	case InstructionInline:
		; listItem *last_instruction = context->record.instructions;
		context->record.instructions = last_instruction->next;
		last_instruction->next = NULL;
		context->input.instruction = last_instruction;
		input->ptr.string = (char *) last_instruction->ptr;
		input->position = NULL;
		break;
	default:
		break;
	}
	if ( failed ) {
		free( input );
		return outputf( Error, "could not open stream: \"%s\"", identifier );
	}
	context->input.buffer.position = NULL;
	context->input.buffer.current = &input->buffer;

	// make stream current
	addItem( &context->input.stack, input );
	context->input.level++;

	// if we were in a for loop, reset command mode to record subsequent
	// instructions from stream
	if (( context->input.stack->next )) {
		input = (InputVA *) context->input.stack->next->ptr;
		if ( input->mode == InstructionInline ) {
			set_command_mode( InstructionMode, context );
			set_record_mode( RecordInstructionMode, 0, context );
		}
	}

	// no prompt in these modes
	context->output.prompt = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	pop_input
---------------------------------------------------------------------------*/
int
pop_input( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "pop_input: popping" );
#endif
	InputVA *input = (InputVA *) context->input.stack->ptr;
	int delta = context->command.level - input->level;
	if ( delta > 0 ) {
		event = output( Error, "reached premature EOF - restoring original stack level" );
		while ( delta-- ) pop( state, event, next_state, context );
	}
	else event = 0;

	context->output.prompt = input->restore.prompt;
	context->input.buffer.position = input->restore.position;
	context->input.buffer.current = input->restore.buffer;
	context->error.flush_input = input->restore.flush;

	int mode = input->mode;
	switch ( mode ) {
	case FileInput:
	case HCNFileInput:
	case HCNValueFileInput:
		close ( input->ptr.fd );
		free( input->identifier );
		break;
	case PipeInput:
		pclose( input->ptr.file );
		free( input->identifier );
		break;
	case SessionOutputPipeInVariator:
		io_close( IO_QUERY, input->client, "" );
		slist_close( &context->record.instructions, &context->record.string, 1 );
		reorderListItem( &context->record.instructions );
		assign_variable( &variator_symbol, context->record.instructions, StringVariable, context );
		context->record.instructions = input->restore.record.instructions;
		context->record.mode = input->restore.record.mode;
		break;
	case ClientInput:
		io_close( IO_TICKET, input->client, "" );
		break;
	case SessionOutputPipe:
	case SessionOutputPipeOut:
		io_close( IO_QUERY, input->client, "" );
		break;
	case InstructionInline:
        	free( context->input.instruction->ptr );
		freeListItem( &context->input.instruction );
		break;
	default:
		break;
	}
	free( input );
	popListItem( &context->input.stack );
	context->input.level--;

	if ( mode == ClientInput )
		pop( base, 0, &same, context );

	if ( context->input.stack != NULL ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( input->mode == InstructionInline ) {
			set_command_mode( ExecutionMode, context );
			set_record_mode( OffRecordMode, 0, context );
		}
	}

	return event;
}

/*---------------------------------------------------------------------------
	set_record_mode
---------------------------------------------------------------------------*/
void
set_record_mode( RecordMode mode, int event, _context *context )
{
	switch ( mode ) {
	case OffRecordMode:
		if ( context->record.mode == OnRecordMode )
		{
			// remove the last event '\n' from the record
			popListItem( &context->record.string.list );
			string_finish( &context->record.string, 1 );
		}
		else if ( context->record.mode == RecordInstructionMode )
		{
			slist_close( &context->record.instructions, &context->record.string, 0 );
		}
		context->record.mode = mode;
		break;
	case OnRecordMode:
		if (( context->record.mode == OffRecordMode ) && ( event >= 0 ))
		{
			string_start( &context->record.string, event );
			context->record.mode = mode;
		}
		break;
	case RecordInstructionMode:
                context->record.level = context->command.level;
		context->record.mode = mode;
		break;
	}
}

/*---------------------------------------------------------------------------
	recordC
---------------------------------------------------------------------------*/
static void
recordC( int event, int as_string, _context *context )
{
	// record instruction or expression if needed
	switch( context->record.mode ) {
	case RecordInstructionMode:
		slist_append( &context->record.instructions, &context->record.string, event, as_string, 1 );
		break;
	case OnRecordMode:
		string_append( &context->record.string, event );
		break;
	case OffRecordMode:
		break;
	}
}

/*---------------------------------------------------------------------------
	bufferize
---------------------------------------------------------------------------*/
#define COMMENT		0
#define BACKSLASH	1
#define STRING		2

int
bufferize( int event, int *mode, _context *context )
{
	if ((context->input.buffer.position)) {
		if ( !event ) {
			context->input.buffer.position = NULL;
			string_start( context->input.buffer.current, 0 );
			if ( context->input.eof ) {
				context->input.eof = 0;
				return EOF;
			}
			return 0;
		}
		return event;
	}
	else if ( !event ) {
		return 0;
	}
	else if ( event == EOF ) {
		string_finish( context->input.buffer.current, 1 );
		context->input.buffer.position = context->input.buffer.current->ptr;
		if ((context->input.buffer.position)) {
			context->input.eof = 1;
			return 0;
		}
		return EOF;
	}
	else if ( event == '\n' ) {
		if ( mode[ BACKSLASH ] ) {
			mode[ BACKSLASH ] = 0;
			if ( context->output.prompt ) {
				context->output.prompt = 0;
				fprintf( stderr, "$\t" );
			}
			return 0;
		}
		else if ( mode[ STRING ] ) {
			string_append( context->input.buffer.current, '\\' );
			string_append( context->input.buffer.current, 'n' );
			if ( context->output.prompt ) {
				context->output.prompt = 0;
				fprintf( stderr, "$\t" );
			}
			return 0;
		}
		else {
			string_append( context->input.buffer.current, '\n' );
			string_finish( context->input.buffer.current, 1 );
			context->input.buffer.position = context->input.buffer.current->ptr;
			return 0;
		}
	}
	else if ( event == '\\' )
	{
		if ( mode[ BACKSLASH ] ) {
			mode[ BACKSLASH ] = 0;
			if ( mode[ COMMENT ] )
				return 0;
			string_append( context->input.buffer.current, '\\' );
			string_append( context->input.buffer.current, event );
			return 0;
		}
		mode[ BACKSLASH ] = 1;
		return 0;
	}
	else if ( mode[ COMMENT ] ) {
		// ignore everything until '\n'
		mode[ BACKSLASH ] = 0;
		return 0;
	}
	else if ( mode[ BACKSLASH ] )
	{
		mode[ BACKSLASH ] = 0;
		// ignore '#' and '\' if backslashed
		string_append( context->input.buffer.current, '\\' );
		string_append( context->input.buffer.current, event );
		return 0;
	}
	else if ( event == '\t' ) {
		if ( mode[ STRING ] ) {
			string_append( context->input.buffer.current, '\\' );
			string_append( context->input.buffer.current, 't' );
			return 0;
		}
	}
	else if ( event == '"' ) {
		mode[ STRING ] = !mode[ STRING ];
	}
	else if ( event == '#' ) {
		mode[ COMMENT ] = 1;
		return 0;
	}
	string_append( context->input.buffer.current, event );
	return 0;
}

/*---------------------------------------------------------------------------
	sgetc
---------------------------------------------------------------------------*/
static int
sgetc( InputVA *input, _context *context )
{
	if ( input->position == NULL ) {
#ifdef DEBUG
		outputf( Debug, "sgetc: reading from \"%s\"", (char *) input->ptr.string );
#endif
		input->position = input->ptr.string;
		if ( context->narrative.mode.output && ( input->mode == InstructionBlock ))
			printtab( context->command.level + 1 );
	}
	int event = (int) (( char *) input->position++ )[ 0 ];
	if ( event )
	{
		if ( context->narrative.mode.output )
			outputf( Text, "%c", event );
	}
	else {
		switch ( input->mode ) {
		case ClientInput:
		case SessionOutputPipe:
			if ( io_read( input->client, input->ptr.string, &input->remainder ) )
			{
				input->position = input->ptr.string;
				event = (int) (( char *) input->position++ )[ 0 ];
			}
			break;
		case InstructionBlock:
			context->input.instruction = context->input.instruction->next;
			if ( context->input.instruction != NULL ) {
				input->ptr.string = context->input.instruction->ptr;
				input->position = NULL;
			}
			break;
		default:
			break;
		}
	}
	return event;
}

/*---------------------------------------------------------------------------
	read_input
---------------------------------------------------------------------------*/
int
read_input( char *state, int event, char **next_state, _context *context )
{
	if ( event != 0 ) {
		return event;
	}
	int mode[ 3 ] = { 0, 0, 0 };
	do {
		if ((context->input.buffer.position))
		{
			event = (int) ((char *) context->input.buffer.position++ )[ 0 ];
		}
		else if ( context->input.stack == NULL )
		{
			if ( context->control.cgi || context->control.session )
				return output( Debug, "input: wrong mode" ); // should be impossible

			StackVA *stack = context->command.stack->ptr;
			if ( stack->narrative.state.closure || !strcmp( state, base ) ) {
				prompt( context );
			}

			context->output.anteprompt = 0;

			read( STDIN_FILENO, &event, 1 );

			switch ( event ) {
			case 0:
				return 0;
			case '\n':
				context->output.prompt = ttyu( context );
			default:
				break;
			}
		}
		else if ( !context->input.eof )
		{
			InputVA *input = (InputVA *) context->input.stack->ptr;
			if ( input->shed ) {
				// e.g. user tried to pop control stack beyond input level
				event = EOF;
			}
			else switch ( input->mode ) {
			case HCNFileInput:
			case HCNValueFileInput:
				event = hcn_getc( input->ptr.fd, context );
				if ( !event ) event = EOF;
				break;
			case PipeInput:
				event = fgetc( input->ptr.file );
				break;
			case FileInput:
				if ( !read( input->ptr.fd, &event, 1 ) )
					event = EOF;
				break;
			case ClientInput:
			case SessionOutputPipe:
			case InstructionInline:
			case InstructionOne:
				event = sgetc( input, context );
				if ( !event ) event = EOF;
				break;
			case SessionOutputPipeInVariator:
				event = io_read( input->client, input->ptr.string, &input->remainder );
				if ( !event ) event = EOF;
				else {
					for ( char *ptr = input->ptr.string; *ptr; )
						recordC( *ptr++, 1, context );
					event = 0;
				}
				break;
			case SessionOutputPipeOut:
				event = ( io_read( input->client, input->ptr.string, &input->remainder ) ?
					output( Text, input->ptr.string ) : EOF );
				break;
			case InstructionBlock:
				event = sgetc( input, context );
				break;
			default:
				break;
			}
		}
		event = bufferize( event, mode, context );

	}
	while ( !event );

	if ( event == EOF ) {
		context->error.flush_input = 0;
		InputVA *input = (InputVA *) context->input.stack->ptr;
		switch ( input->mode ) {
		case InstructionOne:
			return '\n';
		default:
			return 0;
		}
	}

	context->error.flush_input = ( event != '\n' );
	if ( context->error.tell )
		context->error.tell = ( event == '\n' ) ? 2 : 1;

	recordC( event, 0, context );

	return event;
}

