#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "database.h"

static Entity *freeEntityList = NULL;

Entity *
newEntity( Entity *source, Entity *medium, Entity *target, int twist )
{
	Entity *e;

	if ( freeEntityList == NULL ) 
		e = calloc( 1, sizeof( Entity ) );
	else {
		e = freeEntityList;
		freeEntityList = freeEntityList->next;
		e->next = NULL;
		e->state = 0;
	}
	e->sub[0] = source;
	e->sub[1] = medium;
	e->sub[2] = target;
	e->twist = twist;

	if (( source == NULL ) && ( target == NULL )) {
		return e;
	}
	if ( source != NULL ) {
		listItem *item = newItem(e);
		item->next = source->as_sub[0];
		source->as_sub[0] = item;
	}
	if ( medium != NULL ) {
		listItem *item = newItem(e);
		item->next = medium->as_sub[1];
		medium->as_sub[1] = item;
	}
	if ( target != NULL ) {
		listItem *item = newItem(e);
		item->next = target->as_sub[2];
		target->as_sub[2] = item;
	}
	return e;
}

void
freeEntity( Entity *entity )
{
	if ( entity->state == -1 )
		return;

	// remove entity from the as_sub lists of its subs
	for ( int i=0; i< 3; i++ )
	{
		Entity *sub = entity->sub[ i ];
		if (( sub == NULL ) || ( sub->state == -1 ))
			continue;

		removeItem( &sub->as_sub[ i ], entity );
	}

	entity->state = -1;
	entity->next = freeEntityList;
	freeEntityList = entity;
}
