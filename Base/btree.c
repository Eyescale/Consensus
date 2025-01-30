#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "list.h"
#include "btree.h"

//===========================================================================
//	public interface
//===========================================================================
BTree *
newBTree( int user_type ) {
	BTree *btree = (BTree *) newPair( NULL, NULL );
	btree->type.value[ 0 ] = user_type;
	return btree; }

void
freeBTree( BTree *btree, freeBTreeCB cb ) {
	uint type=btree->type.value[ 0 ];
	uint ndx=btree->type.value[ 1 ];
	Pair *entry = btree->root;
	freePair((Pair *) btree );
	if ( !ndx ) return;
	uint mask, index=0;
	listItem *stack = NULL;
	for ( mask=1; mask<ndx; mask<<=1 ) {}
	for ( ; ; ) {
		for ( mask>>=1; mask>1; mask>>=1 ) {
			addItem( &stack, entry );
			entry = entry->name; }
		if (( cb )) {
			cb( type, index, entry->name );
			if ( index+1 < ndx )
				cb( type, index+1, entry->value ); }
		index += 2;
		if ( index >= ndx ) break;
		for ( mask=2; ; mask<<=1 ) {
			Pair *parent = stack->ptr;
			freePair( entry );
			if ( entry==parent->value )
				entry = popListItem( &stack );
			else {
				entry = parent->value;
				break; } } }
RETURN:
	do freePair( entry );
	while (( entry=popListItem(&stack) )); }

uint
btreeAdd( BTree *btree, void *value ) {
	uint mask, ndx=btree->type.value[ 1 ]++;
	for ( mask=!!ndx; mask<ndx; mask<<=1 ) {}
	Pair *entry = btree->root;
	if ( mask==ndx ) { // ndx is either a power of 2, or 0
		if ( ndx!=1 )
			entry = btree->root = newPair( entry, NULL ); }
	else
		while (( mask >>= 1 )) {
			if ( ndx & mask ) {
				if (( entry->value ))
					entry = entry->value;
				else break; }
			else entry = entry->name; }
	if (( mask >>= 1 )) {
		entry = entry->value = newPair( NULL, NULL );
		while (( mask >>= 1 ))
			entry = entry->name = newPair( NULL, NULL ); }
	if ( ndx&1 )
		entry->value = value;
	else	entry->name = value;
	return ndx; }

void *
btreeLookup( BTree *btree, uint index ) {
	uint mask, ndx=btree->type.value[ 1 ];
	if ( index < ndx ) {
		Pair *entry = btree->root;
		for ( mask=1; mask<ndx; mask<<=1 ) {}
		for ( mask>>=1; mask>1; mask>>=1 )
			entry = index&mask ? entry->value : entry->name;
		return index&1 ? entry->value : entry->name; }
	return NULL; }

