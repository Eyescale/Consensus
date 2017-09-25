#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "output.h"
#include "narrative.h"

/*---------------------------------------------------------------------------
	cn_entity
---------------------------------------------------------------------------*/
Entity *
cn_entity( char *name )
{
	registryEntry *entry = lookupByName( CN.registry, name );
	return ( entry == NULL ) ? NULL : (Entity *) entry->value;
}

/*---------------------------------------------------------------------------
	cn_name
---------------------------------------------------------------------------*/
char *
cn_name( Entity *e )
{
	return (char *) cn_va_get_value( e, "name" );
}

/*---------------------------------------------------------------------------
	cn_new
---------------------------------------------------------------------------*/
Entity *
cn_new( char *name )
{
	Entity *e = newEntity( NULL, NULL, NULL );
	cn_va_set_value( e, "name", name );
	registerByName( &CN.registry, name, e );
	addItem( &CN.DB, e );
	addItem( &CN.context->frame.log.entities.instantiated, e );
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

	name = cn_va_get_value( e, "name" );
	free( name );
	name = cn_va_get_value( e, "hcn" );
	free( name );
	name = cn_va_get_value( e, "url" );
	free( name );

	// remove all entity's narratives

	Registry narratives = cn_va_get_value( e, "narratives" );
	for ( registryEntry *r = narratives; r!=NULL; r=r->next )
	{
		Narrative *n = (Narrative *) r->value;
		removeNarrative( e, n );
	}

	// remove entity from name registry

	deregisterByValue( &CN.registry, e );

	// close all value accounts associated with this entity

	for ( registryEntry *r = CN.VB; r!=NULL; r=r->next ) {
		deregisterByAddress( (Registry *) &r->value, e );
	}

	// finally remove entity from CN.DB

	removeItem( &CN.DB, e );
	freeEntity( e );
}

/*---------------------------------------------------------------------------
	cn_va_set_value
---------------------------------------------------------------------------*/
registryEntry *
cn_va_set_value( Entity *e, char *va_name, void *value )
{
	if ( value == NULL ) return NULL;
	registryEntry *entry = lookupByName( CN.VB, va_name );
	if ( entry == NULL ) return NULL;
	Registry *va = (Registry *) &entry->value;
	entry = lookupByAddress( *va, e );
	if ( entry == NULL ) {
		return registerByAddress( va, e, value );
	} else if ( !strcmp( va_name, "narratives" ) ) {
		// deregister entity from all previous narratives
		for (registryEntry *i = (Registry) entry->value; i!=NULL; i=i->next )
		{
			Narrative *n = (Narrative *) i->value;
			removeFromNarrative( n, e );
		}
		// reset value account
		freeRegistry((Registry *) &entry->value );
	} else {
		free( entry->value );
	}
	entry->value = value;
	return entry;
}

/*---------------------------------------------------------------------------
	cn_va_get_value
---------------------------------------------------------------------------*/
void *
cn_va_get_value( Entity *e, char *va_name )
{
	registryEntry *va = lookupByName( CN.VB, va_name );
	if ( va == NULL ) return NULL;
	registryEntry *entry = lookupByAddress((Registry) va->value, e );
	if ( entry == NULL ) return NULL;
	return entry->value;

}

/*---------------------------------------------------------------------------
	cn_expression
---------------------------------------------------------------------------*/
Expression *
cn_expression( Entity *e )
{
	if ( e == NULL ) return NULL;

	Expression *expression = (Expression *) calloc( 1, sizeof(Expression));
	ExpressionSub *sub = expression->sub;
	sub[ 3 ].result.any = 1;
	sub[ 3 ].e = expression;

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
				sub[ i ].e = cn_expression( e->sub[ i ] );
				sub[ i ].result.none = ( sub[ i ].e == NULL );
			}
		}
	}
	return expression;
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
	cn_is_active
---------------------------------------------------------------------------*/
int
cn_is_active( Entity *e )
{
	return ( e->state == 1 ) ? 1 : 0;
}

/*---------------------------------------------------------------------------
	cn_instantiate
---------------------------------------------------------------------------*/
Entity *
cn_instantiate( Entity *source, Entity *medium, Entity *target )
{
	Entity *e = newEntity( source, medium, target );
	addItem( &CN.DB, e );
	addItem( &CN.context->frame.log.entities.instantiated, e );
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
	addItem( &CN.context->frame.log.entities.released, &expression->sub[ 3 ] );
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
		addItem( &CN.context->frame.log.entities.activated, e );
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
		addItem( &CN.context->frame.log.entities.deactivated, e );
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
		fprintf( stderr, "consensus> Warning: narrative '" );
		output_narrative_name( e, narrative->name );
		fprintf( stderr, "' already exists - cannot instantiate\n" );
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
	if ( lookupByAddress( n->instances, e ) != NULL ) {
		fprintf( stderr, "consensus> Warning: narrative '" );
		output_narrative_name( e, name );
		fprintf( stderr, "' is currently active - cannot release\n" );
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
	if ( lookupByAddress( n->instances, e ) != NULL ) return 0;

	// log narrative activation event - the actual activation is performed in systemFrame
	Registry *log = &CN.context->frame.log.narratives.activate;
	if ( *log == NULL ) {
		CN.context->frame.log.narratives.activate = newRegistryItem( n, newItem( e ) );
	} else {
		registryEntry *entry = lookupByAddress( *log, n );
		if ( entry == NULL ) {
			registerByAddress( log, n, newItem(e) );
		} else {
			addIfNotThere((listItem **) &entry->value, e );
		}
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
	if ( lookupByAddress( n->instances, e ) == NULL ) return 0;

	// log narrative deactivation event - the actual deactivation is performed in systemFrame
	Registry *log = &CN.context->frame.log.narratives.deactivate;
	if ( *log == NULL ) {
		CN.context->frame.log.narratives.deactivate = newRegistryItem( n, newItem( e ) );
	} else {
		registryEntry *entry = lookupByAddress( *log, n );
		if ( entry == NULL ) {
			registerByAddress( log, n, newItem(e) );
		} else {
			addIfNotThere((listItem **) &entry->value, e );
		}
	}
	return 1;
}
