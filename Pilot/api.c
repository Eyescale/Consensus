#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <fcntl.h>

#include "list.h"
#include "registry.h"
#include "database.h"
#include "kernel.h"

#include "command.h"
#include "api.h"
#include "io.h"
#include "frame.h"
#include "expression.h"
#include "expression_util.h"
#include "input.h"
#include "output.h"
#include "narrative_util.h"
#include "xl.h"

/*---------------------------------------------------------------------------
	cn_do, cn_dob, cn_dof
---------------------------------------------------------------------------*/
int
cn_dof( const char *format, ... )
{
	_context *context = CN.context;
	char *action = NULL;
	int event = 0;
	if ((format))
	{
		va_list ap;
		va_start( ap, format );
		if ( *format )
		{
			vasprintf( &action, format, ap );
			event = command_read( action, InstructionOne, base, 0, context );
			free( action );
		} else {
			action = va_arg( ap, char * );
			event = command_read( action, InstructionOne, base, 0, context );
		}
		va_end( ap );
	}
	else
	{
		// could do something here...
	}
	return event;
}

int
cn_dob( listItem *actions, int level )
{
	_context *context = CN.context;
	int retval;
	if ( level ) {
		struct { int level; } backup;
		backup.level = context->command.level;
		push( base, 0, &same, context );
		context->command.level = level;	// just to get the tabs right
		retval = command_read( actions, InstructionBlock, base, 0, context );
		context->command.level = backup.level;
	}
	else {
		push( base, 0, &same, context );
		retval = command_read( actions, InstructionBlock, base, 0, context );
	}
	return retval;
}

int
cn_do( char *action )
	{ return cn_dof( "", action ); }

/*---------------------------------------------------------------------------
	cn_setf, cn_getf, cn_testf
---------------------------------------------------------------------------*/
static Entity *vaopf( int op, char *format, va_list ap );

Entity *
cn_setf( char *format, ... )
/*
   cn_setf() allows to specify an expression via a format string using %s and
   %e to reference the (resp.) identifier or entity arguments.
   cn_setf() stores the arguments in a list and rewrites the format string to
   include the corresponding argument position in the list - e.g. %0e, %1s -
   so that read_expression() can cast them to the proper type in the expression.
   The format string is pushed as input to read_expression()
   The argument list is passed to expression_solve() via context->expression.args
*/
{
	va_list ap;
	va_start( ap, format );
	Entity *e = vaopf( InstantiateMode, format, ap );
	va_end( ap );
	return e;
}

static Entity *
vaopf( int op, char *format, va_list ap )
{
	_context *context = CN.context;

	struct { int mode; } backup;
	backup.mode = context->expression.mode;

	Entity *result = NULL;
	listItem *arglist = NULL;
	char *reformat = NULL;
	int argnum = 0;

	for ( char *fmt = format; *fmt; fmt++ )
	{
		switch ( *fmt ) {
		case '%':
			switch ( *++fmt ) {
			case 'e':
				addItem( &arglist, va_arg(ap, Entity *));
				if ((reformat)) {
					char *old = reformat;
					asprintf( &reformat, "%s%%%de", old, argnum++ );
					free( old );
				} else {
					asprintf( &reformat, "%%%de", argnum++ );
				}
				break;
			case 's':
				addItem( &arglist, va_arg(ap, char *));
				if ((reformat)) {
					char *old = reformat;
					asprintf( &reformat, "%s%%%ds", old, argnum++ );
					free( old );
				} else {
					asprintf( &reformat, "%%%ds", argnum++ );
				}
				break;
			default:
				output( Debug, "***** Error: vaopf(): unsupported format" );
				goto RETURN;
			}
			break;
		default:
			if ((reformat)) {
				char *old = reformat;
				asprintf( &reformat, "%s%c", old, *fmt );
				free( old );
			} else {
				asprintf( &reformat, "%c", *fmt );
			}
			break;
		}
	}
	reorderListItem( &arglist );

	context->expression.mode = BuildMode;
	push_input( "", reformat, InstructionOne, context );
	int retval = read_expression( base, 0, &same, context );
	pop_input( base, 0, NULL, context );

        if ( retval < 0 ) {
		output( Warning, "vaopf(): could not read expression" );
		goto RETURN;
	}

        context->expression.mode = op;
	context->expression.args = arglist;
        retval = expression_solve( context->expression.ptr, 3, NULL, context );
	context->expression.args = NULL;

        if ( retval < 0 ) {
		output( Warning, "vaopf(): could not solve expression" );
		goto RETURN;
	}
 
	if (( context->expression.results )) {
		result = context->expression.results->ptr;
		freeListItem( &context->expression.results );
	}

RETURN:
	free( context->expression.ptr );
	context->expression.ptr = NULL;
	free( reformat );
	freeListItem( &arglist );
	context->expression.args = NULL;
	context->expression.mode = backup.mode;
	return result;
}

