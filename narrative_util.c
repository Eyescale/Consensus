#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>
#include <string.h>
#include <strings.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"

#include "api.h"
#include "output.h"
#include "narrative.h"
#include "narrative_util.h"
#include "variable.h"
#include "variable_util.h"

// #define DEBUG

/*---------------------------------------------------------------------------
	newNarrative, newOccurrence
---------------------------------------------------------------------------*/
Narrative *
newNarrative( void )
{
	Narrative *narrative = calloc( 1, sizeof(Narrative) );
	RTYPE( &narrative->variables ) = IndexedByName;
	return narrative;
}

Occurrence *
newOccurrence( OccurrenceType type )
{
	Occurrence *occurrence = calloc( 1, sizeof(Occurrence) );
	RTYPE( &occurrence->variables ) = IndexedByName;
	occurrence->type = type;
	return occurrence;
}

/*---------------------------------------------------------------------------
	freeNarrative, freeOccurrence	- recursive
---------------------------------------------------------------------------*/
void
freeNarrative( Narrative *narrative )
{
	freeListItem( &narrative->frame.events );
	freeListItem( &narrative->frame.actions );
	freeListItem( &narrative->frame.then );
	freeOccurrence( &narrative->root );
	freeVariables( &narrative->variables );
	free( narrative->name );
	free( narrative );
}

void
freeOccurrence( Occurrence *occurrence )
{
	if ( occurrence == NULL ) return;
	for ( listItem *i = occurrence->sub.n; i!=NULL; i=i->next )
	{
		freeOccurrence( i->ptr );
		free( i->ptr );
	}
	freeVariables( &occurrence->variables );
	freeListItem( &occurrence->sub.n );
	switch ( occurrence->type ) {
	case OtherwiseOccurrence:
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			freeOccurrence( i->ptr );
		}
		break;
	case ConditionOccurrence:
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			if ( i->ptr == NULL ) continue;
			ConditionVA *condition = (ConditionVA *) i->ptr;
			free( condition->identifier );
			free( condition->format );
			free( condition );
		}
		break;
	case EventOccurrence:
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			if ( i->ptr == NULL ) continue;
			EventVA *event = (EventVA *) i->ptr;
			free( event->identifier );
			free( event->format );
			free( event );
		}
		break;
	case ActionOccurrence:
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			if ( i->ptr == NULL ) continue;
			ActionVA *action = (ActionVA *) occurrence->va->ptr;
			freeListItem( &action->instructions );
			free( action );
		}
		break;
	case ThenOccurrence:
		break;
	}
	freeListItem( &occurrence->va );
}

/*---------------------------------------------------------------------------
	registerNarrative, deregisterNarrative
---------------------------------------------------------------------------*/
void
registerNarrative( Narrative *narrative, _context *context )
{
	addItem( &context->narrative.registered, narrative );
}

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
	registryEntry *entry = registryLookup( CN.VB, "narratives" );
	Registry *narratives = (Registry *) entry->value;
	entry = registryLookup( narratives, entity );
	if ( entry == NULL ) {
#ifdef DEBUG
		outputf( Debug, "addNarrative(): first entity's narrative assignment - %s()", name );
#endif
		Registry *registry = newRegistry( IndexedByName );
		registryRegister( registry, name, narrative );
		registryRegister( narratives, entity, registry );
	}
	else
	{
		Registry *entity_narratives = (Registry *) entry->value;
		entry = registryLookup( entity_narratives, name );
		if ( entry == NULL ) {
#ifdef DEBUG
			outputf( Debug, "addNarrative(): adding new entity's narrative - %s()", name );
#endif
			entry = registryRegister( entity_narratives, name, narrative );
		} else {
#ifdef DEBUG
			outputf( Debug, "addNarrative(): replacing entity's narrative - %s()", name );
#endif
			removeFromNarrative((Narrative *) entry->value, entity );
			entry->value = narrative;
		}
	}
	registryRegister( &narrative->entities, entity, NULL );
	return entry;
}


/*---------------------------------------------------------------------------
	removeNarrative
---------------------------------------------------------------------------*/
void
removeNarrative( Entity *entity, Narrative *narrative )
{
	registryEntry *entry = registryLookup( CN.VB, "narratives" );
	Registry *narratives = (Registry *) entry->value;
	entry = registryLookup( narratives, entity );
	if ( entry == NULL ) return;

	Registry *entity_narratives = (Registry *) entry->value;

	registryEntry *last_r = NULL, *next_r;
	for ( registryEntry *r = entity_narratives->value; r!=NULL; r=next_r )
	{
		Narrative *n = (Narrative *) r->value;
		next_r = r->next;
		if ( n == narrative )
		{
			removeFromNarrative( n, entity );
			if ( last_r == NULL ) {
				if ( next_r == NULL ) {
					registryDeregister( narratives, entity );
					// Registries are indexed by type
					freeRegistryEntry( IndexedByNumber, entity_narratives );
				} else {
					entity_narratives->value = next_r;
				}
			}
			else last_r->next = next_r;
			freeRegistryEntry( IndexedByAddress, r );
			return;
		}
		last_r = r;
	}
}

