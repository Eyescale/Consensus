#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "string_util.h"

#include "input.h"
#include "hcn.h"
#include "output.h"

// #define DEBUG
#define BUG

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
		StreamVA *input = (StreamVA *) context->input.stack->ptr;
		switch ( input->type ) {
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
	StreamVA *input = context->input.stack->ptr;
	input->ptr.string = instruction->ptr;
	input->position = NULL;
}

/*---------------------------------------------------------------------------
	push_input
---------------------------------------------------------------------------*/
int
push_input( char *identifier, void *src, InputType type, _context *context )
{
	StreamVA *input;
#ifdef DEBUG
	output( Debug, "push_input: \"%s\"", identifier );
#endif
	switch ( type ) {
	case PipeInput:
	case HCNFileInput:
		// check if stream is already in use
		; registryEntry *stream = lookupByName( context->input.stream, identifier );
		if ( stream != NULL ) {
			int event = output( Error, "recursion in stream: \"%s\"", identifier );
			free( identifier );
			return event;
		}
		break;
	default:
		break;
	}

	// create and register new active stream
	input = (StreamVA *) calloc( 1, sizeof(StreamVA) );
	input->level = context->control.level;
	input->type = type;

	switch ( type ) {
	case HCNFileInput:
	case PipeInput:
#ifdef DEBUG
		output( Debug, "opening stream: \"%s\"", identifier );
#endif
		input->ptr.file = ( type == PipeInput ) ?
			popen( identifier, "r" ) :
			fopen( identifier, "r " );
		if ( input->ptr.file == NULL ) {
			free( input );
			return output( Error, "could not open stream: \"%s\"", identifier );
		}
		context->hcn.state = base;
		registerByName( &context->input.stream, identifier, input );
		break;
	case ClientInput:
		input->position = NULL;
		input->ptr.string = context->io.buffer.ptr[ 0 ];
		input->ptr.string[ 0 ] = (char) 0;
		input->remainder = 0;
		int length;
		read( context->io.client, &length, sizeof(length) );
		if ( !length ) {
			break;
		} else if ( length > IO_BUFFER_MAX_SIZE ) {
			int n = IO_BUFFER_MAX_SIZE - 1;
			input->remainder = length - n;
			input->ptr.string[ n ] = (char) 0;
			length = n;
		}
		read( context->io.client, input->ptr.string, length );
		break;
	case InstructionBlock:
	case InstructionOne:
	case LastInstruction:
		input->position = NULL;
		if ( context->narrative.mode.action.block ) {
			input->level--;	// pop_input() will be called from outside in this case
		}
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
		input = (StreamVA *) context->input.stack->next->ptr;
		if ( input->type == LastInstruction ) {
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
	StreamVA *input = (StreamVA *) context->input.stack->ptr;
	int delta = context->control.level - input->level;
	if ( delta > 0 ) {
		event = output( Error, "reached premature EOF - restoring original stack level" );
		while ( delta-- ) pop( state, event, next_state, context );
	}
	else event = 0;

	free( input );
	popListItem( &context->input.stack );

	if ( context->input.stack == NULL ) {
		if ( !context->io.client ) {
			context->control.prompt = context->control.terminal;
		}
	} else {
		StreamVA *input = (StreamVA *) context->input.stack->ptr;
		if ( input->type == LastInstruction ) {
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
			string_finish( &context->record.string );
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
input( char *state, int event, char **next_state, _context *context )
{
	if ( event != 0 ) {
		return event;
	}

	StackVA *stack = (StackVA *) context->control.stack->ptr;
	do {
		if ( context->input.stack == NULL )
		{
			if ( !strcmp( state, base ) ) prompt( context );
			read( STDIN_FILENO, &event, 1 );
			if ( event == '\n' ) context->control.prompt = context->control.terminal;
		}
		else
		{
			StreamVA *input = (StreamVA *) context->input.stack->ptr;
			int do_pop = 1;
			if ( !input->mode.pop ) switch ( input->type ) {
			case HCNFileInput:
				event = hcn_getc( input->ptr.file, context );
				do_pop = ( event == EOF );
				break;
			case PipeInput:
				event = fgetc( input->ptr.file );
				do_pop = ( event == EOF );
				break;
			case ClientInput:
			case InstructionBlock:
			case InstructionOne:
			case LastInstruction:
				if ( input->position == NULL ) {
					input->position = input->ptr.string;
#ifdef DEBUG
					output( Debug, "input: reading from \"%s\"", (char *) input->position );
#endif
					switch ( input->type ) {
					case InstructionBlock:
						if ( !context->narrative.mode.output ) break;
						for ( int i=0; i<=context->control.level; i++ )
							output( Text, "\t" );
						break;
					default:
						break;
					}
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
				switch ( input->type ) {
				case HCNFileInput:
					fclose ( input->ptr.file );
					deregisterByValue( &context->input.stream, input );
					break;
				case PipeInput:
					pclose( input->ptr.file );
					registryEntry *entry = lookupByValue( context->input.stream, input );
					free( entry->identifier );
					deregisterByValue( &context->input.stream, input );
					break;
				case ClientInput:
					; int length = input->remainder;
					if ( !length ) {
						read( context->io.client, &length, sizeof(length) );
						if ( !length ) break;	// EOF
					}
					do_pop = 0;
					input->position = NULL;
					if ( length > IO_BUFFER_MAX_SIZE ) {
						int n = IO_BUFFER_MAX_SIZE - 1;
						input->remainder = length - n;
						length = n;
					} else {
						input->remainder = 0;
					}
					read( context->io.client, input->ptr.string, length );
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
					input->position--;
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
					input->mode.pop = 0;
					event = pop_input( state, event, next_state, context );
					if ( input->type == ClientInput ) return 0;
				}
			}
		}
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
			char *string = string_finish( &context->record.string );
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

/*---------------------------------------------------------------------------
	read_identifier, read_argument, read_va_identifier
---------------------------------------------------------------------------*/
#define read_do_( a, s ) \
	event = read_execute( a, &state, event, s, context );

static int
read_execute( _action action, char **state, int event, char *next_state, _context *context )
{
	event = action( *state, event, &next_state, context );
	if ( strcmp( next_state, same ) )
		*state = next_state;
	return event;
}

static int
identifier_start( char *state, int event, char **next_state, _context *context )
{
	free( context->identifier.id[ context->identifier.current ].ptr );
	return string_start( &context->identifier.id[ context->identifier.current ], event );
}

static int
identifier_append( char *state, int event, char **next_state, _context *context )
{
	return string_append( &context->identifier.id[ context->identifier.current ], event );
}

static int
identifier_finish( char *state, int event, char **next_state, _context *context )
{
	string_finish( &context->identifier.id[ context->identifier.current ] );
	return event;
}

int
read_identifier( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, InstructionMode, ExecutionMode ) ) {
		read_do_( flush_input, same );
		return event;
	}

	state = base;
	do {
		event = input( state, event, NULL, context );
		bgn_
		in_( base ) bgn_
			on_( '\"' )	read_do_( identifier_start, "string" )
			on_separator	read_do_( error, "" )
			on_other	read_do_( identifier_start, "identifier" )
			end
		in_( "string" ) bgn_
			on_( '\"' )	read_do_( identifier_append, "string_" )
			on_other	read_do_( identifier_append, same )
			end
		in_( "string_" ) bgn_
			on_any		read_do_( identifier_finish, "" )
			end
		in_( "identifier" ) bgn_
			on_separator	read_do_( identifier_finish, "" )
			on_other	read_do_( identifier_append, same )
			end
		end
	}
	while ( strcmp( state, "" ) );

	return event;
}

int
read_argument( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 1;
	event = read_identifier( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

int
read_va_identifier( char *state, int event, char **next_state, _context *context )
{
	context->identifier.current = 2;
	event = read_identifier( state, event, next_state, context );
	context->identifier.current = 0;
	return event;
}

