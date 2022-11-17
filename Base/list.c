#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "pair.h"
#include "list.h"

listItem *
newItem( void *ptr )
{
        return (listItem *) newPair( ptr, NULL );
}
void
freeItem( listItem *item )
{
	freePair((Pair *) item );
}
void
clipItem( listItem **list, listItem *item )
{
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*list; i!=NULL; i=next_i ) {
		next_i = i->next;
		if ( i == item ) {
			clipListItem( list, i, last_i, next_i );
			break; }	
		else last_i = i; }
}
listItem *
addItem( listItem **list, void *ptr )
{
	listItem *i = newItem( ptr );
	i->next = *list;
	*list = i;
	return i;
}
void
removeItem( listItem **list, void *ptr )
/*
	Assumption: list has no doublon
*/
{
	listItem *last_i=NULL, *next_i;
	for ( listItem *i=*list; i!=NULL; i=next_i ) {
		next_i = i->next;
		if ( i->ptr == ptr ) {
			clipListItem( list, i, last_i, next_i );
			break; }
		else last_i = i; }
#ifdef DEBUG
	if (( lookupItem( *list, ptr ) )) {
		fprintf( stderr, ">>>>> B%%: Error: removeItem(): doublon\n" );
		exit( -1 ); }
#endif
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
			continue; }
		if ( comparison == 0 ) {
#ifdef DEBUG
			output( Debug, "addIfNotThere: Warning: result already registered" );
#endif
			return NULL; }
		break; }
	item = newItem( ptr );
	if ( last_i == NULL ) {
		item->next = *list;
		*list = item; }
	else {
		item->next = i;
		last_i->next = item; }
	return item;
}
listItem *
lookupIfThere( listItem *list, void *ptr )
{
	for ( listItem *i=list; i!=NULL; i=i->next ) {
		intptr_t comparison = (uintptr_t) i->ptr - (uintptr_t) ptr;
		if ( comparison < 0 )
			continue;
                else if ( comparison == 0 )
			return i;
		break; }
	return NULL;
}
int
removeIfThere( listItem **list, void *ptr )
{
	listItem *last_i = NULL, *next_i;
	for ( listItem *i=*list; i!=NULL; i=next_i ) {
		intptr_t comparison = (uintptr_t) i->ptr - (uintptr_t) ptr;
		next_i = i->next;
		if ( comparison < 0 ) {
			last_i = i;
			continue; }
		else if ( comparison == 0 ) {
			clipListItem( list, i, last_i, next_i );
			return 1; }
		break; }
	return 0;
}
listItem *
catListItem( listItem *list1, listItem *list2 )
{
	if ( list1 == NULL ) return list2;
	if ( list2 != NULL ) {
		listItem *j = list1;
		while ( j->next != NULL ) j=j->next;
		j->next = list2; }
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
		last_i = i; }
	*list = last_i;
	return count;
}
listItem *
add_item( listItem **list, int value )
{
	union { int value; void *ptr; } icast;
	icast.value = value;
	return addItem( list, icast.ptr );
}
int
pop_item( listItem **list )
{
	union { int value; void *ptr; } icast;
	icast.ptr = popListItem( list );
	return icast.value;
}