/*---------------------------------------------------------------------------
	addToNarrative
---------------------------------------------------------------------------*/
registryEntry *
addToNarrative( Narrative *narrative, Entity *entity )
{
	registryEntry *entry = registryLookup( &narrative->entities, entity );
	if ( entry != NULL ) {
		// free the alias previously referring to that narrative
		free( entry->value );
		entry->value = NULL;
	}
	else {
		entry = registryRegister( &narrative->entities, entity, NULL );
	}
	return entry;
}

/*---------------------------------------------------------------------------
	removeFromNarrative
---------------------------------------------------------------------------*/
void
removeFromNarrative( Narrative *narrative, Entity *entity )
{
	registryEntry *entry = registryLookup( &narrative->entities, entity );
	if ( entry == NULL ) return;
	// free the name by which this entity referred to that narrative
	free( entry->value );
	registryDeregister( &narrative->entities, entity );
	if ( narrative->entities.value == NULL ) {
		deregisterNarrative( narrative, CN.context );
	}
}

/*---------------------------------------------------------------------------
	lookupNarrative
---------------------------------------------------------------------------*/
Narrative *
lookupNarrative( Entity *entity, char *name )
{
	registryEntry *entry = registryLookup( CN.VB, "narratives" );
	if ( entry == NULL ) return NULL;
	entry = registryLookup( entry->value, entity );
	if ( entry == NULL ) return NULL;
	entry = registryLookup( entry->value, name );
	return ( entry == NULL ) ? NULL : entry->value;
}

/*---------------------------------------------------------------------------
	activateNarrative
---------------------------------------------------------------------------*/
static void duplicateNarrative( Occurrence *master, Occurrence *copy );
static void search_and_register_init( Narrative * instance );

Narrative *
activateNarrative( Entity *entity, Narrative *narrative )
{
	Narrative *instance;
	registryEntry *entry = registryLookup( &narrative->entities, entity );
	// entry->value is the pseudo used by this entity to refer to that narrative
	if (( entry->value == NULL ) && !narrative->assigned ) {
		instance = narrative;
		narrative->assigned = 1;
	} else {
		instance = newNarrative();
		duplicateNarrative( &narrative->root, &instance->root );
		instance->name = ( entry->value == NULL ) ?
			narrative->name : (char *) entry->value;
	}
	assign_this_variable( &instance->variables, entity );
	registryRegister( &narrative->instances, entity, instance );
	search_and_register_init( instance );
	return instance;
}

static void
duplicateNarrative( Occurrence *master, Occurrence *copy )
{
	for ( listItem *i=master->sub.n; i!=NULL; i=i->next )
	{
		Occurrence *master_sub = (Occurrence *) i->ptr;
		Occurrence *copy_sub = newOccurrence( master_sub->type );
		addItem( &copy->sub.n, copy_sub );
		copy->sub.num++;
		copy_sub->thread = copy;
		copy_sub->registered = 0;
		memcpy( &copy_sub->va, &master_sub->va, sizeof( Occurrence ) );
		switch ( master_sub->type ) {
		case ConditionOccurrence:
			for ( listItem *i = master_sub->va; i!=NULL; i=i->next ) {
				if ( i->ptr == NULL ) continue;
				ConditionVA *condition = (ConditionVA *) i->ptr;
				ConditionVA *copy_va = (ConditionVA *) malloc( sizeof( ConditionVA ) );
				memcpy( copy_va, condition, sizeof( ConditionVA ) );
				addItem( &copy_sub->va, copy_va );
			}
			break;
		case EventOccurrence:
			for ( listItem *i = master_sub->va; i!=NULL; i=i->next ) {
				if ( i->ptr == NULL ) continue;
				EventVA *event = (EventVA *) i->ptr;
				EventVA *copy_va = (EventVA *) malloc( sizeof( EventVA ) );
				memcpy( copy_va, event, sizeof( EventVA ) );
				addItem( &copy_sub->va, copy_va );
			}
			break;
		case OtherwiseOccurrence:
			for ( listItem *i = master_sub->va; i!=NULL; i=i->next ) {
				if ( i->ptr == NULL ) continue;
				Occurrence *occurrence = i->ptr;
				Occurrence *copy_va = newOccurrence( occurrence->type );
				duplicateNarrative( occurrence, copy_va );
				addItem( &copy_sub->va, copy_va );
			}
			break;
		case ActionOccurrence:
			for ( listItem *i = master_sub->va; i!=NULL; i=i->next ) {
				if ( i->ptr == NULL ) continue;
				ActionVA *action = (ActionVA *) i->ptr;
				ActionVA *copy_va = (ActionVA *) malloc( sizeof( ActionVA ) );
				memcpy( copy_va, action, sizeof( ActionVA ) );
				addItem( &copy_sub->va, copy_va );
			}
			break;
		case ThenOccurrence:
			break;

		}
		reorderListItem( &copy_sub->va );
		copy_sub->sub.n = NULL;
		copy_sub->sub.num = 0;
		duplicateNarrative( master_sub, copy_sub );
	}
	reorderListItem( &copy->sub.n );
}

