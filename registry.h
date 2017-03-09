#ifndef REGISTRY_H
#define REGISTRY_H

typedef struct _registryEntry
{
        void *identifier;
        void *value;
        struct _registryEntry *next;
}
registryEntry;

typedef registryEntry * Registry;

registryEntry *newRegistryItem( void *identifier, void *value );
registryEntry *registerByName( Registry *registry, char *name, void *address );
registryEntry *registerByAddress( Registry *registry, void *address, void *value );
registryEntry *lookupByName( Registry registry, char *name );
registryEntry *lookupByAddress( Registry registry, void *address );
registryEntry *lookupByValue( Registry registry, void *value );

Registry copyRegistry( Registry registry );

void	deregisterByValue( Registry *registry, void *ptr );
void	deregisterByAddress( Registry *registry, void *ptr );
void	freeRegistryItem( registryEntry *r );
void	freeRegistry( Registry *r );

#endif	// REGISTRY_H
