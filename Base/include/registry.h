#ifndef REGISTRY_H
#define REGISTRY_H

#include "pair.h"

typedef struct { int type; listItem *entries; } Registry;
typedef enum {
	IndexedByAddress = 0, // default
	IndexedByName,
	IndexedByNumber
} RegistryType;

typedef int freeRegistryCB( Registry *, Pair * );

Registry *newRegistry( RegistryType type );
void	freeRegistry( Registry *registry, freeRegistryCB );
Pair *	newRegistryEntry( RegistryType type, void *name, void *value );
Pair *	registryRegister( Registry *registry, void *name, void *value );
void	registryDeregister( Registry *registry, void *name, ... );
Pair *	registryLookup( Registry *registry, void *name, ... );

Registry *registryDuplicate( Registry *registry );


#endif	// REGISTRY_H
