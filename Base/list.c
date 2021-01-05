#include <stdio.h>
#include <stdlib.h>

#include "cache.h"
#include "list.h"

#define C 500
#if 1
static void **listItemCache = NULL;
#else
int cache_count = 0;
void ** UniversalCache = NULL;
#define listItemCache UniversalCache
#endif

listItem *
newItem( void *ptr )
{
        listItem *item = (listItem *) allocate( &listItemCache, C );
        item->next = NULL;
        item->ptr = ptr;
        return item;
}
void
freeItem( listItem *item )
{
	recycle((void**) item, &listItemCache );
}
void
clipItem( listItem **list, listItem *item )
{
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*list; i!=NULL; i=next_i ) {
		next_i = i->next;
		if ( i == item ) {
			clipListItem( list, i, last_i, next_i );
			break;
		}	
		else last_i = i;
	}
}
listItem *
addItem( listItem **list, void *ptr )
{
	listItem *i = newItem( ptr );
	i->next = *list;
	*list = i;
	return i;
}
listItem *
add_item( listItem **list, int value )
{
        union { int value; char *ptr; } icast;
        icast.value = value;
        return addItem( list, icast.ptr );
}
void
removeItem( listItem **list, void *ptr )
{
	listItem *last_i=NULL, *next_i;
	for ( listItem *i=*list; i!=NULL; i=next_i )
	{
		next_i = i->next;
		if ( i->ptr == ptr ) {
			clipListItem( list, i, last_i, next_i );
			break;
		}
		else last_i = i;
	}
}
listItem *
lookupItem( listItem *list, void *ptr )
{
	for ( listItem *i = list; i!=NULL; i=i->next )
		if ( i->ptr == ptr )
			return i;
	return NULL;
}
void
clipListItem( listItem **list, listItem *i, listItem *last_i, listItem *next_i )
{
	if ( last_i == NULL ) { *list = next_i; }
	else { last_i->next = next_i; }
	freeItem( i );
}
listItem *
addIfNotThere( listItem **list, void *ptr )
{
        listItem *item, *i, *last_i = NULL;
        for ( i=*list; i!=NULL; i=i->next ) {
		intptr_t comparison = (uintptr_t) i->ptr - (uintptr_t) ptr;
                if ( comparison < 0 ) {
                        last_i = i;
                        continue;
                }
                if ( comparison == 0 ) {
#ifdef DEBUG
			output( Debug, "addIfNotThere: Warning: result already registered" );
#endif
                        return NULL;
                }
                break;
        }
        item = newItem( ptr );
        if ( last_i == NULL ) {
                item->next = *list;
                *list = item;
        } else {
                item->next = i;
                last_i->next = item;
        }
	return item;
}
listItem *
lookupIfThere( listItem *list, void *ptr )
{
        listItem *last_i = NULL, *next_i;
        for ( listItem *i=list; i!=NULL; i=next_i ) {
		intptr_t comparison = (uintptr_t) i->ptr - (uintptr_t) ptr;
		next_i = i->next;
                if ( comparison < 0 ) {
                        last_i = i;
                        continue;
                }
                else if ( comparison == 0 ) {
			return i;
                }
                break;
        }
	return NULL;
}
void
removeIfThere( listItem **list, void *ptr )
{
        listItem *last_i = NULL, *next_i;
        for ( listItem *i=*list; i!=NULL; i=next_i ) {
		intptr_t comparison = (uintptr_t) i->ptr - (uintptr_t) ptr;
		next_i = i->next;
                if ( comparison < 0 ) {
                        last_i = i;
                        continue;
                }
                else if ( comparison == 0 ) {
			clipListItem( list, i, last_i, next_i );
                }
                break;
        }
}
listItem *
catListItem( listItem *list1, listItem *list2 )
{
	if ( list1 == NULL ) return list2;
	if ( list2 != NULL ) {
		listItem *j = list1;
		while ( j->next != NULL ) j=j->next;
		j->next = list2;
	}
	return list1;
}
void *
popListItem( listItem **list )
{
	if ( *list == NULL ) return NULL;
	void *ptr = (*list)->ptr;
	listItem *next_i = (*list)->next;
	freeItem( *list );
	*list = next_i;
	return ptr;
}
void
freeListItem( listItem **list )
{
	listItem *i, *next_i;
	for ( i=*list; i!=NULL; i=next_i ) {
		next_i = i->next;
		freeItem( i );
	}
	*list = NULL;
}
int
reorderListItem( listItem **list )
{
	int count = 0;
	listItem *i, *next_i, *last_i = NULL;

	// put list in proper order
	for ( i = *list; i!=NULL; i=next_i, count++ ) {
		next_i = i->next;
		i->next = last_i;
		last_i = i;
	}
	*list = last_i;
	return count;
}

