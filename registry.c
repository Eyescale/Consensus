#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"

static registryEntry *freeRegistryItemList = NULL;

#define NAME 0
#define INDEX 1
#define ADDRESS 2

#define COMPARE	\
	(( type == NAME ) ? strcmp( r->identifier.name, name ) : \
	 ( type == INDEX ) ? r->identifier.index - *(int *) name : \
	 (int) ( (intptr_t) r->identifier.address - (intptr_t) name ))

/*---------------------------------------------------------------------------
	newRegistryEntry
---------------------------------------------------------------------------*/
static registryEntry *
newRegistryEntry( int type, void *name, void *value )
{
        registryEntry *r;
        if ( freeRegistryItemList == NULL )
                r = calloc( 1, sizeof( registryEntry ) );
        else
        {
                r = freeRegistryItemList;
                freeRegistryItemList = r->next;
		r->next = NULL;
        }
	if ( type == INDEX ) r->identifier.index = *(int *) name;
	else r->identifier.address = name;
        r->value = value;
        return r;
}

/*---------------------------------------------------------------------------
	registerBy
---------------------------------------------------------------------------*/
static registryEntry *
registerBy( int type, Registry *registry, void *name, void *value )
{
	if (( type == NAME ) && ( name == NULL ))
		return NULL;

        registryEntry *entry, *r, *last_r = NULL;
        for ( r=*registry; r!=NULL; r=r->next )
        {
		int comparison = COMPARE;
                if ( comparison < 0 )
                {
                        last_r = r;
                        continue;
                }
                if ( comparison == 0 )
                {
#ifdef DEBUG
			output( Debug, "registerBy: Warning: entry already registered" );
#endif
                        return r;
                }
                break;
        }
        entry = newRegistryEntry( type, name, value );
        if ( last_r == NULL )
        {
                entry->next = *registry;
                *registry = entry;
        }
        else
        {
                last_r->next = entry;
                entry->next = r;
        }
	return entry;
}

/*---------------------------------------------------------------------------
	deregisterBy
---------------------------------------------------------------------------*/
static void
deregisterBy( int type, Registry *registry, void *name )
{
        registryEntry *r, *r_next, *r_last = NULL;
        for ( r=*registry; r!=NULL; r=r_next )
        {
		r_next = r->next;
		int comparison = COMPARE;
                if ( !comparison )
                {
                        if ( r_last == NULL )
                                *registry = r_next;
                        else
                                r_last->next = r_next;

                        freeRegistryItem( r );
                        return;
                }
                r_last = r;
        }
}

/*---------------------------------------------------------------------------
	lookupBy
---------------------------------------------------------------------------*/
static registryEntry *
lookupBy( int type, Registry registry, void *name )
{
	if (( registry == NULL ) || (( type == NAME ) && ( name == NULL )))
		return NULL;

        for ( registryEntry *r=registry; r!=NULL; r=r->next )
        {
		int comparison = COMPARE;
                if ( comparison > 0 )
                        return NULL;
                if ( comparison == 0 )
                        return r;
        }
        return NULL;
}

/*---------------------------------------------------------------------------
	registerByName
---------------------------------------------------------------------------*/
registryEntry *
registerByName( Registry *registry, char *name, void *value )
{
	return registerBy( NAME, registry, name, value );
}

/*---------------------------------------------------------------------------
	lookupByName
---------------------------------------------------------------------------*/
registryEntry *
lookupByName( Registry registry, char *name )
{
	return lookupBy( NAME, registry, name );
}

/*---------------------------------------------------------------------------
	registerByIndex
---------------------------------------------------------------------------*/
registryEntry *
registerByIndex( Registry *registry, int ndx, void *value )
{
	return registerBy( INDEX, registry, &ndx, value );
}

/*---------------------------------------------------------------------------
	lookupByIndex
---------------------------------------------------------------------------*/
registryEntry *
lookupByIndex( Registry registry, int ndx )
{
	return lookupBy( INDEX, registry, &ndx );
}

/*---------------------------------------------------------------------------
	registerByAddress
---------------------------------------------------------------------------*/
registryEntry *
registerByAddress( Registry *registry, void *address, void *value )
{
	return registerBy( ADDRESS, registry, address, value );
}

/*---------------------------------------------------------------------------
	lookupByAddress
---------------------------------------------------------------------------*/
registryEntry *
lookupByAddress( Registry registry, void *address )
{
	return lookupBy( ADDRESS, registry, address );
}

/*---------------------------------------------------------------------------
	deregisterByAddress
---------------------------------------------------------------------------*/
void
deregisterByAddress( Registry *registry, void *address )
{
	deregisterBy( ADDRESS, registry, address );
}

/*---------------------------------------------------------------------------
	lookupByValue
---------------------------------------------------------------------------*/
registryEntry *
lookupByValue( Registry registry, void *value )
{
        registryEntry *r;

        for ( r=registry; r!=NULL; r=r->next )
        {
                if ( r->value == value )
                        return r;
        }
        return NULL;
}

/*---------------------------------------------------------------------------
	deregisterByValue
---------------------------------------------------------------------------*/
void
deregisterByValue( Registry *registry, void *value )
{
        registryEntry *r, *r_next, *r_last = NULL;
        for ( r=*registry; r!=NULL; r=r_next )
        {
		r_next = r->next;
                if ( r->value == value )
                {
                        if ( r_last == NULL )
                                *registry = r_next;
                        else
                                r_last->next = r_next;

                        freeRegistryItem( r );
                        return;
                }
                r_last = r;
        }
}

/*---------------------------------------------------------------------------
	newRegistryItem
---------------------------------------------------------------------------*/
registryEntry *
newRegistryItem( void *identifier, void *value )
{
	return newRegistryEntry( ADDRESS, identifier, value );
}

/*---------------------------------------------------------------------------
	freeRegistryItem
---------------------------------------------------------------------------*/
void freeRegistryItem( registryEntry *r )
{
	r->value = NULL;
	r->identifier.address = NULL;
        r->next = freeRegistryItemList;
        freeRegistryItemList = r;
}


/*---------------------------------------------------------------------------
	freeRegistry
---------------------------------------------------------------------------*/
void freeRegistry( Registry *registry )
{
	registryEntry *i, *next_i;
	for ( i=*registry; i!=NULL; i=next_i )
	{
		next_i = i->next;
		freeRegistryItem( i );
	}
	*registry = NULL;
}

/*---------------------------------------------------------------------------
	copyRegistry
---------------------------------------------------------------------------*/
Registry copyRegistry( Registry registry )
{
	// copy entries in the same order
	registryEntry *j = registry;
	registry = newRegistryItem( j->identifier.address, j->value );
	registryEntry *jlast = registry;
	for ( j = j->next; j!=NULL; j=j->next ) {
		registryEntry *jnext = newRegistryItem( j->identifier.address, j->value );
		jlast->next = jnext;
		jlast = jnext;
	}
	return registry;
}
