#ifndef REGISTRY_H
#define REGISTRY_H

typedef struct _registryEntry
{
	union {
		char *name;
		void *address;
		int index;
	} identifier;
        void *value;
        struct _registryEntry *next;
}
registryEntry;

typedef registryEntry * Registry;

registryEntry *newRegistryItem( void *identifier, void *value );
registryEntry *registerByName( Registry *registry, char *name, void *address );
registryEntry *lookupByName( Registry registry, char *name );
registryEntry *registerByAddress( Registry *registry, void *address, void *value );
registryEntry *lookupByAddress( Registry registry, void *address );
registryEntry *registerByIndex( Registry *registry, int ndx, void *value );
registryEntry *lookupByIndex( Registry registry, int ndx );
registryEntry *lookupByValue( Registry registry, void *value );

Registry copyRegistry( Registry registry );

void	deregisterByValue( Registry *registry, void *ptr );
void	deregisterByAddress( Registry *registry, void *ptr );
void	freeRegistryItem( registryEntry *r );
void	freeRegistry( Registry *r );

#endif	// REGISTRY_H
