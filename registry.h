#ifndef REGISTRY_H
#define REGISTRY_H

#define RTYPE( r )	(r)->index.value

typedef struct _registryEntry
{
	union {
		char *name;
		void *address;
		int value;
	} index;
        void *value;
        struct _registryEntry *next;
}
registryEntry;

typedef enum {
	IndexedByAddress = 0, // default
	IndexedByName,
	IndexedByNumber
} RegistryType;

void freeRegistryEntry( RegistryType type, registryEntry *entry );

typedef registryEntry Registry;

Registry *newRegistry( RegistryType type );
void freeRegistry( Registry **registry );
void emptyRegistry( Registry *registry );
Registry *registryDuplicate( Registry *registry );

registryEntry *registryRegister( Registry *registry, void *name, void *value );
void registryDeregister( Registry *registry, void *name, ... );
registryEntry *registryLookup( Registry *registry, void *name, ... );


#endif	// REGISTRY_H