/*---------------------------------------------------------------------------
	cn_getf
---------------------------------------------------------------------------*/
static Entity * vagetf( Entity **candidate, const char *format, va_list ap );

Entity *
cn_getf( const char *format, ... )
/*
	cn_getf() is a mini-solver taking a format string made of the
	following characters:
		[	starts sub-expression
		]	closes sub-expression
		?	target result
		.	identifier passed as argument
		%	entity passed as argument
	and returning the first found result, or NULL if not found
	e.g.
		cn_getf( "?..", "is", "session" )
	returns
		the first result of %[ ?-is->session ], if there is;
		NULL otherwise
*/
{
	Entity *e = NULL, *solve;
	va_list ap;
	va_start( ap, format );
	solve = vagetf( &e, format, ap );
	va_end( ap );
	return ( solve = NULL ) ? NULL : e;
}

static Entity *
vagetf( Entity **candidate, const char *format, va_list ap )
{
	Entity *sub[ 3 ], *e;
	int count = 0, mark = 0;
	while ( *format && ( count < 3 ) ) {
		switch ( *format++ ) {
		case '[':
			e = vagetf( candidate, format, ap );
			if ( e == NULL ) return NULL;
			sub[ count++ ] = e;
			break;
		case '?':
			mark = ++count;
			break;
		case '%':
			sub[ count++ ] = va_arg( ap, Entity * );
			break;
		case '.':
			; char *name = va_arg( ap, char * );
			e = cn_entity( name );
			if ( e == NULL ) return NULL;
			sub[ count++ ] = e;
			break;
		case ']':
			goto RETURN;
		}
	}
RETURN:
	if ( count < 3 ) {
		output( Debug, "***** Error: vagetf: format inconsistent" );
		return NULL;
	}
	if ( mark-- ) {
		int j=( mark + 1 )%3, k=( mark + 2 )%3;
		listItem *i;
		for ( i=sub[ j ]->as_sub[ j ]; i!=NULL; i=i->next ) {
			Entity *c = i->ptr;
			if ( c->sub[ k ] == sub[ k ] ) {
				sub[ mark ] = c->sub[ mark ];
				*candidate = sub[ mark ];
				break;
			}
		}
		if ( i == NULL ) return NULL;
	}
	return cn_instance( sub[ 0 ], sub[ 1 ], sub[ 2 ] );
}

/*---------------------------------------------------------------------------
	cn_instf
---------------------------------------------------------------------------*/
static Entity *vainstf( const char *format, va_list ap );

Entity *
cn_instf( const char *format, ... )
/*
	See cn_getf() format description above
	Note that string arguments will be duplicated - vs. cn_setf() -
	and that no notification will result for generated entities
*/
{

	va_list ap;
	va_start( ap, format );
	Entity *e = vainstf( format, ap );
	va_end( ap );
	return e;
}

static Entity *
vainstf( const char *format, va_list ap )
{
	Entity *sub[ 3 ], *e;
	int count = 0;
	while ( *format && ( count < 3 ) ) {
		switch ( *format++ ) {
		case '[':
			e = vainstf( format, ap );
			if ( e == NULL ) return NULL;
			sub[ count++ ] = e;
			break;
		case '%':
			sub[ count++ ] = va_arg( ap, Entity * );
			break;
		case '.':
			; char *identifier = va_arg( ap, char * );
			e = cn_entity( identifier );
			if ( e == NULL ) {
				identifier = strdup( identifier );
				e = newEntity( NULL, NULL, NULL, 0 );
				cn_va_set( e, "name", identifier );
				registryRegister( CN.names, identifier, e );
				addItem( &CN.DB, e );
			}
			sub[ count++ ] = e;
			break;
		case ']':
			goto RETURN;
		}
	}
RETURN:
	if ( count < 3 ) {
		output( Debug, "***** Error: vainstf: format inconsistent" );
		return NULL;
	}
	e = cn_instance( sub[ 0 ], sub[ 1 ], sub[ 2 ] );
	if ( e == NULL ) {
		e = newEntity( sub[ 0 ], sub[ 1 ], sub[ 2 ], 0 );
		addItem( &CN.DB, e );
	}
	return e;
}

