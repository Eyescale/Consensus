#define KERNEL_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "input.h"
#include "output.h"
#include "variables.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	context_check	- utility
---------------------------------------------------------------------------*/
int
context_check( int freeze, int instruct, int execute )
{
	int mode = CN.context->control.mode;
	switch ( mode ) {
	case FreezeMode:
		return freeze;
	case InstructionMode:
		return instruct;
	case ExecutionMode:
		return execute;
	}
	return 0;
}

/*---------------------------------------------------------------------------
	set_control_mode	- utility
---------------------------------------------------------------------------*/
void
set_control_mode( ControlMode mode, int event, _context *context )
{
	switch ( mode ) {
	case FreezeMode:
		if ( context->input.stack == NULL ) {
			output( Warning, "switching to passive mode - type in '/~' to alternate condition" );
		}
		context->freeze.level = context->control.level;
		break;
	case InstructionMode:
		set_input_mode( RecordInstructionMode, event, context );
		break;
	case ExecutionMode:
		if (( context->control.mode == FreezeMode ) && ( context->input.stack == NULL )) {
			output( Warning, "back to active mode" );
		}
		set_input_mode( OffRecordMode, event, context );
		break;
	}
	context->control.mode = mode;
}

/*---------------------------------------------------------------------------
	nothing, nop	- actions
---------------------------------------------------------------------------*/
// the nothing action passes everything through
// it is used to change state and enter with the same token
int
nothing( char *state, int event, char **next_state, _context *context )
{
	return event;
}

// the nop action causes new input to be read
// it is used to change state and enter with a new token
int
nop( char *state, int event, char **next_state, _context *context )
{
	return 0;
}

/*---------------------------------------------------------------------------
	error	- action
---------------------------------------------------------------------------*/
int
error( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, InstructionMode, ExecutionMode ) )
		return 0;

	if ( event == '\n' ) {
		return output( Error, "in state \"%s\", instruction incomplete", state );
	} else if ( event != 0 ) {
		return output( Error, "in \"%s\", on '%c', syntax error", state, event );
	}
	return event;
}

/*---------------------------------------------------------------------------
	warning	- action
---------------------------------------------------------------------------*/
int
warning( char *state, int event, char **next_state, _context *context )
{
	if ( !context_check( 0, InstructionMode, ExecutionMode ) )
		return 0;

	if ( event == '\n' ) {
		output( Warning, "\"%s\", instruction incomplete", state );
	} else if ( event != 0 ) {
		output( Warning, "syntax error: in \"%s\", on '%c'", state, event );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	push	- action
---------------------------------------------------------------------------*/
int
push( char *state, int event, char **next_state, _context *context )
{
	if ( context_check( FreezeMode, 0, 0 ) ) {
		context->control.level++;
		return 0;
	}
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	if ( next_state != NULL ) {
		stack->next_state = strcmp( *next_state, same ) ? *next_state : state;
	}
	context->control.level++;

	stack = (StackVA *) calloc( 1, sizeof(StackVA) );
	addItem( &context->control.stack, stack );

#ifdef DEBUG
	output( Debug, "Consensus: push: from state=\"%s\" to state=\"%s\"", state, stack->next_state );
#endif
	return 0;
}

/*---------------------------------------------------------------------------
	pop	- action
---------------------------------------------------------------------------*/
int
pop( char *state, int event, char **next_state, _context *context )
{
#ifdef DEBUG
	output( Debug, "kernel pop: from level=%d, state=\"%s\"", context->control.level, state );
#endif
	if ( context_check( FreezeMode, 0, 0 ) ) {
		if ( context->control.level == context->freeze.level ) {
			set_control_mode( ExecutionMode, event, context );
		} else {
			context->control.level--;
			return 0;
		}
	}
	if ( context->input.stack != NULL ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( context->control.level == input->level ) {
			input->state.pop = 1;
			return output( Error, "attempt to pop control beyond authorized level - "
				"closing stream..." );
		}
	}

	// free current level's variables
	StackVA *stack = (StackVA *) context->control.stack->ptr;
	freeVariables( &stack->variables );

	if ( context->control.level ) {
		free( stack );
		popListItem( &context->control.stack );
		stack = (StackVA *) context->control.stack->ptr;
		if ( next_state != NULL ) {
			*next_state  = stack->next_state;
		}
		context->control.level--;
	} else {
		if ( next_state != NULL )
			*next_state = "";
	}

#ifdef DEBUG
	output( Debug, "kernel pop: to state=\"%s\"", state );
#endif
	return 0;
}

