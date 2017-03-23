#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "string_util.h"

#include "input.h"
#include "hcn.h"
#include "output.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	flush_input
---------------------------------------------------------------------------*/
/*
	flush_input is called upon error from e.g. read_command and read_narrative.
	input (whether StreamInput or StringInput ) is assumed to be '\n'-terminated
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
		StreamVA *stream = (StreamVA *) context->input.stack->ptr;
		do_flush = ( stream->type != StringInput );
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
		log_error( context, 0, "aborting loop..." );
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
	set_input
---------------------------------------------------------------------------*/
void
set_input( listItem *src, _context *context )
{
	context->input.instruction = src;
	StreamVA *input = context->input.stack->ptr;
	input->ptr.string = context->input.instruction->ptr;
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
	fprintf( stderr, "debug> push_input: \"%s\"\n", identifier );
#endif
	switch ( type ) {
	case PipeInput:
	case HCNFileInput:
		// check if stream is already in use
		; registryEntry *stream = lookupByName( context->input.stream, identifier );
		if ( stream != NULL ) {
			char *msg; asprintf( &msg, "recursion in stream: \"%s\"", identifier );
			int event = log_error( context, event, msg ); free( msg );
			free( identifier );
			return event;
		}
#ifdef DEBUG
		fprintf( stderr, "opening stream: \"%s\"\n", identifier );
#endif
		// create and register new active stream
		input = (StreamVA *) calloc( 1, sizeof(StreamVA) );
		input->level = context->control.level;
		input->type = type;
		input->ptr.file = ( type == PipeInput ) ?
			popen( identifier, "r" ) :
			fopen( identifier, "r " );
		if ( input->ptr.file == NULL ) {
			free( input );
			char *msg; asprintf( &msg, "could not open stream: \"%s\"", identifier );
			int event = log_error( context, event, msg ); free( msg );
			return event;
		}
		context->hcn.state = base;
		registerByName( &context->input.stream, identifier, input );
		break;
	default:
		if ( identifier != NULL ) {
			// check if variable already in use - embedded strings
			registryEntry *entry = lookupByName( context->input.string, identifier );
			if ( entry != NULL ) {
				char *msg; asprintf( &msg, "recursion in input variable: %%%s", identifier );
				int event = log_error( context, 0, msg ); free( msg );
				return event;
			}
		}
		// create and register new active stream
		input = (StreamVA *) calloc( 1, sizeof(StreamVA) );
		input->level = context->control.level;
		if ( context->narrative.mode.action.block ) {
			input->level--;	// pop_input() will be called from outside in this case
		}
		input->type = StringInput;
		input->position = NULL;
		switch ( type ) {
			case InstructionBlock:
				input->mode.instructions = 1;
				context->record.level = context->control.level;
				listItem *first_instruction = (listItem *) src;
				context->input.instruction = first_instruction;
				input->ptr.string = first_instruction->ptr;
				break;
			case LastInstruction:
				input->mode.escape = 1;
				listItem *last_instruction = context->record.instructions;
				context->input.instruction = last_instruction;
				context->record.instructions = last_instruction->next;
				input->ptr.string = last_instruction->ptr;
				break;
			case APIStringInput:
				input->mode.api = 1;
				input->ptr.string = src;
				break;
			default: break;
		}
		if ( identifier != NULL ) {
			input->mode.variable = 1;
			registerByName( &context->input.string, identifier, input );
		}
	}

	// make stream current
	addItem( &context->input.stack, input );

	// if we were in a for loop, reset control mode to record subsequent
	// instructions from stream
	if ( context->input.stack->next != NULL ) {
		input = (StreamVA *) context->input.stack->next->ptr;
		if ( input->mode.escape ) {
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
		event = log_error( context, 0, "reached premature EOF - restoring original stack level" );
		while ( delta-- ) pop( state, event, next_state, context );
	}
	else event = 0;

	free( input );
	popListItem( &context->input.stack );

	if ( context->input.stack == NULL ) {
		context->control.prompt = 1;
	} else {
		StreamVA *input = (StreamVA *) context->input.stack->ptr;
		if ( input->mode.escape ) {
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
			event = getchar( );
			if ( event == '\n' ) context->control.prompt = 1;
		}
		else
		{
			StreamVA *stream = (StreamVA *) context->input.stack->ptr;
			int do_pop = 1;
			if ( !stream->mode.pop ) switch ( stream->type ) {
			case StringInput:
				if ( stream->position == NULL ) {
					if ( stream->mode.instructions && context->narrative.mode.output ) {
						for ( int i=0; i<=context->control.level; i++ )
							printf( "\t" );
					}
					stream->position = stream->ptr.string;
#ifdef DEBUG
					fprintf( stderr, "debug> input: reading from \"%s\"\n", (char *) stream->position );
#endif
				}
				event = (int ) (( char *) stream->position++ )[ 0 ];
				if ( event && ( context->narrative.mode.output )) {
					printf( "%c", event );
				}
				do_pop = ( event == 0 );
				break;
			case StreamInput:
				// DO_LATER
				// special case: stream == stdin: :<< (??)
				// would allow to reenter interactive mode even during
				// pipe or hcn file input read
				// must make sure we don't stack it up upon itself
				break;
			case HCNFileInput:
				event = hcn_getc( stream->ptr.file, context );
				do_pop = ( event == EOF );
				break;
			case PipeInput:
				event = fgetc( stream->ptr.file );
				do_pop = ( event == EOF );
				break;
			default:
				break;
			}
			if ( do_pop ) {
				event = 0;
				switch ( stream->type ) {
				case StringInput:
					if ( stream->mode.escape ) {
	        				free( context->input.instruction->ptr );
				        	freeItem( context->input.instruction );
					        context->input.instruction = NULL;
						set_control_mode( InstructionMode, event, context );
					}
					else if ( stream->mode.instructions ) {
						context->input.instruction = context->input.instruction->next;
						if ( context->input.instruction != NULL ) {
							stream->ptr.string = context->input.instruction->ptr;
							stream->position = NULL;
						}
						do_pop = 0;
					}
					else if ( stream->mode.api ) {
						stream->position--;
						return '\n';
					}
					else if ( stream->mode.variable ) {
						registryEntry *entry = lookupByValue( context->input.string, stream );
						if ( entry != NULL ) free( entry->identifier );
						free( stream->ptr.string );
					}
					if ( stream->mode.variable ) {
						deregisterByValue( &context->input.stream, stream );
					}
					break;
				case StreamInput:
					// DO_LATER
					break;
				case HCNFileInput:
					fclose ( stream->ptr.file );
					deregisterByValue( &context->input.stream, stream );
					break;
				case PipeInput:
					pclose( stream->ptr.file );
					registryEntry *entry = lookupByValue( context->input.stream, stream );
					free( entry->identifier );
					deregisterByValue( &context->input.stream, stream );
					break;
				default:
					break;
				}

				if ( do_pop ) {
					stream->mode.pop = 0;
					event = pop_input( state, event, next_state, context );
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
			fprintf( stderr, "debug> input read \"%s\"\n", context->record.string.ptr );
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

