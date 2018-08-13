#ifndef LIST_H
#define LIST_H

typedef struct _listItem
{
	struct _listItem *next;
	void *ptr;
}
listItem;

void *newItem( void *ptr );
void freeItem( listItem *item );
listItem *addItem( listItem **list, void *ptr );
void removeItem( listItem **list, void *ptr );
void *lookupItem( listItem *list, void *ptr );
void clipListItem( listItem **list, listItem *i, listItem *last_i, listItem *next_i );
listItem *addIfNotThere( listItem **list, void *ptr );
void removeIfThere( listItem **list, void *ptr );
listItem *catListItem( listItem *list1, listItem *list2 );
void *popListItem( listItem **item );
void freeListItem( listItem **item );
int reorderListItem( listItem **item );


#endif	// LIST_H
