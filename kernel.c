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
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	ttyu	- utility
---------------------------------------------------------------------------*/
int
ttyu( _context *context )
{
	return ( context->control.terminal &&
		!context->control.cgi &&
		// !context->control.cgim &&
		!context->control.session );
}

/*---------------------------------------------------------------------------
	command_mode	- utility
---------------------------------------------------------------------------*/
int
command_mode( int freeze, int instruct, int execute )
{
	int mode = CN.context->command.mode;
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
	set_command_mode	- utility
---------------------------------------------------------------------------*/
void
set_command_mode( CommandMode mode, _context *context )
{
	switch ( mode ) {
	case FreezeMode:
		if ( context->input.stack == NULL ) {
			output( Warning, "switching to passive mode - type in '/~' to alternate condition" );
		}
		context->command.freeze.level = context->command.level;
		break;
	case InstructionMode:
		break;
	case ExecutionMode:
		if (( context->command.mode == FreezeMode ) && ( context->input.stack == NULL )) {
			output( Warning, "back to active mode" );
		}
		break;
	}
	context->command.mode = mode;
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
	push	- action
---------------------------------------------------------------------------*/
int
push( char *state, int event, char **next_state, _context *context )
{
	if ( command_mode( FreezeMode, 0, 0 ) ) {
		context->command.level++;
		return 0;
	}
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	stack->next_state = strcmp( *next_state, same ) ? *next_state : state;
	context->command.level++;

	stack = (StackVA *) calloc( 1, sizeof(StackVA) );
	RTYPE( &stack->variables ) = IndexedByName;
	addItem( &context->command.stack, stack );

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
	output( Debug, "kernel pop: from level=%d, state=\"%s\"", context->command.level, state );
#endif
	if ( command_mode( FreezeMode, 0, 0 ) ) {
		if ( context->command.level == context->command.freeze.level ) {
			set_command_mode( ExecutionMode, context );
		} else {
			context->command.level--;
			return 0;
		}
	}

	if ( context->input.stack != NULL ) {
		InputVA *input = (InputVA *) context->input.stack->ptr;
		if ( !strcmp( input->identifier, "" ) )
			;
		else if ( context->command.level == input->level ) {
			input->shed = 1;
			output( Warning, "attempt to pop control beyond authorized level - closing stream..." );
			return 0;
		}
	}

	// free current level's variables
	StackVA *stack = (StackVA *) context->command.stack->ptr;
	freeVariables( &stack->variables );

	if ( context->command.level ) {
		free( stack );
		popListItem( &context->command.stack );
		stack = (StackVA *) context->command.stack->ptr;
		if ( next_state != &same ) {
			*next_state  = stack->next_state;
		}
		context->command.level--;
	} else {
		if ( next_state != &same )
			*next_state = "";
	}

#ifdef DEBUG
	output( Debug, "kernel pop: to state=\"%s\"", state );
#endif
	return 0;
}

