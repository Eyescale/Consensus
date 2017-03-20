#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "narrative.h"
#include "narrative_util.h"
#include "variables.h"

// #define DEBUG
#include "output.h"

/*---------------------------------------------------------------------------
	registerNarrative
---------------------------------------------------------------------------*/
void
registerNarrative( Narrative *narrative, _context *context )
{
	addItem( &context->narrative.registered, narrative );
}

/*---------------------------------------------------------------------------
	deregisterNarrative
---------------------------------------------------------------------------*/
void
deregisterNarrative( Narrative *narrative, _context *context )
{
	removeItem( &context->narrative.registered, narrative );
	freeNarrative( narrative );
}

/*---------------------------------------------------------------------------
	addNarrative
---------------------------------------------------------------------------*/
registryEntry *
addNarrative( Entity *entity, char *name, Narrative *narrative )
/*
	add narrative to entity's narrative value account
*/
{
	registryEntry *va = lookupByName( CN.VB, "narratives" );
	if ( va == NULL ) return NULL;

	registryEntry *entry;
	if ( va->value == NULL ) {
#ifdef DEBUG
		fprintf( stderr, "debug> addNarrative(): first narrative ever - %s()\n", name );
#endif
		entry = newRegistryItem( name, narrative );
		va->value = newRegistryItem( entity, entry );
	} else {
		entry = lookupByAddress( va->value, entity );
		if ( entry == NULL ) {
#ifdef DEBUG
			fprintf( stderr, "debug> addNarrative(): first entity's narrative assignment - %s()\n", name );
#endif
			entry = newRegistryItem( name, narrative );
			registerByAddress((Registry *) &va->value, entity, entry );
		} else {
			Registry *narratives = (Registry *) &entry->value;
			entry = lookupByName( *narratives, name );
			if ( entry == NULL ) {
#ifdef DEBUG
				fprintf( stderr, "debug: addNarrative(): adding new entity's narrative - %s()\n", name );
#endif
				entry = registerByName( narratives, name, narrative );
			} else {
#ifdef DEBUG
				fprintf( stderr, "debug: addNarrative(): replacing entity's narrative - %s()\n", name );
#endif
				removeFromNarrative((Narrative *) entry->value, entity );
				entry->value = narrative;
			}
		}
	}

	registerByAddress( &narrative->entities, entity, NULL );
	return entry;
}


/*---------------------------------------------------------------------------
	removeNarrative
---------------------------------------------------------------------------*/
void
removeNarrative( Entity *entity, Narrative *narrative )
{
	registryEntry *entry = lookupByName( CN.VB, "narratives" );
	if ( entry == NULL ) return;
	Registry *va = (Registry *) &entry->value;
	entry = lookupByAddress( *va, entity );

	if ( entry == NULL ) return;
	registryEntry *last_r = NULL, *next_r;
	for ( registryEntry *r = (Registry) entry->value; r!=NULL; r=next_r )
	{
		Narrative *n = (Narrative *) r->value;
		next_r = r->next;
		if ( n == narrative )
		{
			removeFromNarrative( n, entity );
			if ( last_r == NULL ) {
				if ( next_r == NULL ) {
					deregisterByAddress( va, entity );
				} else {
					entry->value = next_r;
				}
			} else {
				last_r->next = next_r;
			}
			freeRegistryItem( r );
			return;
		}
		last_r = r;
	}
}

/*---------------------------------------------------------------------------
	activateNarrative
---------------------------------------------------------------------------*/
static void
duplicateNarrative( Occurrence *master, Occurrence *copy )
{
	for ( listItem *i=master->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *master_sub = (Occurrence *) i->ptr;
		Occurrence *copy_sub = (Occurrence *) malloc( sizeof( Occurrence ) );
		addItem( &copy->sub.n, copy_sub );
		copy->sub.num++;
		copy_sub->thread = copy;
		copy_sub->type = master_sub->type;
		copy_sub->registered = 0;
		bcopy( &master_sub->va, &copy_sub->va, sizeof( OccurrenceVA ) );
		copy_sub->sub.n = NULL;
		copy_sub->sub.num = 0;
		duplicateNarrative( master_sub, copy_sub );
	}
	reorderListItem( &copy->sub.n );
}

Narrative *
activateNarrative( Entity *entity, Narrative *narrative )
{
	Narrative *instance;
	registryEntry *entry = lookupByAddress( narrative->entities, entity );
	// entry->value is the pseudo used by this entity to refer to that narrative
	if (( entry->value == NULL ) && !narrative->assigned ) {
		instance = narrative;
		narrative->assigned = 1;
	} else {
		instance = (Narrative *) calloc( 1, sizeof( Narrative ) );
		duplicateNarrative( &narrative->root, &instance->root );
		instance->name = ( entry->value == NULL ) ?
			narrative->name : (char *) entry->value;
	}
	set_this_variable( &instance->variables, entity );
	registerByAddress( &narrative->instances, entity, instance );
	return instance;
}

