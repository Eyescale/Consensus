#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "registry.h"
#include "string_util.h"

static int compare( RegistryType, Pair *entry, void *name );

//===========================================================================
//	public interface
//===========================================================================
Registry *
newRegistry( RegistryType type ) {
	Registry *registry = (Registry *) newPair( NULL, NULL );
	registry->type.value[ 0 ] = type;
	return registry; }

void
freeRegistry( Registry *registry, freeRegistryCB cb ) {
	RegistryType type = registry->type.value[ 0 ];
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		if (( cb )) cb( registry, entry );
		else if ( type==IndexedByName ) free( entry->name );
		freePair( entry ); }
	freeListItem( &registry->entries );
	freePair((Pair *) registry ); }

Pair *
newRegistryEntry( RegistryType type, void *name, void *value ) {
	if ( type==IndexedByNumber )
		return newPair( cast_ptr( *(int *)name ), value );
	else return newPair( name, value ); }

Pair *
registryRegister( Registry *registry, void *name, void *value ) {
	RegistryType type = registry->type.value[ 0 ];
	switch ( type ) {
	case IndexedByName:
	case IndexedByNameRef:
		if ( name == NULL ) return NULL;
	default:
		break; }

	Pair *r;
        listItem *i, *last_i=NULL;
	for ( i=registry->entries; i!=NULL; i=i->next ) {
		r = i->ptr;
		int comparison = compare( type, r, name );
                if ( comparison < 0 ) {
                        last_i = i;
                        continue; }
                if ( comparison==0 ) {
#ifdef DEBUG
			fprintf( stderr, "registryRegister: Warning: entry already registered\n" );
#endif
                        return r; }
                break; }
        r = newRegistryEntry( type, name, value );
	listItem *j = newItem( r );
        if (( last_i )) {
                last_i->next = j;
                j->next = i; }
        else {
		j->next = registry->entries;
		registry->entries = j; }
	return r; }

void
registryCBDeregister( Registry *registry, freeRegistryCB cb, void *name ) {
	Pair *r;
	RegistryType type = registry->type.value[ 0 ];
	listItem *i, *next_i, *last_i=NULL;
	for ( i=registry->entries; i!=NULL; i=next_i ) {
		r = i->ptr;
		next_i = i->next;
		int comparison;
		comparison = compare( type, r, name );
		if ( comparison > 0 ) return;
		else if ( comparison ) last_i = i;
		else {
			if ((cb)) cb( registry, r );
			freeItem( i );
			freePair( r );
			if (( last_i )) last_i->next = next_i;
			else registry->entries = next_i; } } }

static freeRegistryCB free_CB;
void
registryDeregister( Registry *registry, void *name, ... ) {
	RegistryType type = registry->type.value[ 0 ];
	freeRegistryCB *cb = ( type==IndexedByName ? free_CB : NULL );
	if (( name ))
		registryCBDeregister( registry, cb, name );
	else {
		va_list ap;
		va_start( ap, name );
		void *value = va_arg( ap, void * );
		va_end( ap );
		Pair *r;
		listItem *i, *next_i, *last_i=NULL;
		for ( i=registry->entries; i!=NULL; i=next_i ) {
			r = i->ptr;
			next_i = i->next;
			if ( r->value!=value ) last_i=i;
			else {
				if ((cb)) cb( registry, r );
				freeItem( i );
				freePair( r );
				if (( last_i )) last_i->next = next_i;
				else registry->entries = next_i; } } } }

static void free_CB( Registry *registry, Pair *entry ) {
	free( entry->name ); }

Pair *
registryLookup( Registry *registry, void *name, ... ) {
	if (( name )) {
		RegistryType type = registry->type.value[ 0 ];
        	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
			Pair *r = i->ptr;
			int comparison = compare( type, r, name );
			if ( comparison > 0 ) return NULL;
			else if ( !comparison ) return r; } }
	else {
		va_list ap;
		va_start( ap, name );
		void *value = va_arg( ap, void * );
		va_end( ap );
	        for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
			Pair *r = i->ptr;
			if ( r->value == value ) return r; } }
        return NULL; }

void
registryRemove( Registry *registry, Pair *entry ) {
	// remove entry from registry
	listItem *next_i, *last_i=NULL;
	for ( listItem *i=registry->entries; i!=NULL; i=next_i ) {
		Pair *r = i->ptr;
		next_i = i->next;
		if ( r!=entry ) last_i = i;
		else {
			freeItem( i );
			if (( last_i )) last_i->next = next_i;
			else registry->entries = next_i;
			return; } } }

Registry *
registryDuplicate( Registry *registry ) {
	int type = registry->type.value[ 0 ];
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
		jlast = jnext; }
	return copy; }

//===========================================================================
//	compare
//===========================================================================
static int
compare( RegistryType type, Pair *entry, void *name ) {
	switch ( type ) {
	case IndexedByName:
		return strcomp( entry->name, (char *) name, 0 );
	case IndexedByNameRef:
		return strcomp( entry->name, (char *) name, 1 );
	case IndexedByNumber:
		return ( cast_i(entry->name) - *(int *) name );
	case IndexedByAddress:
		return ( (uintptr_t) entry->name - (uintptr_t) name ); } }