static void
search_and_register_init( Narrative * instance )
{
	for ( listItem *i = instance->root.sub.n; i!=NULL; i=i->next )
	{
		Occurrence *occurrence = (Occurrence *) i->ptr;
		switch ( occurrence->type ) {
		case EventOccurrence:
			; EventVA *event = (EventVA *) occurrence->va->ptr;
			if ( event->type == InitEvent ) {
				occurrence->registered = 1;
				addItem( &instance->frame.events, occurrence );
			}
			break;
		default:
			break;
		}
	}
	// in case there is no init, trigger manually the next frame
	if ( !(instance->frame.events) ) {
		instance->root.registered = 1;
		addItem( &instance->frame.events, &instance->root );
	}
}

/*---------------------------------------------------------------------------
	deactivateNarrative
---------------------------------------------------------------------------*/
void
deactivateNarrative( Entity *entity, Narrative *narrative )
{
	registryEntry *entry = registryLookup( &narrative->instances, entity );
	Narrative *instance = (Narrative *) entry->value;
	if ( instance == narrative ) {
		for ( listItem *i = narrative->frame.events; i!=NULL; i=i->next )
			((Occurrence *) i->ptr )->registered = 0;
		freeListItem( &narrative->frame.events );
		for ( listItem *i = narrative->frame.actions; i!=NULL; i=i->next )
			((Occurrence *) i->ptr )->registered = 0;
		freeListItem( &narrative->frame.actions );
		for ( listItem *i = narrative->frame.then; i!=NULL; i=i->next )
			((Occurrence *) i->ptr )->registered = 0;
		freeListItem( &narrative->frame.then );

		narrative->deactivate = 0;
		narrative->root.registered = 0;
		narrative->assigned = 0;
		freeVariables( &narrative->variables );
	} else {
		instance->name = NULL;
		freeNarrative( instance );
	}
	registryDeregister( &narrative->instances, NULL, instance );
}

/*---------------------------------------------------------------------------
	narrative_active, any_narrative_active
---------------------------------------------------------------------------*/
int
narrative_active( Entity *entity, Narrative *narrative )
{
	if ((entity) && (narrative)) {
		if (( registryLookup( &narrative->instances, entity ) )) {
			return 1;
		}
	}
	else {
		for ( listItem *i = CN.context->narrative.registered; i!=NULL; i=i->next ) {
			Narrative *n = (Narrative *) i->ptr;
			if (( n->instances.value )) return 1;
		}
	}
	return 0;
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
			case OtherwiseOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					freeOccurrence( i->ptr );
				}
				break;
			case ConditionOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					ConditionVA *condition = (ConditionVA *) i->ptr;
					free( condition->format );
				}
				break;
			case EventOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					EventVA *event = (EventVA *) sub->va->ptr;
					free( event->identifier );
					free( event->format );
				}
				break;
			case ActionOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					ActionVA *action = (ActionVA *) sub->va->ptr;
					freeListItem( &action->instructions );
				}
				break;
			case ThenOccurrence:
				break;
			}
			freeListItem( &sub->va );
			clipListItem( list, i, last_i, next_i );
			occurrence->sub.num--;
		}
		else last_i = i;
#if 0
		sub_cut = sub_cut || (( sub->type == ActionOccurrence ) && ((ActionVA *) sub->va->ptr )->exit );
#endif
	}
	if ( cut ) {
		return 1;
	}
	if ( occurrence->sub.num == 0 ) {
		switch ( occurrence->type ) {
		case OtherwiseOccurrence:
		case ConditionOccurrence:
		case EventOccurrence:
		case ThenOccurrence:
			return 1;
			break;
		case ActionOccurrence:
			for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
				ActionVA *action = (ActionVA *) i->ptr;
				if ( action->instructions != NULL )
					return 0;
			}
			return 1;
		}
	}
	return 0;
}

