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
	command interface
---------------------------------------------------------------------------*/
static int output_( int type, char *state, int event, char **next_state, _context * );
enum {
	Modulo = 1,
	Dollar,
	Char,
	SpecialChar,
	SpecialQuote,
	VariableValue,
	VariatorValue,
	SchemeContents,
	SchemeValue,
	ResultsAsIs,
	ExpressionResults,
	EntitiesValueAccount,
	EntitiesNarrative
};
int
output_mod( char *state, int event, char **next_state, _context *context )
	{ return output_( Modulo, state, event, next_state, context ); }
int
output_dol( char *state, int event, char **next_state, _context *context )
	{ return output_( Dollar, state, event, next_state, context ); }
int
output_char( char *state, int event, char **next_state, _context *context )
	{ return output_( Char, state, event, next_state, context ); }
int
output_special_char( char *state, int event, char **next_state, _context *context )
	{ return output_( SpecialChar, state, event, next_state, context ); }
int
output_special_quote( char *state, int event, char **next_state, _context *context )
	{ return output_( SpecialQuote, state, event, next_state, context ); }
int
output_variable( char *state, int event, char **next_state, _context *context )
	{ return output_( VariableValue, state, event, next_state, context ); }
int
output_variator( char *state, int event, char **next_state, _context *context )
	{ return output_( VariatorValue, state, event, next_state, context ); }
int
output_scheme_contents( char *state, int event, char **next_state, _context *context )
	{ return output_( SchemeContents, state, event, next_state, context ); }
int
output_scheme_value( char *state, int event, char **next_state, _context *context )
	{ return output_( SchemeValue, state, event, next_state, context ); }
int
output_as_is( char *state, int event, char **next_state, _context *context )
	{ return output_( ResultsAsIs, state, event, next_state, context ); }
int
output_results( char *state, int event, char **next_state, _context *context )
	{ return output_( ExpressionResults, state, event, next_state, context ); }
int
output_vas( char *state, int event, char **next_state, _context *context )
	{ return output_( EntitiesValueAccount, state, event, next_state, context ); }
int
output_narratives( char *state, int event, char **next_state, _context *context )
	{ return output_( EntitiesNarrative, state, event, next_state, context ); }

