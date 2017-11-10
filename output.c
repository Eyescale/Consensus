#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "input.h"
#include "input_util.h"
#include "expression_util.h"
#include "command.h"
#include "output.h"
#include "output_util.h"
#include "path.h"
#include "io.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	error
---------------------------------------------------------------------------*/
int
error( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) )
		return 0;

	if ( context->error.tell ) {
		if ( event ) switch ( context->error.tell ) {
		case 1:
			outputf( Error, "in \"%s\", on '%c', syntax error", state, event );
			break;
		case 2:
			outputf( Error, "in state \"%s\", instruction incomplete", state );
			break;
		}
		context->error.tell = 0;
	}
	flush_input( base, 0, &same, context );

	return -1;
}

/*---------------------------------------------------------------------------
	warning
---------------------------------------------------------------------------*/
int
warning( char *state, int event, char **next_state, _context *context )
{
	if ( !command_mode( 0, InstructionMode, ExecutionMode ) )
		return 0;

	if ( event == '\n' ) {
		outputf( Warning, "\"%s\", instruction incomplete", state );
	} else if ( event != 0 ) {
		outputf( Warning, "syntax error: in \"%s\", on '%c'", state, event );
	}
	return 0;
}

/*---------------------------------------------------------------------------
	anteprompt
---------------------------------------------------------------------------*/
void
anteprompt( _context *context )
{
	if ( ttyu( context ) && context->output.anteprompt && (context->input.stack))
	{
		fprintf( stderr, "\n" );
		context->output.anteprompt = 0;
	}
}

/*---------------------------------------------------------------------------
	prompt
---------------------------------------------------------------------------*/
void
prompt( _context *context )
{
	if ( !context->output.prompt )
		return;

	context->output.prompt = 0;
	context->output.anteprompt = 1;

	StackVA *stack = (StackVA *) context->command.stack->ptr;
	switch ( context->command.mode ) {
	case FreezeMode:
		fprintf( stderr, "(/~) ?_ " );
		break;
	case InstructionMode:
		fprintf( stderr, "directive$ " );
		break;
	case ExecutionMode:
		if ( context->narrative.current ) {
			StackVA *stack = (StackVA *) context->command.stack->ptr;
			if ( stack->narrative.state.closure )
				fprintf( stderr, "(then/.)?_ " );
			else
				fprintf( stderr, "in/on/do_$ " );
		}
		else {
			fprintf( stderr, 
				context->control.operator ? "operator$ " :
				context->control.cgim ? "cgi$ " :
				"consensus$ " );
		}
		break;
	}
	for ( int i=0; i < context->command.level; i++ )
		fprintf( stderr, "\t" );
}

/*---------------------------------------------------------------------------
	push_output
---------------------------------------------------------------------------*/
int
push_output( char *identifier, void *dst, OutputType type, _context *context )
{
	OutputVA *output = (OutputVA *) calloc( 1, sizeof(OutputVA) );
	output->level = context->command.level;
	output->restore.redirected = context->output.redirected;
	output->restore.query = context->output.query;
	output->mode = type;
	switch ( type ) {
	case SessionOutput:
		output->ptr.socket = io_query((char *) dst, context );
		if ( output->ptr.socket <= 0 ) {
			free( output );
			return -1;
		}
		context->output.query = 1;
		context->output.redirected = 1;
		break;
	case ClientOutput:
		output->ptr.socket = *(int *) dst;
		context->output.redirected = 1;
		break;
	case StringOutput:
		output->variable.identifier = dst;
		for ( listItem *i=context->output.slist; i!=NULL; i=i->next )
			free( i->ptr );
		freeListItem( &context->output.slist );
		context->output.redirected = 1;
		break;
	}
	addItem( &context->output.stack, output );
	return 0;
}

/*---------------------------------------------------------------------------
	pop_output
---------------------------------------------------------------------------*/
int
pop_output( _context *context, int eot )
{
	OutputVA *output = (OutputVA *) context->output.stack->ptr;
	context->output.redirected = output->restore.redirected;
	context->output.query = output->restore.query;
	switch ( output->mode ) {
	case SessionOutput:
		io_flush( output->ptr.socket, &output->remainder, eot, context );
		break;
	case ClientOutput:
		io_flush( output->ptr.socket, &output->remainder, eot, context );
		break;
	case StringOutput:
		slist_close( &context->output.slist, &context->output.string, 0 );
		reorderListItem( &context->output.slist );
		char *identifier = output->variable.identifier;
		if (( context->output.slist )) {
			listItem *i = context->output.slist;
#ifdef EXPAND
			if ( i->next == NULL ) {
				i = string_expand( i->ptr, NULL );
				assign_variable( &identifier, i, StringVariable, context );
			} else
#endif
			{
				assign_variable( &identifier, i, StringVariable, context );
				context->output.slist = NULL;
			}
		} else {
			free( identifier );
		}
		output->variable.identifier = NULL;
		break;
	}
	free( output );
	popListItem( &context->output.stack );
	return 0;
}

