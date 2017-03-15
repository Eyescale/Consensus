#ifndef DATABASE_H
#define DATABASE_H
#include "registry.h"

typedef struct _listItem
{
	struct _listItem *next;
	void *ptr;
}
listItem;

typedef struct _Entity
{
	// entity data
        struct _Entity *sub[3];

	int state;

	// memory management
        struct _Entity *next;
	listItem *as_sub[3];
}
Entity;

Entity *newEntity( Entity *source, Entity *medium, Entity *target );
void freeEntity( Entity *this );

void *newItem( void *ptr );
void *lookupItem( listItem *list, void *ptr );
listItem *addItem( listItem **list, void *ptr );
void removeItem( listItem **list, void *ptr );
void clipListItem( listItem **list, listItem *i, listItem *last_i, listItem *next_i );
listItem *addIfNotThere( listItem **list, void *ptr );
void removeIfThere( listItem **list, void *ptr );
void freeItem( listItem *item );
void popListItem( listItem **item );
void freeListItem( listItem **item );
int reorderListItem( listItem **item );


#endif	// DATABASE_H
