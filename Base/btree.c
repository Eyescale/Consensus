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
#define CB( value ) if ((cb(key,value),++key[1])>=ndx) break;

BTree *
newBTree( uint user_type ) {
	BTree *btree = (BTree *) newPair( NULL, NULL );
	btree->type.key[ 0 ] = user_type;
	return btree; }

void
freeBTree( BTree *btree, freeBTreeCB cb ) {
	uint type=btree->type.key[ 0 ];
	uint ndx=btree->type.key[ 1 ];
	Pair *entry = btree->root;
	freePair((Pair *) btree );
	if ( !ndx ) return;
	uint mask, key[2]={type,0};
	listItem *stack = NULL;
	for ( mask=1; mask<ndx; mask<<=1 ) {}
	for ( ; ; ) {
		for ( mask>>=1; mask>1; mask>>=1 ) {
			addItem( &stack, entry );
			entry = entry->name; }
		if (( cb )) {
			CB( entry->name )
			CB( entry->value ) }
		else {
			key[1] += 2;
			if ( key[1] >= ndx ) break; }
		for ( mask=2; ; mask<<=1 ) {
			Pair *parent = stack->ptr;
			freePair( entry );
			if ( entry==parent->value )
				entry = popListItem( &stack );
			else {
				entry = parent->value;
				break; } } }
	do freePair( entry );
	while (( entry=popListItem(&stack) )); }

//---------------------------------------------------------------------------
//	btreeAdd
//---------------------------------------------------------------------------
uint
btreeAdd( BTree *btree, void *value ) {
	uint mask, ndx=btree->type.key[ 1 ]++;
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

//---------------------------------------------------------------------------
//	btreePick
//---------------------------------------------------------------------------
void *
btreePick( BTree *btree, uint key[2] ) {
	uint ndx=btree->type.key[ 1 ];
	uint mask, index=key[1];
	if ( index < ndx ) {
		Pair *entry = btree->root;
		for ( mask=1; mask<ndx; mask<<=1 ) {}
		for ( mask>>=1; mask>1; mask>>=1 )
			entry = index&mask ? entry->value : entry->name;
		return index&1 ? entry->value : entry->name; }
	return NULL; }

//---------------------------------------------------------------------------
//	btreeShake
//---------------------------------------------------------------------------
#define SHAKE( value ) \
	if ( !cb( key, value, user_data ) ) return 1; \
	if ( ++key[1] >= ndx ) break;

int
btreeShake( BTree *btree, shakeBTreeCB cb, void *user_data )
/*
	Assumption: cb!=NULL
*/ {
	uint type=btree->type.key[ 0 ];
	uint ndx=btree->type.key[ 1 ];
	Pair *entry = btree->root;
	if ( !ndx ) return 0;
	uint mask, key[2]={type,0};
	listItem *stack = NULL;
	for ( mask=1; mask<ndx; mask<<=1 ) {}
	for ( ; ; ) {
		for ( mask>>=1; mask>1; mask>>=1 ) {
			addItem( &stack, entry );
			entry = entry->name; }
		SHAKE( entry->name )
		SHAKE( entry->value )
		for ( mask=2; ; mask<<=1 ) {
			Pair *parent = stack->ptr;
			if ( entry==parent->value )
				entry = popListItem( &stack );
			else {
				entry = parent->value;
				break; } } }
	freeListItem( &stack );
	return 0; }