/*---------------------------------------------------------------------------
	outputf
---------------------------------------------------------------------------*/
static char *header_list[] =	// ordered as they appear below
{
	"***** Error: ",
	"Error: ",
	"Warning: ",
	"consensus: ",
	"debug> "
};

int
outputf( OutputContentsType type, const char *format, ... )
{
	_context *context = CN.context;
	int retval = (( type == Error ) || ( type == Debug ) ? -1 : 0 );
	if ( format == NULL ) return retval;	// e.g. outputf( Error, NULL );

	if ( type == Error ) {
		flush_input( base, 0, &same, context );
		if ( !context->error.tell )
			return retval;
		context->error.tell = 0;
	}

	va_list ap;
	va_start( ap, format );
	if ( type == Text )
	{
		if ( context->output.stack == NULL )
		{
			if ( !context->control.session ) {
				anteprompt( context );
				vfprintf( stdout, ( *format ? format : "%s" ), ap );
			}
		}
		else
		{
			OutputVA *output = context->output.stack->ptr;
			char *str;
			if ( *format ) {
				vasprintf( &str, format, ap );
			} else {
				str = va_arg( ap, char * );
			}
			if ( !strcmp( str, "" ) ) {
				va_end( ap );
				free( str );
				return 0;
			}
			switch ( output->mode ) {
			case SessionOutput:
			case ClientOutput:
				io_write( output->ptr.socket, str, strlen( str ), &output->remainder );
				break;
			case StringOutput:
				; listItem **slist = &context->output.slist;
				IdentifierVA *identifier = &context->output.string;
				for ( char *ptr = str; *ptr; ptr++ )
					slist_append( slist, identifier, *ptr, 1, 0 );
				break;
			}
			if ( *format ) free( str );
		}
	}
	else if ( ttyu( context ) || context->control.terminal )
	{
		FILE *stream = stderr;
		char *header;
		switch ( type ) {
		case Error:
			if ( context->error.flush_output ) {
				// output in the text
				stream = stdout;
				context->error.flush_output = 0;
				header = header_list[ 0 ];
			} else {
				header = header_list[ 1 ];
			}
			break;
		case Warning:	header = header_list[ 2 ]; break;
		case Info:	header = header_list[ 3 ]; break;
		case Debug:	header = header_list[ 4 ]; break;
		default:
			break;
		}

		anteprompt( context );
		if ( type == Enquiry ) {
			vfprintf( stream, ( *format ? format : "%s" ), ap );
		} else {
			fprintf( stream, "%s", header );
			vfprintf( stream, ( *format ? format : "%s" ), ap );
			fprintf( stream, "\n" );
		}
	}
	va_end( ap );
	return retval;
}

/*---------------------------------------------------------------------------
	output_
---------------------------------------------------------------------------*/
enum {
	Modulo = 1,
	Char,
	SpecialChar,
	VariableValue,
	VariatorValue,
	SchemeDescriptor,
	SchemeValueAccount,
	ExpressionResults,
	ExpressionValueAccount,
	NarrativeBody
};