static int
output_( int type, char *state, int event, char **next_state, _context *context )
{
	int retval;
	switch ( type ) {
	case Modulo:
	case Dollar:
	case VariableValue:
		retval = event;
		break;
	case Char:
		retval = (( event == '\n' ) ? '\n' : 0 );
		break;
	default:
		retval = 0;
	}
	if ( !command_mode( 0, 0, ExecutionMode ) )
		return retval;

	VariableVA *variable;
	char *scheme, *path, *name;
	context->error.flush_output = ( event != '\n' );
	int marked = 0;

	switch ( type ) {
	case Modulo:
		output( Text, "%" );
		marked = 1;
		break;
	case Dollar:
		output( Text, "$" );
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
	case SpecialQuote:
		output( Text, "\"" );
		marked = 1;
		break;
	case VariatorValue:
#ifdef DEBUG
		outputf( Debug, "output_variator_value: %%%s", variator_symbol );
#endif
		variable = lookupVariable( context, variator_symbol );
		if ( variable == NULL ) return retval;
		marked = output_variable_value( variable );
		break;
	case VariableValue:
#ifdef DEBUG
		outputf( Debug, "output_variable_value: %%%s", get_identifier( context, 1 ) );
#endif
		name = get_identifier( context, 1 );
		variable = lookupVariable( context, name );
		if ( variable == NULL ) return retval;
		marked = output_variable_value( variable );
		break;
	case SchemeContents:
		scheme = context->identifier.scheme;
		path = context->identifier.path;
		context->identifier.path = NULL;
		if ( strcmp( scheme, "file" ) )
			return outputf( Error, ">: $.< %s:%s > - operation not supported", scheme, path );
		char *fullpath = cn_path( context, "r", path );
		if ( fullpath != path ) free( path );
		FILE *file = fopen( fullpath, "r" );
		// DO_LATER: optimize
		for ( int byte; (( byte = fgetc( file )) != EOF ); )
			outputf( Text, "%c", byte );
		fclose( file );
		free( fullpath );
		break;
	case SchemeValue:
		scheme = context->identifier.scheme;
		path = context->identifier.path;
		if ( strcmp( scheme, "session" ) )
			return outputf( Error, ">: %%< %s:%s > - operation not supported", scheme, path );
		if ( !context->control.operator )
			return outputf( Error, "misdirected: >: %%< session:%s > - not operator", path  );
		registryEntry *entry = registryLookup( context->operator.sessions, path );
		if ( entry == NULL ) return retval;
		SessionVA *session = entry->value;
		outputf( Text, "%d", session->pid );
		marked = 1;
		break;
	case ResultsAsIs:
		if ( context->expression.results == NULL )
			return retval;
		struct { int as_is; } backup;
		backup.as_is = context->output.as_is;
		context->output.as_is = 1;
		switch ( context->expression.mode ) {
		case ReadMode:
			marked = output_collection( context->expression.results, LiteralResults, context->expression.ptr, 0 );
			freeLiterals( &context->expression.results );
			break;
		default:
			marked = output_collection( context->expression.results, EntityResults, context->expression.ptr, 0 );
		}
		context->output.as_is = backup.as_is;
		break;
	case ExpressionResults:
		if ( context->expression.results == NULL )
			return retval;
		switch ( context->expression.mode ) {
		case ReadMode:
			marked = output_collection( context->expression.results, LiteralResults, context->expression.ptr, 0 );
			freeLiterals( &context->expression.results );
			break;
		default:
			marked = output_collection( context->expression.results, EntityResults, context->expression.ptr, 0 );
		}
		break;
	case EntitiesValueAccount:
		if ( context->expression.results == NULL )
			return retval;
		char *va_name = get_identifier( context, 2 );
		if ( !strcmp( va_name, "html" ) ) {
			Entity *e = popListItem( &context->expression.results );
			char *hcn_value = cn_va_get( e, "hcn" );
			if ( hcn_value == NULL ) {
				hcn_value = ( e == CN.nil ) ? "//Session.hcn" : "//Entity.hcn";
			}
			retval = push_input( "", hcn_value, HCNValueFileInput, context );
			*next_state = base;
		}
		else if ( registryLookup( CN.VB, va_name ) == NULL ) {
			return outputf( Error, "unknown value account name '%s'", va_name );
		}
		else if ( !strcmp( va_name, "literal" ) ) {
			switch ( context->expression.mode ) {
			case ReadMode:
				marked = output_collection( context->expression.results, LiteralResults, context->expression.ptr, 0 );
				freeLiterals( &context->expression.results );
				break;
			default:
				marked = output_collection( context->expression.results, EntityResults, context->expression.ptr, 0 );
			}
		}
		else marked = output_entities_va( va_name, context );
		break;
	case EntitiesNarrative:
		if ( context->expression.results == NULL )
			return retval;
		name = get_identifier( context, 1 );
#ifdef DO_LATER
		for ( listItem *i = context->expression.results; i!=NULL; i=i->next )
#else
		listItem *i = context->expression.results; // output the first one only
#endif
		{
			Entity *e = (Entity *) i->ptr;
			Registry *narratives = cn_va_get( e, "narratives" );
			if ( narratives != NULL ) {
				registryEntry *entry = registryLookup( narratives, name );
				if ( entry != NULL ) {
					if ( context->output.marked ) output( Text, "\n" );
					output_narrative((Narrative *) entry->value, context );
					// marked = 1; // output_narrative does output "\n"
				}
			}
		}
		break;
	}
	context->output.marked |= marked;
	return retval;
}

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
	outputf
