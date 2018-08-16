#include <stdio.h>
#include <stdlib.h>

#include "system.h"

/*---------------------------------------------------------------------------
	newSystem, freeSystem
---------------------------------------------------------------------------*/
System *
newSystem( void )
{
	return calloc( 1, sizeof(System) );
}
static void SystemFree( System * );
void
freeSystem( System *s )
{
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next )
		SystemFree( i->ptr );
	freeListItem( &s->subsystems );
	free( s );
}
static void freeSystemRegistry( listItem ** );
static void
SystemFree( System *s )
{
	freeSystemRegistry( &s->changes );
	freeSystemRegistry( &s->subscriptions );
	freeListItem( &s->log );
	freeListItem( &s->frontbuffer );
	freeListItem( &s->backbuffer );
	free( s->data );
	free( s );
}
static void
freeSystemRegistry( listItem **registry )
{
	for ( listItem *j=*registry; j!=NULL; j=j->next ) {
		Pair *pair = j->ptr;
		freeListItem((listItem **) &pair->value );
		freePair( pair );
	}
	freeListItem( registry );
}

/*---------------------------------------------------------------------------
	SystemFind, SystemAdd, newSubscription
---------------------------------------------------------------------------*/
System *
SystemFind( System *s, int id )
{
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next ) {
		System *subsystem = i->ptr;
		if ( subsystem->id == id )
			return subsystem;
	}
	return NULL;
}
System *
SystemAdd( System *s, int id, Narrative init )
{
	System *subsystem = newSystem();
	subsystem->operator = s;
	subsystem->id = id;
	if ( !init( subsystem ) ) {
		freeSystem( subsystem );
		return NULL;
	}
	addItem( &s->subsystems, subsystem );
	return subsystem;
}
Pair *
newSubscription( System *s, System *cosystem )
{
	for ( listItem *i=s->subscriptions; i!=NULL; i=i->next ) {
		Pair *subscription = i->ptr;
		if ( subscription->name == cosystem )
			return subscription;
	}
	Pair *subscription = newPair( cosystem, NULL );
	addItem( &s->subscriptions, subscription );
	return subscription;
}

/*---------------------------------------------------------------------------
	SystemFrame
---------------------------------------------------------------------------*/
static void SystemUpdateNotifications( System * );

int
SystemFrame( System *s )
{
	/* propagate frame notification to all subsystems
	*/
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next ) {
		System *subsystem = i->ptr;
		if (( subsystem->frame ))
			((Narrative *) subsystem->frame)( subsystem );
	}
	/* have subsystems update their logs, according to their interests
	*/
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next ) {
		System *subsystem = i->ptr;
		if (( subsystem->sync )) {
			SystemUpdateNotifications( subsystem );
			((Narrative *) subsystem->sync)( subsystem );
		}
	}
	/* swap subsystems data buffers
	*/
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next ) {
		System *subsystem = i->ptr;
		freeListItem( &subsystem->frontbuffer );
		subsystem->frontbuffer = subsystem->backbuffer;
		subsystem->backbuffer = NULL;
	}
	/* s is alive so long as at least one of its subsystems
	   has something to process
	*/
	for ( listItem *i=s->subsystems; i!=NULL; i=i->next ) {
		System *subsystem = i->ptr;
		if (( subsystem->frontbuffer ))
			return 1;
	}
	return 0;
}

static void
SystemUpdateNotifications( System *s )
{
	freeListItem( &s->log );
	freeSystemRegistry( &s->changes );
	for ( listItem *j=s->subscriptions; j!=NULL; j=j->next ) {
		Pair *subscription = j->ptr;
		System *cosystem = subscription->name;
		Pair *notification = NULL;
		for ( listItem *k = subscription->value; k!=NULL; k=k->next ) {
			void *field = k->ptr;
			if (( lookupIfThere( cosystem->backbuffer, field ) )) {
				if ( notification == NULL )
					notification = newPair( cosystem, NULL );
				addIfNotThere((listItem **) &notification->value, field );
			}
		}
		if (( notification )) addItem( &s->changes, notification );
	}
}