/*---------------------------------------------------------------------------
	deactivateNarrative
---------------------------------------------------------------------------*/
void
deactivateNarrative( Entity *entity, Narrative *narrative )
{
	Narrative *instance;
	registryEntry *entry = lookupByAddress( narrative->instances, entity );
	instance = (Narrative *) entry->value;
	freeListItem( &instance->frame.events );
	freeListItem( &instance->frame.actions );
	freeListItem( &instance->frame.then );
	if ( instance == narrative ) {
		narrative->deactivate = 0;
		narrative->assigned = 0;
		freeVariables( &narrative->variables );
	} else {
		instance->name = NULL;
		freeNarrative( instance );
	}
	deregisterByValue( &narrative->instances, instance );
}

/*---------------------------------------------------------------------------
	lookupNarrative
---------------------------------------------------------------------------*/
Narrative *
lookupNarrative( Entity *entity, char *name )
{
	Registry narratives = cn_va_get_value( entity, "narratives" );
	registryEntry *entry = lookupByName( narratives, name );
	return ( entry == NULL ) ? NULL : entry->value;
}

/*---------------------------------------------------------------------------
	addToNarrative
---------------------------------------------------------------------------*/
registryEntry *
addToNarrative( Narrative *narrative, Entity *entity )
{
	registryEntry *entry = lookupByAddress( narrative->entities, entity );
	if ( entry != NULL ) {
		// free the alias previously referring to that narrative
		free( entry->value );
		entry->value = NULL;
	} else {
		entry = registerByAddress( &narrative->entities, entity, NULL );
	}
	return entry;
}

/*---------------------------------------------------------------------------
	removeFromNarrative
---------------------------------------------------------------------------*/
void
removeFromNarrative( Narrative *narrative, Entity *entity )
{
	registryEntry *entry = lookupByAddress( narrative->entities, entity );
	if ( entry == NULL ) return;
	// free the name by which this entity referred to that narrative
	free( entry->value );
	deregisterByAddress( &narrative->entities, entity );
	if ( narrative->entities == NULL ) {
		deregisterNarrative( narrative, CN.context );
	}
}
/*---------------------------------------------------------------------------
	reorderNarrative	- recursive
---------------------------------------------------------------------------*/
void
reorderNarrative( Occurrence *occurrence )
{
	if ( occurrence->sub.num == 1 ) {
		reorderNarrative((Occurrence *) occurrence->sub.n->ptr );
	}
	else if ( occurrence->sub.num > 1 ) {
		reorderListItem( &occurrence->sub.n );
		for ( listItem *i=occurrence->sub.n; i!=NULL; i=i->next )
			reorderNarrative((Occurrence *) i->ptr );
	}
}


/*---------------------------------------------------------------------------
	is_narrative_dangling	- recursive
---------------------------------------------------------------------------*/
int
is_narrative_dangling( Occurrence *occurrence, int cut )
{
	listItem *last_i = NULL, *next_i;
	listItem **list = &occurrence->sub.n;
	int sub_cut = cut;
	for ( listItem *i = *list; i!=NULL; i=next_i )
	{
		Occurrence *sub = (Occurrence *) i->ptr;
		next_i = i->next;
		if ( is_narrative_dangling( sub, sub_cut ) ) {
			switch ( sub->type ) {
			case ConditionOccurrence:
				free( sub->va.condition.expression );
				break;
			case EventOccurrence:
				free( sub->va.event.identifier.name );
				free( sub->va.event.expression );
				break;
			case ActionOccurrence:
				freeListItem( &sub->va.action.instructions );
				break;
			case ThenOccurrence:
				break;
			}
			clipListItem( list, i, last_i, next_i );
			occurrence->sub.num--;
		}
		else last_i = i;
#if 0
		sub_cut = sub_cut || (( sub->type == ActionOccurrence ) && sub->va.action.exit );
#endif
	}
	if ( cut ) {
		return 1;
	}
	if ( occurrence->sub.num == 0 ) {
		switch ( occurrence->type ) {
		case ConditionOccurrence:
		case EventOccurrence:
		case ThenOccurrence:
			return 1;
			break;
		case ActionOccurrence:
			return ( occurrence->va.action.instructions == NULL );
			break;
		default:
			return 1;
		}
	}
	return 0;
}
/*---------------------------------------------------------------------------
	freeNarrative	- recursive
---------------------------------------------------------------------------*/
static void
freeOccurrence( Occurrence *occurrence )
{
	for ( listItem *i = occurrence->sub.n; i!=NULL; i=i->next )
	{
		freeOccurrence( i->ptr );
		free( i->ptr );
	}
	freeListItem( &occurrence->sub.n );
	switch ( occurrence->type ) {
	case ConditionOccurrence:
		free( occurrence->va.condition.expression );
		break;
	case EventOccurrence:
		free( occurrence->va.event.identifier.name );
		free( occurrence->va.event.expression );
		break;
	case ActionOccurrence:
		freeListItem( &occurrence->va.action.instructions );
		break;
	case ThenOccurrence:
		break;
	}
}

void
freeNarrative( Narrative *narrative )
{
	freeOccurrence( &narrative->root );
	freeVariables( &narrative->variables );
	free( narrative->name );
	free( narrative );
}

