#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <fcntl.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "command.h"
#include "api.h"
#include "expression.h"
#include "input.h"
#include "output.h"
#include "narrative.h"

/*---------------------------------------------------------------------------
	cn_read
---------------------------------------------------------------------------*/
int
cn_read( void *src, InputType type, char *state, int event )
{
	_context *context = CN.context;

	int restore[ 3 ];
	restore[ 0 ] = event;
	char *next_state = same;

	event = command_init( src, type, restore, context );
	if ( event < 0 ) return event;
	do {
		event = read_command( state, event, &next_state, context );
		state = next_state;
	}
	while ( strcmp( state, "" ) );
	event = command_exit( state, event, restore, context );
	return event;
}

/*---------------------------------------------------------------------------
	cn_do, cn_dol, cn_dof
---------------------------------------------------------------------------*/
int
cn_dof( const char *format, ... )
{
	char *action = NULL;
	int event = 0;
	if ((format))
	{
		va_list ap;
		va_start( ap, format );
		if ( *format )
		{
			vasprintf( &action, format, ap );
			event = cn_read( action, InstructionOne, base, 0 );
			free( action );
		} else {
			action = va_arg( ap, char * );
			event = cn_read( action, InstructionOne, base, 0 );
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
cn_do( char *action )
{
	return cn_dof( "", action );
}

int
cn_dol( listItem *actions )
{
	return cn_read( actions, InstructionBlock, base, 0 );
}

/*---------------------------------------------------------------------------
	cn_setf, cn_getf, cn_testf
---------------------------------------------------------------------------*/
static Entity *
vaopf( int op, char *format, va_list ap )
{
	_context *context = CN.context;
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

	push_input( "", reformat, InstructionOne, context );
	read_expression( base, 0, &same, context );
	pop_input( base, 0, NULL, context );

        if ( context->expression.mode == ErrorMode ) {
		output( Warning, "vaopf(): could not read expression" );
		goto RETURN;
	}

        context->expression.mode = op;
	context->expression.args = arglist;
        int retval = expression_solve( context->expression.ptr, 3, context );
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
	free( reformat );
	freeListItem( &arglist );
	context->expression.args = NULL;
	return result;
}

Entity *
cn_setf( char *format, ... )
{
	va_list ap;
	va_start( ap, format );
	Entity *e = vaopf( InstantiateMode, format, ap );
	va_end( ap );
	return e;
}

/*---------------------------------------------------------------------------
	cn_getf
---------------------------------------------------------------------------*/
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

Entity *
cn_getf( const char *format, ... )
{
	Entity *e = NULL, *solve;
	va_list ap;
	va_start( ap, format );
	solve = vagetf( &e, format, ap );
	va_end( ap );
	return ( solve = NULL ) ? NULL : e;
}

/*---------------------------------------------------------------------------
	cn_instf
---------------------------------------------------------------------------*/
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
			if ( e == NULL )
			{
				identifier = strdup( identifier );
				e = newEntity( NULL, NULL, NULL );
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
		e = newEntity( sub[ 0 ], sub[ 1 ], sub[ 2 ] );
		addItem( &CN.DB, e );
	}
	return e;
}

Entity *
cn_instf( const char *format, ... )
{

	va_list ap;
	va_start( ap, format );
	Entity *e = vainstf( format, ap );
	va_end( ap );
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
	Entity *e = newEntity( NULL, NULL, NULL );
	cn_va_set( e, "name", name );
	registryRegister( CN.names, name, e );
	addItem( &CN.DB, e );
	FrameLog *log = &CN.context->io.input.log;
	addItem( &log->entities.instantiated, e );
	return e;
}

/*---------------------------------------------------------------------------
	cn_free
---------------------------------------------------------------------------*/
void
cn_free( Entity *e )
{
	void *name;

	// free entity's name, hcn and url strings

	name = cn_va_get( e, "name" );
	free( name );
	name = cn_va_get( e, "hcn" );
	free( name );
	name = cn_va_get( e, "url" );
	free( name );

	// remove all entity's narratives

	Registry *entity_narratives = cn_va_get( e, "narratives" );
	if (( entity_narratives )) {
		for ( registryEntry *r = entity_narratives->value; r!=NULL; r=r->next )
		{
			Narrative *n = (Narrative *) r->value;
			removeNarrative( e, n );
		}
	}

	// remove entity from name registry (deregister by value)

	registryDeregister( CN.names, NULL, e );

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
	if ( value == NULL ) return NULL;

	registryEntry *entry = registryLookup( CN.VB, va_name );
	Registry *va = (Registry *) entry->value;
	entry = registryLookup( va, e );
	if ( entry == NULL )
	{
		return registryRegister( va, e, value );
	}
	else if ( !strcmp( va_name, "narratives" ) )
	{
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
	else free( entry->value );
	entry->value = value;
	return entry;
}

/*---------------------------------------------------------------------------
	cn_va_get
---------------------------------------------------------------------------*/
void *
cn_va_get( Entity *e, char *va_name )
{
	registryEntry *entry = registryLookup( CN.VB, va_name );
	Registry *va = (Registry *) entry->value;
	entry = registryLookup( va, e );
	if ( entry == NULL ) return NULL;

	return entry->value;

}

/*---------------------------------------------------------------------------
	cn_expression
---------------------------------------------------------------------------*/
static Expression *
cn_x( Entity *e, ExpressionSub *super )
{
	if ( e == NULL ) return NULL;

	Expression *expression = (Expression *) calloc( 1, sizeof(Expression));
	ExpressionSub *sub = expression->sub;
	sub[ 3 ].result.any = 1;
	sub[ 3 ].e = expression;
	sub[ 3 ].super = super;

	if ( e == CN.nil ) {
		sub[ 0 ].result.identifier.type = NullIdentifier;
		sub[ 1 ].result.none = 1;
		sub[ 2 ].result.none = 1;
	}
	else
	{
		char *name = cn_name( e );
		if ( name != NULL ) {
			sub[ 0 ].result.identifier.type = DefaultIdentifier;
			sub[ 0 ].result.identifier.value = strdup( name );
			sub[ 1 ].result.none = 1;
			sub[ 2 ].result.none = 1;
		}
		else for ( int i=0; i<3; i++ ) {
			name = cn_name( e->sub[ i ] );
			if ( name != NULL ) {
				sub[ i ].result.identifier.type = DefaultIdentifier;
				sub[ i ].result.identifier.value = strdup( name );
			} else {
				sub[ i ].e = cn_x( e->sub[ i ], &sub[ 3 ] );
				sub[ i ].result.none = ( sub[ i ].e == NULL );
			}
		}
	}
	return expression;
}

Expression *
cn_expression( Entity *e )
{
	return cn_x( e, NULL );
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
cn_instantiate( Entity *source, Entity *medium, Entity *target )
{
	Entity *e = newEntity( source, medium, target );
	addItem( &CN.DB, e );
	FrameLog *log = &CN.context->io.input.log;
	addItem( &log->entities.instantiated, e );
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
	Expression *expression = cn_expression( e );
	FrameLog *log = &CN.context->io.input.log;
	addItem( &log->entities.released, &expression->sub[ 3 ] );
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
		FrameLog *log = &CN.context->io.input.log;
		addItem( &log->entities.activated, e );
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
		FrameLog *log = &CN.context->io.input.log;
		addItem( &log->entities.deactivated, e );
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
int
cn_activate_narrative( Entity *e, char *name )
{
	Narrative *n = lookupNarrative( e, name );
	if ( n == NULL ) return 0;

	// check if the requested narrative instance already exists
	if (( registryLookup( &n->instances, e ) )) return 0;

	// log narrative activation event - the actual activation is performed in systemFrame
	FrameLog *log = &CN.context->io.input.log;
	Registry *registry = &log->narratives.activate;
	registryEntry *entry = registryLookup( registry, n );
	if ( entry == NULL ) {
		registryRegister( registry, n, newItem(e) );
	} else {
		addIfNotThere((listItem **) &entry->value, e );
	}

	return 1;
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
	FrameLog *log = &CN.context->io.input.log;
	Registry *r = &log->narratives.deactivate;
	registryEntry *entry = registryLookup( r, n );
	if ( entry == NULL ) {
		registryRegister( r, n, newItem(e) );
	} else {
		addIfNotThere((listItem **) &entry->value, e );
	}

	return 1;
}

/*---------------------------------------------------------------------------
	cn_open
---------------------------------------------------------------------------*/
int
cn_open( char *path, int oflags )
{
	// open stream
	int fd = open( path, oflags );
	if ( fd < 0 ) {
		// log error	DO_LATER
		return outputf( Warning, "file://%s - unable to open", path );
	}
	char *mode;
	switch ( oflags ) {
	case O_RDONLY: mode = "read_only"; break;
	case O_WRONLY: mode = "write-only"; break;
	case O_RDWR: mode = "read-write"; break;
	}
	// instantiate stream entity
	/*
		we don't want any event to be logged for this
		particular instantiation, but we want to retrieve
		the resulting entity. so we do it by hand...
	*/
	char *name;
	asprintf( &name, "%d", fd );

	Entity *e[ 5 ];
	e[ 0 ] = cn_instf( "...", name, "is", "Stream" );
	e[ 1 ] = cn_instf( "%..", e[0], "has", "Mode" );
	e[ 2 ] = cn_instf( "..%", strdup( mode ), "is", e[1] );
 	e[ 3 ] = cn_instf( "%..", e[0], "has", "File" );
 	e[ 4 ] = cn_instf( "..%", strdup( path ), "is", e[3] );

	free( name );

	// log stream event
	addItem( &CN.context->frame.log.streams.instantiated, e[0] );
	return 0;
}
