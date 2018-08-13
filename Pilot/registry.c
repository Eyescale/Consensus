#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "registry.h"

static registryEntry *freeRegistryItemList = NULL;

// #define DEBUG

/*---------------------------------------------------------------------------
	compare		- local utility
---------------------------------------------------------------------------*/
static int
compare( RegistryType type, registryEntry *entry, void *name )
{
	switch ( type ) {
	case IndexedByName:
		return strcmp( entry->index.name, (char *) name );
	case IndexedByNumber:
		return ( entry->index.value - *(int *) name );
	case IndexedByAddress:
		return ( (intptr_t) entry->index.address - (intptr_t) name );
	}
}

/*---------------------------------------------------------------------------
	newRegistryEntry, freeRegistryEntry	- local
---------------------------------------------------------------------------*/
registryEntry *
newRegistryEntry( RegistryType type, void *name, void *value )
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
	switch ( type ) {
	case IndexedByNumber:
		r->index.value = *(int *) name;
		break;
	default:
		r->index.address = name;
	}
        r->value = value;
        return r;
}

void
freeRegistryEntry( RegistryType type, registryEntry *r )
{
	r->value = NULL;
	switch ( type ) {
	case IndexedByNumber:
		r->index.value = 0;
		break;
	default:
		r->index.address = NULL;
		break;
	}
        r->next = freeRegistryItemList;
        freeRegistryItemList = r;
}

/*---------------------------------------------------------------------------
	newRegistry, freeRegistry
---------------------------------------------------------------------------*/
Registry *
newRegistry( RegistryType type )
{
	return newRegistryEntry( IndexedByNumber, &type, NULL );
}

void
emptyRegistry( Registry *registry )
{
	RegistryType type = RTYPE( registry );
	for ( registryEntry *r=registry->value, *next_r; r!=NULL; r=next_r )
	{
		next_r = r->next;
		freeRegistryEntry( type, r );
	}
	registry->value = NULL;
}

void
freeRegistry( Registry **registry )
{
	emptyRegistry( *registry );
	freeRegistryEntry( IndexedByNumber, *registry );
	*registry = NULL;
}

/*---------------------------------------------------------------------------
	registryRegister
---------------------------------------------------------------------------*/
registryEntry *
registryRegister( Registry *registry, void *name, void *value )
{
	RegistryType type = RTYPE( registry );
	if (( type == IndexedByName ) && ( name == NULL ))
		return NULL;

        registryEntry *entry, *r, *last_r = NULL;
        for ( r=registry->value; r!=NULL; r=r->next )
        {
		int comparison = compare( type, r, name );
                if ( comparison < 0 )
                {
                        last_r = r;
                        continue;
                }
                if ( comparison == 0 )
                {
#ifdef DEBUG
			fprintf( stderr, "registryRegister: Warning: entry already registered\n" );
#endif
                        return r;
                }
                break;
        }
        entry = newRegistryEntry( type, name, value );
        if (( last_r ))
        {
                last_r->next = entry;
                entry->next = r;
        }
        else {
		entry->next = registry->value;
		registry->value = entry;
	}
	return entry;
}

/*---------------------------------------------------------------------------
	registryDeregister
---------------------------------------------------------------------------*/
void
registryDeregister( Registry *registry, void *name, ... )
{
	RegistryType type = RTYPE( registry );
        registryEntry *r, *r_next, *r_last = NULL;

        for ( r=registry->value; r!=NULL; r=r_next )
        {
		r_next = r->next;
		int comparison;
		if (( name )) {
			comparison = compare( type, r, name );
			if ( comparison > 0 ) return;
		}
		else {
			va_list ap;
			va_start( ap, name );
			comparison = !( r->value == va_arg( ap, void * ) );
			va_end( ap );
		}
		if ( !comparison )
                {
                        if ( r_last == NULL )
                                registry->value = r_next;
                        else
                                r_last->next = r_next;

                        freeRegistryEntry( type, r );
                        return;
                }
                r_last = r;
        }
}

/*---------------------------------------------------------------------------
	registryLookup
---------------------------------------------------------------------------*/
registryEntry *
registryLookup( Registry *registry, void *name, ... )
{
	RegistryType type = RTYPE( registry );
	if (( type == IndexedByName ) && ( name == NULL ))
		return NULL;

        for ( registryEntry *r=registry->value; r!=NULL; r=r->next )
        {
		int comparison;
		if (( name )) {
			comparison = compare( type, r, name );
			if ( comparison > 0 ) return NULL;
		}
		else {
			va_list ap;
			va_start( ap, name );
			comparison = !( r->value == va_arg( ap, void * ) );
			va_end( ap );
		}
                if ( !comparison ) return r;
        }
        return NULL;
}

/*---------------------------------------------------------------------------
	registryDuplicate
---------------------------------------------------------------------------*/
Registry *registryDuplicate( Registry *registry )
{
	int type = RTYPE( registry );
	Registry *copy = newRegistry( type );

	// copy entries in the same order
	registryEntry *j = registry->value;
	switch ( type ) {
	case IndexedByNumber:
		copy->value = newRegistryEntry( type, &j->index.value, j->value );
		break;
	default:
		copy->value = newRegistryEntry( type, j->index.address, j->value );
		break;
	}
	registryEntry *jlast = copy->value, *jnext;
	for ( j = j->next; j!=NULL; j=j->next )
	{
		switch ( type ) {
		case IndexedByNumber:
			jnext = newRegistryEntry( type, &j->index.value, j->value );
			break;
		default:
			jnext = newRegistryEntry( type, j->index.address, j->value );
			break;
		}
		jlast->next = jnext;
		jlast = jnext;
	}

	return copy;
}

