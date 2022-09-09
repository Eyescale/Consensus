#ifndef SYSTEM_H
#define SYSTEM_H

#include "list.h"
#include "pair.h"
#include "macros_private.h"

/*---------------------------------------------------------------------------
	System
---------------------------------------------------------------------------*/
typedef struct _system {
	int id;
	void *data;
	void *init;
	void *frame;
	void *sync;
	listItem *subsystems;
	listItem *backbuffer, *frontbuffer;
	struct _system *operator;
	listItem *subscriptions;
	listItem *changes;
} System;

typedef int Narrative( System * );

System * newSystem( void );
void freeSystem( System * );
System * SystemFind( System *, int id );
System * SystemAdd( System *, int id, Narrative init );
Pair * newSubscription( System *, System * );
int SystemFrame( System * );

/*---------------------------------------------------------------------------
	build macros
---------------------------------------------------------------------------*/
#define CNSystemBegin( s ) \
	System *s = newSystem(); \
	{ \
		System *this = s;
#define CNSubSystem( id, init ) \
		SystemAdd( this, id, init );
#define CNSystemEnd( ) \
	}
#define CNSystemFrame( s ) \
	SystemFrame( s )
#define CNSystemFree( s ) \
	freeSystem( s )
	
/*---------------------------------------------------------------------------
	subscription macros
---------------------------------------------------------------------------*/
#define CNSubscriptionBegin( id ) \
	{ \
		System *cosystem = SystemFind( this->operator, id ); \
		Pair *subscription = newSubscription( this, cosystem ); \
		void *handle = cosystem->data;
#define CNRegister( type, field ) \
		addIfNotThere((listItem **) &subscription->value, &((type *)handle)->field );
#define CNSubscriptionEnd \
	}

/*---------------------------------------------------------------------------
	frame macros
---------------------------------------------------------------------------*/
#define CNFrameBegin( type ) \
	type *data = this->data; \
	static int init = 1; \
	{

#define CNOn( field ) \
	( lookupIfThere( this->frontbuffer, &data->field ))

#define CNOni( field, value ) \
	( CNOn( field ) && ((int) data->field == value ))

#define CNIni( field, value ) \
	( data->field == value )

#define CNDoi( field, value ) \
	data->field = value; \
	addIfNotThere( &this->backbuffer, &data->field );

#define CNFrameEnd \
	} \
	return 1;

/*---------------------------------------------------------------------------
	sync macros
---------------------------------------------------------------------------*/
#define CNSyncBegin( type ) \
	type *data = this->data; \
	for ( listItem *i=this->changes; i!=NULL; i=i->next ) { \
		Pair *notification = i->ptr; \
		System *cosystem = notification->name; \
		listItem *changes = notification->value; \
		void *handle = cosystem->data; \
		if ( 0 ) {
#define CNOnSystemChange( ID ) \
		} else if ( cosystem->id == ID ) {
#define CNOnFieldChange( type, field ) \
			} else if ( lookupIfThere( changes, &CNFieldValue(type,field) ) ) {
#define CNFieldValue( type, field ) \
				(((type *)handle)->field)
#define CNSyncEnd \
		} \
	} \
	return 1;


#endif	// SYSTEM_H