---------------------------------------------------------------------------*/
int
outputf( OutputContentsType type, const char *format, ... )
{
	_context *context = CN.context;
	int retval = (( type == Error ) || ( type == Debug ) ? -1 : 0 );
	if ( format == NULL ) {
		return retval;	// e.g. outputf( Error, NULL );
	}
	else if ( type == Error ) {
		flush_input( base, 0, &same, context );
		if ( !context->error.tell )
			return retval;
		context->error.tell = 0;
	}

	va_list ap;
	va_start( ap, format );
	if ( type == Text ) {
		if ( context->output.stack == NULL ) {
#if 0
			if ( !context->control.session )
#endif
			{
				anteprompt( context );
				vfprintf( stdout, ( *format ? format : "%s" ), ap );
			}
		}
		else {
			char *str;
			if ( *format ) {
				vasprintf( &str, format, ap );
			}
			else str = va_arg( ap, char * );
			if ( strcmp( str, "" ) )
			{
				OutputVA *output = context->output.stack->ptr;
				switch ( output->mode ) {
				case SessionOutput:
				case ClientOutput:
					io_write( output->ptr.socket, str, strlen( str ), &output->remainder );
					break;
				case FileOutput:
					fprintf( output->ptr.file, "%s", str );
					break;
				case StringOutput:
					; listItem **slist = &context->output.slist;
					StringVA *string = &context->output.string;
					for ( char *ptr = str; *ptr; ptr++ )
						slist_append( slist, string, *ptr, 1, 0 );
					break;
				}
			}
			if ( *format ) free( str );
		}
	}
#if 0
	else if ( ttyu( context ) || context->control.terminal ) {
#else
	else if ( !context->control.cgi ) {
#endif
		FILE *stream = stderr;
		anteprompt( context );
		if ( type == Enquiry ) {
			// output data
			vfprintf( stream, ( *format ? format : "%s" ), ap );
		} else {
			// output header
			switch ( type ) {
			case Error:
				if ( context->error.flush_output ) {
					// output in the text
					stream = stdout;
					context->error.flush_output = 0;
					fprintf( stream, "***** Error: " );
				}
				else if ( context->control.session ) {
					fprintf( stream, "Error < session: %s >: ", context->session.path );
				}
				else fprintf( stream, "Error: " );
				break;
			case Warning:
				if ( context->control.session ) {
					fprintf( stream, "Warning < session: %s >: ", context->session.path );
				}
				else fprintf( stream, "Warning: " );
				break;
			case Info:
				if ( context->control.session ) {
					fprintf( stream, "Info < session: %s >: ", context->session.path );
				}
				else fprintf( stream, "Info: " );
				break;
			case Debug:
				if ( context->control.session ) {
					fprintf( stream, "debug < session: %s >: ", context->session.path );
				}
				else fprintf( stream, "debug> ");
				break;
			default:
				break;
			}
			// output data
			vfprintf( stream, ( *format ? format : "%s" ), ap );
			fprintf( stream, "\n" );
		}
	}
	va_end( ap );
	return retval;
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
		fprintf( stderr, "passive$ " );
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
				context->control.cgim ? "cgim$ " :
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
static OutputVA * newOutput( OutputType type, _context * );
static void freeOutput( OutputVA *output, _context * );

int
push_output( char *identifier, void *dst, OutputType type, _context *context )
{
	if ( dst == NULL )
		switch ( type ) {
		case StringOutput:
			// used internally to convert entity or expression
			// into strings - cf. path.c, xl.c
			break;
		default:
			return 0;
		}

	if ( context->output.redirected )
	/*
		The main use case here is one-liner commands in HCN files, e.g.
			<< >@ operator:< \%[ ?-is->session ] | > variable: >>
		The output is already re-directed to CGI, and the last data
		packet (from HCN) must be sent (to CGI) before issueing cmd.
		Note that both context->input.hcn and context->output.cgi flags
		are reset to 0 below, which limits output re-entrance to one-liner
		commands, but allows clear_output_target() to pop output right
		after the command.
	*/
	{
		OutputVA *output = context->output.stack->ptr;
		switch ( output->mode ) {
		case ClientOutput:
		case SessionOutput:
			// send previous data packet without data-closing packet
			io_flush( output->ptr.socket, &output->remainder, 0, context );
			break;
		case StringOutput:
			// not supposed to happen
			break;
		}
	}
	OutputVA *output = newOutput( type, context );

	switch ( type ) {
	case SessionOutput:
		output->ptr.socket = io_query((char *) dst, context );
		if ( output->ptr.socket <= 0 ) {
			freeOutput( output, context );
			return outputf( Error, "push_output: could not reach session: \'%s\'", dst );
		}
		context->output.query = 1;
		context->output.redirected = 1;
		break;
	case FileOutput:
		; char *path = cn_path( context, "w", dst );
		output->ptr.file = fopen( path, "w" );
		if ( output->ptr.file == NULL ) {
			freeOutput( output, context );
			return outputf( Error, "push_output: could not open file: \'%s\'", dst );
		}
		if ( path != dst ) free( path );
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

static OutputVA *
newOutput( OutputType type, _context *context )
{
	OutputVA *output = (OutputVA *) calloc( 1, sizeof(OutputVA) );
	output->mode = type;

	output->level = context->command.level; // not used
	output->restore.redirected = context->output.redirected;
	output->restore.query = context->output.query;
	output->restore.hcn = context->input.hcn;
	output->restore.cgi = context->output.cgi;

	context->input.hcn = 0;
	context->output.cgi = 0;
	return output;
}

static void
freeOutput( OutputVA *output, _context *context )
{
	context->output.redirected = output->restore.redirected;
	context->output.query = output->restore.query;
	context->input.hcn = output->restore.hcn;
	context->output.cgi = output->restore.cgi;
	free( output );
}

/*---------------------------------------------------------------------------
	pop_output
---------------------------------------------------------------------------*/
int
pop_output( _context *context, int eot )
{
	OutputVA *output = (OutputVA *) context->output.stack->ptr;
	switch ( output->mode ) {
	case SessionOutput:
		io_finish( output->ptr.socket, &output->remainder, eot, context );
		break;
	case ClientOutput:
		io_finish( output->ptr.socket, &output->remainder, eot, context );
		break;
	case FileOutput:
		fclose( output->ptr.file );
		break;
	case StringOutput:
		slist_close( &context->output.slist, &context->output.string, 0 );
		reorderListItem( &context->output.slist );
		if (( output->variable.identifier ))
		{
			char *identifier = output->variable.identifier;
			if (( context->output.slist )) {
				listItem *value = context->output.slist;
				assign_variable( AssignSet, &identifier, value, StringResults, context );
				context->output.slist = NULL;
			}
			else free( identifier );
			output->variable.identifier = NULL;
		}
		break;
	}
	freeOutput( output, context );
	popListItem( &context->output.stack );
	return 0;
}
