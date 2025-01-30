#ifndef REGISTRY_H
#define REGISTRY_H

#include "pair.h"
#include "list.h"

typedef struct {
	union { void *ptr; uint value[2]; } type; // 64-bit
	listItem *entries; } Registry;
typedef enum {
	IndexedByAddress = 0, // default
	IndexedByNameRef,
	IndexedByName,
	IndexedByNumber
} RegistryType;

typedef void freeRegistryCB( Registry *, Pair * );

Registry *newRegistry( RegistryType type );
void	freeRegistry( Registry *registry, freeRegistryCB );
Pair *	newRegistryEntry( RegistryType type, void *name, void *value );
Pair *	registryRegister( Registry *registry, void *name, void *value );
void	registryCBDeregister( Registry *registry, freeRegistryCB, void * );
void	registryDeregister( Registry *registry, void *name, ... );
Pair *	registryLookup( Registry *registry, void *name, ... );
void	registryRemove( Registry *registry, Pair *entry );

Registry *registryDuplicate( Registry *registry );


#endif	// REGISTRY_H
