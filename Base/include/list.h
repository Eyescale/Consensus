#ifndef LIST_H
#define LIST_H

#include <string.h>

typedef struct _listItem
{
	void *ptr;
	struct _listItem *next;
}
listItem;

listItem *newItem( void *ptr );
void freeItem( listItem *item );
void clipItem( listItem **, listItem *item );

listItem *addItem( listItem **list, void *ptr );
void removeItem( listItem **list, void *ptr );
listItem *lookupItem( listItem *list, void *ptr );

void clipListItem( listItem **list, listItem *i, listItem *last_i, listItem *next_i );

listItem *addIfNotThere( listItem **list, void *ptr );
listItem *lookupIfThere( listItem *list, void *ptr );
void removeIfThere( listItem **list, void *ptr );

listItem *catListItem( listItem *list1, listItem *list2 );
void *popListItem( listItem **item );
void freeListItem( listItem **item );
int reorderListItem( listItem **item );

listItem *add_item( listItem **list, int value );
int pop_item( listItem **list );

#endif	// LIST_H
