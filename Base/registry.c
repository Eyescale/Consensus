#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "registry.h"

//===========================================================================
//	public interface
//===========================================================================
static int compare( RegistryType, Pair *entry, void *name );

Registry *
newRegistry( RegistryType type )
{
	union { RegistryType value; char *ptr; } icast;
	icast.value = type;
	return (Registry *) newPair( icast.ptr, NULL );
}
void
freeRegistry( Registry *registry, freeRegistryCB callback )
{
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		if (( callback )) callback( registry, entry );
		else free( entry->name );
		freePair( entry );
	}
	freeListItem( &registry->entries );
	freePair((Pair *) registry );
}
void
nopCB( Registry *registry, Pair *entry )
{
}

Pair *
newRegistryEntry( RegistryType type, void *name, void *value )
{
	union { int value; char *ptr; } icast;
	switch ( type ) {
		case IndexedByNumber:
			icast.value = *(int *) name;
			break;
		default:
			icast.ptr = name;
	}
	return newPair( icast.ptr, value );
}
Pair *
registryRegister( Registry *registry, void *name, void *value )
{
	RegistryType type = registry->type;
	if (( type == IndexedByName ) && ( name == NULL ))
		return NULL;

	Pair *r;
        listItem *i, *last_i=NULL;
	for ( i=registry->entries; i!=NULL; i=i->next ) {
		r = i->ptr;
		int comparison = compare( type, r, name );
                if ( comparison < 0 ) {
                        last_i = i;
                        continue;
                }
                if ( comparison == 0 ) {
#ifdef DEBUG
			fprintf( stderr, "registryRegister: Warning: entry already registered\n" );
#endif
                        return r;
                }
                break;
        }
        r = newRegistryEntry( type, name, value );
	listItem *j = newItem( r );
        if (( last_i )) {
                last_i->next = j;
                j->next = i;
        }
        else {
		j->next = registry->entries;
		registry->entries = j;
	}
	return r;
}
void
registryDeregister( Registry *registry, void *name, ... )
{
	RegistryType type = registry->type;
	listItem *i, *next_i, *last_i=NULL;

	for ( i=registry->entries; i!=NULL; last_i=i, i=next_i ) {
		Pair *r = i->ptr;
		next_i = i->next;
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
		if ( !comparison ) {
                        if (( last_i )) last_i->next = next_i;
                        else registry->entries = next_i;

			freeItem( i );
                        freePair( r );
                        return;
                }
        }
}
Pair *
registryLookup( Registry *registry, void *name, ... )
{
	if (( name )) {
		RegistryType type = registry->type;
        	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
			Pair *r = i->ptr;
			int comparison = compare( type, r, name );
			if ( comparison > 0 ) return NULL;
			else if ( !comparison ) return r;
		}
	}
	else {
		va_list ap;
		va_start( ap, name );
		void *value = va_arg( ap, void * );
		va_end( ap );
	        for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
			Pair *r = i->ptr;
			if ( r->value == value ) return r;
        	}
	}
        return NULL;
}
Registry *
registryDuplicate( Registry *registry )
{
	int type = registry->type;
	Registry *copy = newRegistry( type );

	/* copy entries in the same order
	*/
	listItem *j = registry->entries;
	if ( j == NULL ) return copy;

	Pair *r = j->ptr;
	r = ( type == IndexedByNumber ) ?
		newRegistryEntry( type, &r->name, r->value ) :
		newRegistryEntry( type, r->name, r->value );

	listItem *jlast = copy->entries = newItem( r );
	for ( j = j->next; j!=NULL; j=j->next ) {
		r = j->ptr;
		r = ( type == IndexedByNumber ) ?
			newRegistryEntry( type, &r->name, r->value ) :
			newRegistryEntry( type, r->name, r->value );

		listItem *jnext = newItem( r );
		jlast->next = jnext;
		jlast = jnext;
	}
	return copy;
}

//===========================================================================
//	compare
//===========================================================================
static int
compare( RegistryType type, Pair *entry, void *name )
{
	switch ( type ) {
	case IndexedByName:
		return strcmp( entry->name, (char *) name );
	case IndexedByNumber:
		return ( (int) entry->name - *(int *) name );
	case IndexedByAddress:
		return ( (uintptr_t) entry->name - (uintptr_t) name );
	}
}