/*---------------------------------------------------------------------------
	cn_entity
---------------------------------------------------------------------------*/
Entity *
cn_entity( char *name )
{
	registryEntry *entry = registryLookup( CN.names, name );
	return ( entry == NULL ) ? NULL : (Entity *) entry->value;
}

/*---------------------------------------------------------------------------
	cn_is_active
---------------------------------------------------------------------------*/
int
cn_is_active( Entity *e )
{
	return ( e->state == 1 ) ? 1 : 0;
}

/*---------------------------------------------------------------------------
	cn_name
---------------------------------------------------------------------------*/
char *
cn_name( Entity *e )
{
	return (char *) cn_va_get( e, "name" );
}

/*---------------------------------------------------------------------------
	cn_new
---------------------------------------------------------------------------*/
Entity *
cn_new( char *name )
{
	Entity *e = newEntity( NULL, NULL, NULL, 0 );
	cn_va_set( e, "name", name );
	registryRegister( CN.names, name, e );
	addItem( &CN.DB, e );
	CNLog *log = CN.context->frame.log.entities.backbuffer;
	addItem( &log->instantiated, e );
	return e;
}

/*---------------------------------------------------------------------------
	cn_free
---------------------------------------------------------------------------*/
void
cn_free( Entity *e )
{
	// remove all entity's narratives
	Registry *entity_narratives = cn_va_get( e, "narratives" );
	if (( entity_narratives )) {
		for ( registryEntry *r = entity_narratives->value, *next_r; r!=NULL; r=next_r )
		{
			next_r = r->next;
			removeNarrative( e, (Narrative *) r->value );
		}
	}

	// remove entity from name registry
	registryEntry *entry = registryLookup( CN.VB, "name" );
	entry = registryLookup( entry->value, e );
	if ((entry)) {
		char *name = entry->value;
		registryDeregister( CN.names, name );
		free( name );
	}

	// free entity's hcn, story and url strings
	entry = registryLookup( CN.VB, "hcn" );
	entry = registryLookup( entry->value, e );
	if ((entry)) free( entry->value );

	entry = registryLookup( CN.VB, "story" );
	entry = registryLookup( entry->value, e );
	if ((entry)) free( entry->value );

	entry = registryLookup( CN.VB, "url" );
	entry = registryLookup( entry->value, e );
	if ((entry)) free( entry->value );

	// close all value accounts associated with this entity
	for ( registryEntry *r = CN.VB->value; r!=NULL; r=r->next ) {
		Registry *registry = r->value;
		registryDeregister( registry, e );
	}

	// finally remove entity from CN.DB
	removeItem( &CN.DB, e );
	freeEntity( e );
}

/*---------------------------------------------------------------------------
	cn_va_set
---------------------------------------------------------------------------*/
registryEntry *
cn_va_set( Entity *e, char *va_name, void *value )
{
	registryEntry *entry = registryLookup( CN.VB, va_name );
	Registry *va = (Registry *) entry->value;
	entry = registryLookup( va, e );
	if ( entry == NULL ) {
		return ( value == NULL ) ? NULL : registryRegister( va, e, value );
	}
	else if ( !strcmp( va_name, "narratives" ) ) {
		Registry *entity_narratives = entry->value;
		// deregister entity from all previous narratives
		for (registryEntry *i = entity_narratives->value; i!=NULL; i=i->next )
		{
			Narrative *n = (Narrative *) i->value;
			removeFromNarrative( n, e );
		}
		// reset value account
		freeRegistry((Registry **) &entry->value );
	}
	else {
		free( entry->value );
	}
	if ( value == NULL ) {
		registryDeregister( va, e );
	}
	else entry->value = value;

	return entry;
}

/*---------------------------------------------------------------------------
	cn_va_get
---------------------------------------------------------------------------*/
void *
cn_va_get( Entity *e, char *va_name )
{
	registryEntry *entry = registryLookup( CN.VB, va_name );
	if ( entry == NULL ) return NULL;
	entry = registryLookup( entry->value, e );
	return ( entry == NULL ) ? NULL : entry->value;
}

/*---------------------------------------------------------------------------
	cn_instance
---------------------------------------------------------------------------*/
Entity *
cn_instance( Entity *source, Entity *medium, Entity *target )
{
	Entity *sub[3];
	sub[0] = source;
	sub[1] = medium;
	sub[2] = target;

	for ( int i=0; i<3; i++ )
	if ( sub[i] != NULL ) {
		for ( listItem *j = sub[i]->as_sub[i]; j!=NULL; j=j->next )
		{
			Entity *e = (Entity *) j->ptr;	
			int k = (i+1)%3;
			if ( e->sub[k] != sub[k] )
				continue;
			k = (k+1)%3;
			if ( e->sub[k] != sub[k] )
				continue;
			return e;
		}
		return NULL;
	}

	return NULL;
}