static int
output_( int type, int event, _context *context )
{
	if ( context->output.html )
		return output( Error, ".$( html )*****trailing characters" );

	char *va_name = context->identifier.id[ 2 ].ptr;
	if (( type == ExpressionValueAccount ) && !strcmp( va_name, "html" ))
		// so that clear_output_target() can close Client connection
		context->output.html = 1;

	int retval;
	switch ( type ) {
	case Char:
		retval = (( event == '\n' ) ? '\n' : 0 );
		break;
	case SpecialChar:
		retval = (( event == 'n' ) ? '\n' : 0 );
		break;
	default:
		retval = event;
	}
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return retval;

	VariableVA *variable;
	char *scheme, *path;
	context->error.flush_output = ( event != '\n' );
	int marked = 0;

	switch ( type ) {
	case Modulo:
		output( Text, "%" );
		marked = 1;
		break;

	case Char:
		switch ( event ) {
		case ' ':
		case '\t':
		case '\n':
			if ( !context->output.marked ) break;
		default:
			outputf( Text, "%c", event );
			marked = 1;
		}
		break;

	case SpecialChar:
		switch ( event ) {
		case 'n': output( Text, "\n" ); break;
		case 't': output( Text, "\t" ); break;
		case '\0': break;
		default:
			outputf( Text, "%c", event );
		}
		marked = ( event && ( event != 'n' ));
		break;

	case VariatorValue:
#ifdef DEBUG
		outputf( Debug, "output_variator_value: %%%s", variator_symbol );
#endif
		variable = lookupVariable( context, variator_symbol, 0 );
		if ( variable == NULL ) return retval;
		marked = output_variable_value_( variable );
		break;

	case VariableValue:
#ifdef DEBUG
		outputf( Debug, "output_variable_value: %%%s", context->identifier.id[ 1 ].ptr );
#endif
		path = context->identifier.id[ 1 ].ptr;
		context->identifier.id[ 1 ].ptr = NULL;
		variable = lookupVariable( context, path, 0 );
		free( path );
		if ( variable == NULL ) return retval;
		marked = output_variable_value_( variable );
		break;

	case SchemeDescriptor:
		scheme = context->identifier.scheme;
		path = context->identifier.path;
		if ( strcmp( scheme, "session" ) )
			return outputf( Error, ">: %%[ %s://%s ] - operation not supported", scheme, path );
		if ( !context->control.operator )
			return outputf( Error, "misdirected: >: %%[ session:%s ] - not operator", path  );
		registryEntry *entry = registryLookup( context->operator.sessions, path );
		if ( entry == NULL ) return retval;
		SessionVA *session = entry->value;
		outputf( Text, "%d", session->pid );
		marked = 1;
		break;

	case SchemeValueAccount:
		scheme = context->identifier.scheme;
		path = context->identifier.path;
		if ( strcmp( scheme, "file" ) || strcmp( va_name, "html" ) )
			return outputf( Error, ">:%%[ %s://%s ].$( %s ) - operation not supported",
				scheme, path, va_name );
		context->identifier.path = NULL;
		command_read( path, HCNFileInput, base, 0, context );
		marked = 1;
		break;

	case ExpressionResults:
		if ( context->expression.results == NULL )
			return retval;

		switch ( context->expression.mode ) {
		case ReadMode:
			marked = output_literal_variable( context->expression.results, context->expression.ptr );
			freeLiteral( &context->expression.results );
			break;
		default:
			marked = output_entity_variable( context->expression.results, context->expression.ptr );
		}
		break;

	case ExpressionValueAccount:
		if ( context->expression.results == NULL )
			return retval;

		if ( !strcmp( va_name, "html" ) ) {
			Entity *entity = (Entity *) context->expression.results->ptr;
			char *identifier = cn_va_get( entity, "hcn" );
			if ( identifier == NULL ) break;
			command_read( identifier, HCNValueFileInput, base, 0, context );
			marked = 1;
		}
		else if ( registryLookup( CN.VB, va_name ) == NULL ) {
			return outputf( Error, "unknown value account name '%s'", va_name );
		}
		else if ( !strcmp( va_name, "literal" ) ) {
			switch ( context->expression.mode ) {
			case ReadMode:
				marked = output_literal_variable( context->expression.results, context->expression.ptr );
				freeLiteral( &context->expression.results );
				break;
			default:
				marked = output_entity_variable( context->expression.results, context->expression.ptr );
			}
		}
		else marked = output_va_( va_name, context );
		break;

	case NarrativeBody:
		; char *name = context->identifier.id[ 1 ].ptr;
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
		{
			Entity *e = (Entity *) i->ptr;
			Registry *narratives = cn_va_get( e, "narratives" );
			if ( narratives == NULL ) continue;
			registryEntry *entry = registryLookup( narratives, name );
			if ( entry == NULL ) continue;
			marked = 1;
			output_narrative_((Narrative *) entry->value, context );
		}
		break;
	}
	context->output.marked |= marked;
	return retval;
}

/*---------------------------------------------------------------------------
	Public Interface	- cf. read_command()
---------------------------------------------------------------------------*/
int
output_mod( char *state, int event, char **next_state, _context *context )
	{ return output_( Modulo, event, context ); }
int
output_char( char *state, int event, char **next_state, _context *context )
	{ return output_( Char, event, context ); }
int
output_special_char( char *state, int event, char **next_state, _context *context )
	{ return output_( SpecialChar, event, context ); }
int
output_variable_value( char *state, int event, char **next_state, _context *context )
	{ return output_( VariableValue, event, context ); }
int
output_variator_value( char *state, int event, char **next_state, _context *context )
	{ return output_( VariatorValue, event, context ); }
int
output_scheme_descriptor( char *state, int event, char **next_state, _context *context )
	{ return output_( SchemeDescriptor, event, context ); }
int
output_scheme_va( char *state, int event, char **next_state, _context *context )
	{ return output_( SchemeValueAccount, event, context ); }
int
output_results( char *state, int event, char **next_state, _context *context )
	{ return output_( ExpressionResults, event, context ); }
int
output_va( char *state, int event, char **next_state, _context *context )
	{ return output_( ExpressionValueAccount, event, context ); }
int
output_narrative( char *state, int event, char **next_state, _context *context )
	{ return output_( NarrativeBody, event, context ); }

