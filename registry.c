#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "registry.h"

static registryEntry *freeRegistryItemList = NULL;

/*---------------------------------------------------------------------------
	lookupByName
---------------------------------------------------------------------------*/
registryEntry *
lookupByName( Registry registry, char *name )
{
	if ( name == NULL ) return NULL;
	if ( registry == NULL ) return NULL;

        for ( registryEntry *r=registry; r!=NULL; r=r->next )
        {
                int comparison = strcmp( r->identifier, name );
                if ( comparison > 0 )
                        return NULL;
                else if ( comparison == 0 )
                        return r;
        }
        return NULL;
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
	registerByAddress
---------------------------------------------------------------------------*/
registryEntry *
registerByAddress( Registry *registry, void *address, void *value )
{
        registryEntry *entry, *r, *last_r = NULL;
        for ( r=*registry; r!=NULL; r=r->next )
        {
		intptr_t comparison = (intptr_t) r->identifier - (intptr_t) address;
                if ( comparison > 0 )
                	break;
                else if ( comparison == 0 ) {
#ifdef DEBUG
			output( Debug, "registerByAddress: Warning: entry already registered" );
#endif
                        return NULL;
                }
                last_r = r;
        }
        entry = newRegistryItem( address, value );
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
	lookupByAddress
---------------------------------------------------------------------------*/
registryEntry *
lookupByAddress( Registry registry, void *address )
{
        registryEntry *r;

        for ( r=registry; r!=NULL; r=r->next )
        {
		intptr_t comparison = (intptr_t) r->identifier - (intptr_t) address;
                if ( comparison > 0 )
                        return NULL;
                else if ( comparison == 0 )
                        return r;
        }
        return NULL;
}

/*---------------------------------------------------------------------------
	registerByName
---------------------------------------------------------------------------*/
registryEntry *
registerByName( Registry *registry, char *name, void *address )
{
	if ( name == NULL ) return NULL;

        registryEntry *entry, *r, *last_r = NULL;
        for ( r=*registry; r!=NULL; r=r->next )
        {
                int comparison = strcmp( r->identifier, name );
                if ( comparison < 0 )
                {
                        last_r = r;
                        continue;
                }
                if ( comparison == 0 )
                {
#ifdef DEBUG
			output( Debug, "registerByName: Warning: entry already registered" );
#endif
                        return r;
                }
                break;
        }
        entry = newRegistryItem( name, address );
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
#ifdef DEBUG
	output( Debug, "registerByName: %s, next: %0x from %0x", name, entry->next, *registry );
#endif

	return entry;
}

/*---------------------------------------------------------------------------
	deregisterByAddress
---------------------------------------------------------------------------*/
void
deregisterByAddress( Registry *registry, void *address )
{
        registryEntry *r, *r_next, *r_last = NULL;
        for ( r=*registry; r!=NULL; r=r_next )
        {
		r_next = r->next;
                if ( r->identifier == address )
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
        registryEntry *r;
        if ( freeRegistryItemList == NULL )
                r = calloc( 1, sizeof( registryEntry ) );
        else
        {
                r = freeRegistryItemList;
                freeRegistryItemList = r->next;
		r->next = NULL;
        }
        r->identifier = identifier;
        r->value = value;
        return r;
}

/*---------------------------------------------------------------------------
	freeRegistryItem
---------------------------------------------------------------------------*/
void freeRegistryItem( registryEntry *r )
{
	r->value = NULL;
	r->identifier = NULL;
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
	registry = newRegistryItem( j->identifier, j->value );
	registryEntry *jlast = registry;
	for ( j = j->next; j!=NULL; j=j->next ) {
		registryEntry *jnext = newRegistryItem( j->identifier, j->value );
		jlast->next = jnext;
		jlast = jnext;
	}
	return registry;
}
