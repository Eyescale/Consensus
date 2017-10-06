#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/types.h>
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
	int do_flush = context_check( FreezeMode, 0, 0 ) || context->error.flush_input;

	if ( do_flush && ( context->input.stack != NULL ) ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		switch ( input->mode ) {
		case InstructionBlock:
		case InstructionOne:
		case LastInstruction:
			do_flush = 0;
			break;
		default:
			break;
		}
	}

	if ( do_flush ) do {
		event = input( state, 0, NULL, context );
	} while ( event != '\n' );

	switch ( context->control.mode ) {
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
		StackVA *stack = (StackVA *) context->control.stack->ptr;
		output( Error, "aborting loop..." );
		freeListItem( &stack->loop.index );
		freeInstructionBlock( context );
		stack->loop.begin = NULL;
		pop_input( state, event, next_state, context );
		pop( state, event, next_state, context );
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
int
push_input( char *identifier, void *src, InputType type, _context *context )
{
	InputVA *input;
#ifdef DEBUG
	output( Debug, "push_input: \"%s\"", identifier );
#endif
	switch ( type ) {
	case PipeInput:
	case HCNFileInput:
	case StreamInput:
		// check if stream is already in use
		for ( listItem *i=context->input.stack; i!=NULL; i=i->next )
		{
			input = (InputVA *) i->ptr;
			if ( strcmp( input->identifier, identifier ) )
				continue;
			free( identifier );
			return output( Error, "recursion in stream: \"%s\"", identifier );
		}
		break;
	default:
		break;
	}

	// create and register new active stream
	input = (InputVA *) calloc( 1, sizeof(InputVA) );
	input->identifier = identifier;
	input->level = context->control.level;
	input->restore.prompt = ( context->control.prompt ? 1 : 0 );
	input->mode = type;

#ifdef DEBUG
	output( Debug, "opening stream: \"%s\"", identifier );
#endif
	switch ( type ) {
	case HCNFileInput:
	case PipeInput:
	case StreamInput:
		; int failed = 0;
		if ( type == HCNFileInput ) {
			input->ptr.fd = open( identifier, O_RDONLY );
			failed = ( input->ptr.fd < 0 );
		} else if ( type == PipeInput ) {
			input->ptr.file = popen( identifier, "r" );
			failed = ( input->ptr.file == NULL );
		} else {
			input->ptr.fd = open( identifier, O_RDONLY );
			failed = ( input->ptr.file < 0 );
		}
		if ( failed ) {
			free( input );
			return output( Error, "could not open stream: \"%s\"", identifier );
		}
		context->hcn.state = base;
		break;
	case ClientInput:
		input->client = *(int *) src;
		input->ptr.string = context->io.input.buffer.ptr;
		input->ptr.string[ 0 ] = (char) 0;
		input->remainder = 0;
		input->position = NULL;
		break;
	case InstructionBlock:
	case InstructionOne:
	case LastInstruction:
		input->position = NULL;
		switch ( type ) {
			case InstructionBlock:
				context->record.level = context->control.level;
				listItem *first_instruction = (listItem *) src;
				context->input.instruction = first_instruction;
				input->ptr.string = first_instruction->ptr;
				break;
			case InstructionOne:
				input->ptr.string = src;
				break;
			case LastInstruction:
				; listItem *last_instruction = context->record.instructions;
				context->input.instruction = last_instruction;
				context->record.instructions = last_instruction->next;
				input->ptr.string = last_instruction->ptr;
				break;
			default: break;
		}
		break;
	default:
		break;
	}

	// make stream current
	addItem( &context->input.stack, input );

	// if we were in a for loop, reset control mode to record subsequent
	// instructions from stream
	if ( context->input.stack->next != NULL ) {
		input = (InputVA *) context->input.stack->next->ptr;
		if ( input->mode == LastInstruction ) {
			set_control_mode( InstructionMode, 0, context );
		}
	}

	// no prompt in these modes
	context->control.prompt = 0;
	return 0;
}

/*---------------------------------------------------------------------------
	pop_input
---------------------------------------------------------------------------*/
int
pop_input( char *state, int event, char **next_state, _context *context )
{
	InputVA *input = (InputVA *) context->input.stack->ptr;
	int delta = context->control.level - input->level;
	if ( delta > 0 ) {
		event = output( Error, "reached premature EOF - restoring original stack level" );
		while ( delta-- ) pop( state, event, next_state, context );
	}
	else event = 0;

	context->control.prompt = input->restore.prompt;
	switch ( input->mode ) {
	case PipeInput:
	case StreamInput:
		free( input->identifier );
		break;
	default:
		break;
	}
	free( input );
	popListItem( &context->input.stack );

	if ( context->input.stack != NULL ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( input->mode == LastInstruction ) {
			set_control_mode( ExecutionMode, 0, context );
		}
	}
	return event;
}

/*---------------------------------------------------------------------------
	set_input_mode
---------------------------------------------------------------------------*/
void
set_input_mode( RecordMode mode, int event, _context *context )
{
	switch ( mode ) {
	case OffRecordMode:
		if ( context->record.mode == OnRecordMode )
		{
			// remove the last event from the record
			popListItem( &context->record.string.list );
			string_finish( &context->record.string, 1 );
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
                context->record.level = context->control.level;
		context->record.mode = mode;
		break;
	}
}

/*---------------------------------------------------------------------------
	input
---------------------------------------------------------------------------*/
int
skip_comment( int event, int *comment_on, int *backslash, _context *context )
{
	if ( context->input.buffer ) {
		context->input.buffer = 0;
	}
	else if ( event == '\n' ) {
		// overrules everything, backslash and all
		/*
		// not needed because these are local,
		// but keep this for clarity
		*comment_on = 0;
		*backslash = 0;
		*/
	}
	else if ( *comment_on ) {
		// ignore everything until '\n'
		event = 0;
	}
	else if ( *backslash )
	{
		// ignore '#' and '\' if backslashed
		context->input.buffer = event;
		event = '\\';
		*backslash = 0;
	}
	else if ( event == '\\' )
	{
		*backslash = 1;
		event = 0;
	}
	else if ( event == '#' ) {
		*comment_on = 1;
		event = 0;
	}
	return event;
}

int
input( char *state, int event, char **next_state, _context *context )
{
	if ( event != 0 ) {
		return event;
	}

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	int comment_on = 0, backslash = 0;
	do {
		if ( context->input.buffer )
		{
			event = context->input.buffer;
		}
		else if ( context->input.stack == NULL )
		{
			if ( !strcmp( state, base ) ) prompt( context );
			read( STDIN_FILENO, &event, 1 );
			if ( event == '\n' ) context->control.prompt = context->control.terminal;
		}
		else
		{
			InputVA *input = (InputVA *) context->input.stack->ptr;
			int do_pop = 1;
			if ( input->corrupted )
				; // user tried to pop control stack beyond input level
			else switch ( input->mode ) {
			case HCNFileInput:
				event = hcn_getc( input->ptr.fd, context );
				do_pop = !event;
				break;
			case PipeInput:
				event = fgetc( input->ptr.file );
				do_pop = ( event == EOF );
				break;
			case StreamInput:
				; char buf;
				do_pop = !read( input->ptr.fd, &buf, 1 );
				event = buf;
				break;
			case ClientInput:
			case InstructionOne:
			case InstructionBlock:
			case LastInstruction:
				if ( input->position == NULL ) {
					input->position = input->ptr.string;
#ifdef DEBUG
					output( Debug, "input: reading from \"%s\"", (char *) input->position );
#endif
					if ( context->narrative.mode.output && ( input->mode == InstructionBlock ))
						for ( int i=0; i<=context->control.level; i++ )
							output( Text, "", "\t" );
				}
				event = (int ) (( char *) input->position++ )[ 0 ];
				if ( event && ( context->narrative.mode.output )) {
					output( Text, "%c", event );
				}
				do_pop = ( event == 0 );
				break;
			default:
				break;
			}
			if ( do_pop ) {
				event = 0;
				switch ( input->mode ) {
				case HCNFileInput:
					close ( input->ptr.fd );
					break;
				case PipeInput:
					pclose( input->ptr.file );
					break;
				case StreamInput:
					close( input->ptr.fd );
					break;
				case ClientInput:
					input->position = NULL;
					do_pop = !io_read( input->client, input->ptr.string, &input->remainder );
					if ( do_pop ) {
						input->corrupted = 0;
						pop_input( state, event, next_state, context );
						return 0;
					}
					break;
				case InstructionBlock:
					context->input.instruction = context->input.instruction->next;
					if ( context->input.instruction != NULL ) {
						input->ptr.string = context->input.instruction->ptr;
						input->position = NULL;
					}
					do_pop = 0;
					break;
				case InstructionOne:
					// input->position--;
					return '\n';
				case LastInstruction:
	        			free( context->input.instruction->ptr );
				       	freeItem( context->input.instruction );
				        context->input.instruction = NULL;
					set_control_mode( InstructionMode, event, context );
					break;
				default:
					break;
				}

				if ( do_pop ) {
					input->corrupted = 0;
					event = pop_input( state, event, next_state, context );
				}
			}
		}
		event = skip_comment( event, &comment_on, &backslash, context );

	}
	while ( event == 0 );

	context->input.event = event;

	// record instruction or expression if needed

	if ( event > 0 ) switch( context->record.mode ) {
	case RecordInstructionMode:
		if ( context->record.string.list == NULL ) {
			string_start( &context->record.string, event );
		}
		else {
			string_append( &context->record.string, event );
		}
		if ( event == '\n' ) {
			char *string = string_finish( &context->record.string, 1 );
			if ( strcmp((char *) context->record.string.ptr, "\n" ) ) {
				addItem( &context->record.instructions, string );
			} else {
				free ( context->record.string.ptr );
			}
#ifdef DEBUG
			output( Debug, "input read \"%s\"", context->record.string.ptr );
#endif
			context->record.string.ptr = NULL;
		}
		break;
	case OnRecordMode:
		string_append( &context->record.string, event );
		break;
	case OffRecordMode:
		break;
	}

	return event;
}

