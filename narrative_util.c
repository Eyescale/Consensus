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
		output( Debug, "addNarrative(): first narrative ever - %s()", name );
#endif
		entry = newRegistryItem( name, narrative );
		va->value = newRegistryItem( entity, entry );
	} else {
		entry = lookupByAddress( va->value, entity );
		if ( entry == NULL ) {
#ifdef DEBUG
			output( Debug, "addNarrative(): first entity's narrative assignment - %s()", name );
#endif
			entry = newRegistryItem( name, narrative );
			registerByAddress((Registry *) &va->value, entity, entry );
		} else {
			Registry *narratives = (Registry *) &entry->value;
			entry = lookupByName( *narratives, name );
			if ( entry == NULL ) {
#ifdef DEBUG
				output( Debug, "addNarrative(): adding new entity's narrative - %s()", name );
#endif
				entry = registerByName( narratives, name, narrative );
			} else {
#ifdef DEBUG
				output( Debug, "addNarrative(): replacing entity's narrative - %s()", name );
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
				Occurrence *copy_va = (Occurrence *) calloc( 1, sizeof( Occurrence ) );
				duplicateNarrative( (Occurrence *) i->ptr, copy_va );
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
			case OtherwiseOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					freeOccurrence( i->ptr );
				}
				break;
			case ConditionOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					ConditionVA *condition = (ConditionVA *) i->ptr;
					free( condition->expression );
				}
				break;
			case EventOccurrence:
				for ( listItem *i = sub->va; i!=NULL; i=i->next ) {
					EventVA *event = (EventVA *) sub->va->ptr;
					free( event->identifier.name );
					free( event->expression );
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
/*---------------------------------------------------------------------------
	freeNarrative	- recursive
---------------------------------------------------------------------------*/
void
freeOccurrence( Occurrence *occurrence )
{
	if ( occurrence == NULL ) return;
	for ( listItem *i = occurrence->sub.n; i!=NULL; i=i->next )
	{
		freeOccurrence( i->ptr );
		free( i->ptr );
	}
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
			free( condition->expression );
			free( condition );
		}
		break;
	case EventOccurrence:
		for ( listItem *i = occurrence->va; i!=NULL; i=i->next ) {
			if ( i->ptr == NULL ) continue;
			EventVA *event = (EventVA *) i->ptr;
			free( event->identifier.name );
			free( event->expression );
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

void
freeNarrative( Narrative *narrative )
{
	freeOccurrence( &narrative->root );
	freeVariables( &narrative->variables );
	free( narrative->name );
	free( narrative );
}

