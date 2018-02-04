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
#include "path.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	read_input
---------------------------------------------------------------------------*/
static int bufferize( int event, int *mode, _context * );
static int sgetc( InputVA *, _context * );
static void recordC( int event, int nocr, _context * );

int
read_input( char *state, int event, char **next_state, _context *context )
{
	if ( event != 0 ) {
		return event;
	}
	int mode[ 3 ] = { 0, 0, 0 };
	do {
		if ((context->input.buffer.position)) {
			event = (int) ((char *) context->input.buffer.position++ )[ 0 ];
		}
		else if ( context->input.stack == NULL ) {
			if ( context->control.cgi || context->control.session )
				return output( Debug, "input: wrong mode" ); // should be impossible

			StackVA *stack = context->command.stack->ptr;
			if ( stack->narrative.state.closure || !strcmp( state, base ) || !strcmp( state, "?!:_/" ) )
				prompt( context );

			context->output.anteprompt = 0;
			read( STDIN_FILENO, &event, 1 );
			if ( event == '\n' ) {
				context->output.prompt = ttyu( context );
			}
			else if ( !event ) {
				return 0;
			}
		}
		else if ( !context->input.eof ) {
			InputVA *input = (InputVA *) context->input.stack->ptr;
			if ( input->discard ) {
				// e.g. user tried to pop control stack beyond input level
				event = EOF;
			}
			else switch ( input->mode ) {
			case HCNFileInput:
			case HCNValueFileInput:
				event = hcn_getc( input->ptr.fd, context );
				if ( !event ) event = EOF;
				break;
			case UNIXPipeInput:
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
			case SessionOutputPipeToVariable:
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

#define COMMENT		0
#define BACKSLASH	1
#define STRING		2
static int
bufferize( int event, int *mode, _context *context )
{
	if ((context->input.buffer.position)) {
		switch ( event ) {
		case 0:
			context->input.buffer.position = NULL;
			string_start( context->input.buffer.ptr, 0 );
			if ( context->input.eof ) {
				context->input.eof = 0;
				return EOF;
			}
			break;
		default:
			return event;
		}
	}
	else if ( event == 0 ) {
		return 0;
	}
	else if ( event == EOF ) {
		string_finish( context->input.buffer.ptr, 1 );
		context->input.buffer.position = context->input.buffer.ptr->value;
		if ((context->input.buffer.position)) {
			context->input.eof = 1;
			return 0;
		}
		return EOF;
	}
	else if ( mode[ COMMENT ] ) {
		// ignore everything until '\n'
		if ( event == '\n' ) {
			string_append( context->input.buffer.ptr, '\n' );
			string_finish( context->input.buffer.ptr, 1 );
			context->input.buffer.position = context->input.buffer.ptr->value;
		}
	}
	else if ( mode[ BACKSLASH ] ) {
		switch ( event ) {
		case '\n':
			if ( context->output.prompt ) {
				context->output.prompt = 0;
				fprintf( stderr, "$\t" );
			}
			break;
		default:
			string_append( context->input.buffer.ptr, '\\' );
			string_append( context->input.buffer.ptr, event );
		}
		mode[ BACKSLASH ] = 0;
	}
	else if ( mode[ STRING ] ) {
		switch ( event ) {
		case '\n':
			if ( context->output.prompt ) {
				context->output.prompt = 0;
				fprintf( stderr, "$\t" );
			}
			string_append( context->input.buffer.ptr, '\\' );
			string_append( context->input.buffer.ptr, 'n' );
			break;
		case '\t':
			string_append( context->input.buffer.ptr, '\\' );
			string_append( context->input.buffer.ptr, 't' );
			break;
		case '\\':
			mode[ BACKSLASH ] = 1;
			break;
		case '"':
			mode[ STRING ] = 0;
			// no break
		default:
			string_append( context->input.buffer.ptr, event );
		}
	}
	else {
		switch ( event ) {
		case '\n':
			string_append( context->input.buffer.ptr, '\n' );
			string_finish( context->input.buffer.ptr, 1 );
			context->input.buffer.position = context->input.buffer.ptr->value;
			break;
		case '#':
			mode[ COMMENT ] = 1;
			break;
		case '\\':
			mode[ BACKSLASH ] = 1;
			break;
		case '"':
			mode[ STRING ] = 1;
			// no break
		default:
			string_append( context->input.buffer.ptr, event );
		}
	}
	return 0;
}

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
	if ( event ) {
		if ( context->narrative.mode.output )
			outputf( Text, "%c", event );
	}
	else {
		switch ( input->mode ) {
		case ClientInput:
		case SessionOutputPipe:
			if ( io_read( input->client, input->ptr.string, &input->remainder ) ) {
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

static void
recordC( int event, int nocr, _context *context )
{
	// record instruction or expression if needed
	switch ( context->record.mode ) {
	case RecordInstructionMode:
		slist_append( &context->record.instructions, &context->record.string, event, nocr, 1 );
		break;
	case OnRecordMode:
		string_append( &context->record.string, event );
		break;
	case OffRecordMode:
		break;
	}
}

/*---------------------------------------------------------------------------
	push_input
---------------------------------------------------------------------------*/
static InputVA *newInput( char *identifier, InputType, _context * );
static void freeInput( InputVA *, _context * );

int
push_input( char *identifier, void *src, InputType type, _context *context )
{
#ifdef DEBUG
	if ((src)) outputf( Debug, "push_input: \"%s\"", (char *) src );
	else output( Debug, "push_input: NULL" );
#endif
	switch ( type ) {
	case HCNValueFileInput:
		src = strdup( src );
		// no break
	case HCNFileInput:
	case UNIXPipeInput:
	case FileInput:
		identifier = cn_path( context, "r", src );
		if ( identifier != src ) free( src );
		// check if stream is already in use
		for ( listItem *i=context->input.stack; i!=NULL; i=i->next )
		{
			InputVA *input = i->ptr;
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
	InputVA *input = newInput( identifier, type, context );
	int failed = 0;
	switch ( type ) {
	case HCNFileInput:
	case HCNValueFileInput:
		context->input.hcn = 1;
		context->command.one = 0;
		context->hcn.state = base;
		input->ptr.fd = open( identifier, O_RDONLY );
		failed = ( input->ptr.fd < 0 );
		break;
	case UNIXPipeInput:
		context->command.one = 0;
		input->ptr.file = popen( identifier, "r" );
		failed = ( input->ptr.file == NULL );
		break;
	case FileInput:
		context->command.one = 0;
		input->ptr.fd = open( identifier, O_RDONLY );
		failed = ( input->ptr.fd < 0 );
		break;
	case ClientInput:
		input->client = io_open( IO_TICKET, src );
		input->ptr.string = context->io.input.buffer.ptr;
		input->ptr.string[ 0 ] = (char) 0;
		input->remainder = 0;
		input->position = NULL;
		break;
	case SessionOutputPipeToVariable:
		input->variable.identifier = take_identifier( context, 0 );
		input->restore.record.mode = context->record.mode;
		input->restore.record.instructions = context->record.instructions;
		context->record.mode = RecordInstructionMode;
		context->record.instructions = NULL;
		// no break
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
		input->restore.instruction = context->input.instruction;
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
		outputf( Error, "could not open stream: \"%s\"", identifier );
		freeInput( input, context );
		free( identifier );
		return output( Error, NULL );
	}
	context->input.buffer.position = NULL;
	context->input.buffer.ptr = &input->buffer;
	context->input.eof = 0;

	// make stream current (register)
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

static InputVA *
newInput( char *identifier, InputType type, _context *context )
{
	InputVA *input = calloc( 1, sizeof(InputVA) );
	input->identifier = identifier;
	input->mode = type;
	input->level = context->command.level;
	input->restore.prompt = context->output.prompt;
	input->restore.position = context->input.buffer.position;
	input->restore.buffer = context->input.buffer.ptr;
	input->restore.eof = context->input.eof;
	input->restore.flush = context->error.flush_input;
	input->restore.command_one = context->command.one;
	return input;
}
static void
freeInput( InputVA *input, _context *context )
{
	context->output.prompt = input->restore.prompt;
	context->input.buffer.position = input->restore.position;
	context->input.buffer.ptr = input->restore.buffer;
	context->input.eof = input->restore.eof;
	context->error.flush_input = input->restore.flush;
	context->command.one = input->restore.command_one;
	free( input );
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
		outputf( Debug, "command level=%d, input level=%d", context->command.level, input->level );
		while ( delta-- ) pop( state, event, next_state, context );
	}
	else event = 0;

	int mode = input->mode;
	switch ( mode ) {
	case HCNFileInput:
	case HCNValueFileInput:
		context->input.hcn = 0;
		// no break
	case FileInput:
		close ( input->ptr.fd );
		free( input->identifier );
		break;
	case UNIXPipeInput:
		pclose( input->ptr.file );
		free( input->identifier );
		break;
	case ClientInput:
		io_close( IO_TICKET, input->client, "" );
		break;
	case SessionOutputPipeToVariable:
		io_close( IO_QUERY, input->client, "" );
		slist_close( &context->record.instructions, &context->record.string, 1 );
		reorderListItem( &context->record.instructions );
		char *identifier = input->variable.identifier;
		if (( context->record.instructions )) {
			listItem *value = context->record.instructions;
			assign_variable( AssignSet, &identifier, value, StringResults, context );
		}
		else free( identifier );
		input->variable.identifier = NULL;
		context->record.instructions = input->restore.record.instructions;
		context->record.mode = input->restore.record.mode;
		break;
	case SessionOutputPipe:
	case SessionOutputPipeOut:
		io_close( IO_QUERY, input->client, "" );
		break;
	case InstructionBlock:
		context->input.instruction = input->restore.instruction;
		break;
	case InstructionInline:
        	free( context->input.instruction->ptr );
		freeListItem( &context->input.instruction );
		break;
	default:
		break;
	}
	freeInput( input, context );
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
	flush_input
---------------------------------------------------------------------------*/
static void freeLastInstruction( _context * );

int
flush_input( char *state, int event, char **next_state, _context *context )
/*
	flush_input is called upon error from e.g. read_command and read_narrative.
	input is assumed to be '\n'-terminated
*/
{
	int do_flush = 1;
	if (( context->input.stack )) {
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

	switch ( context->command.mode ) {
	case FreezeMode:
#if 1	// actually this is just an optimization, as FreezeMode is supposed to
	// be handled at command level, so far...
		if ( do_flush ) {
			do { event = read_input( state, 0, &same, context ); }
			while ( event != '\n' );
		}
#endif
		break;
	case InstructionMode:
		if ( context->error.flush_input ) {
			do { event = read_input( state, 0, &same, context ); }
			while ( event != '\n' );
		}
		else {
			string_start( &context->record.string, 0 );
			freeLastInstruction( context );
		}
		break;
	case ExecutionMode:
		if ( context->error.flush_input ) {
			do { event = read_input( state, 0, &same, context ); }
			while ( event != '\n' );
		}
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
	context->error.flush_input = 0;
	return 0;
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

/*---------------------------------------------------------------------------
	set_record_mode
---------------------------------------------------------------------------*/
void
set_record_mode( RecordMode mode, int event, _context *context )
{
	switch ( mode ) {
	case OffRecordMode:
		if ( context->record.mode == OnRecordMode ) {
			// remove the last event '\n' from the record
			popListItem( (listItem **) &context->record.string.value );
			string_finish( &context->record.string, 1 );
		}
		else if ( context->record.mode == RecordInstructionMode ) {
			slist_close( &context->record.instructions, &context->record.string, 0 );
		}
		context->record.mode = mode;
		break;
	case OnRecordMode:
		if (( context->record.mode == OffRecordMode ) && ( event >= 0 )) {
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

