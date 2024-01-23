#ifndef LIST_H
#define LIST_H

#include <string.h>
#include "pair.h"

typedef struct _listItem {
	void *ptr;
	struct _listItem *next;
} listItem;

listItem *newItem( void *ptr );
void freeItem( listItem *item );
void clipItem( listItem **, listItem *item );

listItem *addItem( listItem **list, void *ptr );
void removeItem( listItem **list, void *ptr );
listItem *lookupItem( listItem *list, void *ptr );

void clipListItem( listItem **list, listItem *i, listItem *last_i, listItem *next_i );

listItem *addIfNotThere( listItem **list, void *ptr );
listItem *lookupIfThere( listItem *list, void *ptr );
int removeIfThere( listItem **list, void *ptr );

listItem *catListItem( listItem *list1, listItem *list2 );
void *popListItem( listItem **item );
void freeListItem( listItem **item );
int reorderListItem( listItem **item );

#define new_item( i ) newItem( cast_ptr(i) )
#define add_item( list, i ) addItem( list, cast_ptr(i) )
#define pop_item( list ) cast_i( popListItem(list) )

#endif	// LIST_H