/*---------------------------------------------------------------------------
	cn_instantiate
---------------------------------------------------------------------------*/
Entity *
cn_instantiate( Entity *source, Entity *medium, Entity *target, int twist )
{
	if ((source) && ( source->twist & 8 )) twist |= 1;
	if ((medium) && ( medium->twist & 8 )) twist |= 2;
	if ((target) && ( target->twist & 8 )) twist |= 4;
	Entity *e = newEntity( source, medium, target, twist );
	addItem( &CN.DB, e );
	CNLog *log = CN.context->frame.log.entities.backbuffer;
	addItem( &log->instantiated, e );
	return e;
}

/*---------------------------------------------------------------------------
	cn_release
---------------------------------------------------------------------------*/
void
cn_release( Entity *e )
{
	if (( e == CN.nil ) || ( e == CN.this ) || ( e->state == -1 ))
		return;

	// we must find all instances where e is involved and release these as well.
	for ( int i=0; i<3; i++ ) {
		listItem *next_j;
		for ( listItem *j=e->as_sub[ i ]; j!=NULL; j=next_j ) {
			next_j = j->next;
			cn_release( (Entity *) j->ptr );
		}
	}
	CNLog *log = CN.context->frame.log.entities.backbuffer;
	addItem( &log->released, e2l(e) );
	cn_free( e );
}

/*---------------------------------------------------------------------------
	cn_activate
---------------------------------------------------------------------------*/
int
cn_activate( Entity *e )
{
	if ( !cn_is_active( e ) ) {
		e->state = 1;
		CNLog *log = CN.context->frame.log.entities.backbuffer;
		addItem( &log->activated, e );
		return 1;
	}
	else return 0;
}

/*---------------------------------------------------------------------------
	cn_deactivate
---------------------------------------------------------------------------*/
int
cn_deactivate( Entity *e )
{
	if ( cn_is_active( e ) ) {
		e->state = 0;
		CNLog *log = CN.context->frame.log.entities.backbuffer;
		addItem( &log->deactivated, e );
		return 1;
	}
	else return 0;
}

/*---------------------------------------------------------------------------
	cn_instantiate_narrative
---------------------------------------------------------------------------*/
int
cn_instantiate_narrative( Entity *e, Narrative *narrative )
{
	// does e already have a narrative with the same name?
	Narrative *n = lookupNarrative( e, narrative->name );
	if ( n != NULL ) {
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Warning, "narrative '%s' already exists - cannot instantiate", narrative->name );
		return 0;
	}
	addNarrative( e, narrative->name, narrative );

	return 1;
}

/*---------------------------------------------------------------------------
	cn_release_narrative
---------------------------------------------------------------------------*/
int
cn_release_narrative( Entity *e, char *name )
{
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return 0;

	//check if the requested narrative instance is active
	if ( registryLookup( &n->instances, e ) != NULL ) {
#ifdef DO_LATER
		// output in full: %[ e ].name() - cf. output_narrative_name()
#endif
		outputf( Warning, "narrative '%s' is currently active - cannot release", name );
		return 0;
	}
	removeNarrative( e, n );

	return 1;
}

/*---------------------------------------------------------------------------
	cn_activate_narrative
---------------------------------------------------------------------------*/
Narrative *
cn_activate_narrative( Entity *e, char *name )
{
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return NULL;

	// check if the requested narrative instance already exists
	if (( registryLookup( &n->instances, e ) )) return n;

	// log narrative activation event - the actual activation is performed in systemFrame
	Registry *r = &CN.context->frame.log.narratives.activate;
	registryEntry *entry = registryLookup( r, n );
	if ( entry == NULL ) {
		registryRegister( r, n, newItem(e) );
	} else {
		addIfNotThere((listItem **) &entry->value, e );
	}

	return n;
}

/*---------------------------------------------------------------------------
	cn_deactivate_narrative
---------------------------------------------------------------------------*/
int
cn_deactivate_narrative( Entity *e, char *name )
{
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return 0;

	// check if the requested narrative instance actually exists
	if ( registryLookup( &n->instances, e ) == NULL ) return 0;

	// log narrative deactivation event - the actual deactivation is performed in systemFrame
	Registry *r = &CN.context->frame.log.narratives.deactivate;
	registryEntry *entry = registryLookup( r, n );
	if ( entry == NULL ) {
		registryRegister( r, n, newItem(e) );
	} else {
		addIfNotThere((listItem **) &entry->value, e );
	}

	return 1;
}

