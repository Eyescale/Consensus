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
#define CB( value ) \
	if (( value )) cb(key,value); \
	if( ++key[1] >= ndx ) break;

BTree *
newBTree( uint user_type ) {
	BTreeData *data = (BTreeData *) newPair( NULL, NULL );
	data->type.key[ 0 ] = user_type;
	return (BTree *) newPair( NULL, data ); }

void
freeBTree( BTree *btree, freeBTreeCB cb ) {
	freeListItem( &btree->cache );
	BTreeData *data = btree->data;
	uint type = data->type.key[ 0 ];
	uint ndx = data->type.key[ 1 ];
	Pair *entry = data->root;
	freePair((Pair *) data );
	freePair((Pair *) btree );
	if ( !ndx ) return;
	uint mask, key[2]={ type, 0 };
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
	do freePair( entry ); while (( entry=popListItem(&stack) )); }

//---------------------------------------------------------------------------
//	btreeAdd
//---------------------------------------------------------------------------
static inline void btreeSet( BTree *, uint, void * );

uint
btreeAdd( BTree *btree, void *value ) {
	if (( btree->cache )) {
		uint index = pop_item( &btree->cache );
		btreeSet( btree, index, value );
		return index; }
	BTreeData *data = btree->data;
	uint mask, ndx = data->type.key[ 1 ]++;
	for ( mask=!!ndx; mask<ndx; mask<<=1 ) {}
	Pair *entry = data->root;
	if ( mask==ndx ) { // ndx is either a power of 2, or 0
		if ( ndx!=1 )
			entry = data->root = newPair( entry, NULL ); }
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
	if ( ndx & 1 )
		entry->value = value;
	else	entry->name = value;
	return ndx; }

static inline void
btreeSet( BTree *btree, uint index, void *value ) {
	BTreeData *data = btree->data;
	uint mask, ndx = data->type.key[ 1 ];
	if ( index < ndx ) {
		Pair *entry = data->root;
		for ( mask=1; mask<ndx; mask<<=1 ) {}
		for ( mask>>=1; mask>1; mask>>=1 )
			entry = index&mask ? entry->value : entry->name;
		if ( index & 1 )
			entry->value = value;
		else	entry->name = value; } }

//---------------------------------------------------------------------------
//	btreeShake
//---------------------------------------------------------------------------
#define SHAKE_CB( entry ) \
	if (( entry )) switch ( cb(key,entry,user_data) ) {	\
		case BT_DONE: 					\
			freeListItem(&stack);			\
			return 1;				\
		case BT_CACHE:					\
			add_item( &btree->cache, key[1] );	\
			entry = NULL; }				\
	if ( ++key[1] >= ndx ) break;

int
btreeShake( BTree *btree, btreeShakeCB cb, void *user_data )
/*
	Assumption: cb!=NULL
*/ {
	BTreeData *data = btree->data;
	uint type = data->type.key[ 0 ];
	uint ndx = data->type.key[ 1 ];
	Pair *entry = data->root;
	if ( !ndx ) return 0;
	uint mask, key[2]={ type, 0 };
	listItem *stack = NULL;
	for ( mask=1; mask<ndx; mask<<=1 ) {}
	for ( ; ; ) {
		for ( mask>>=1; mask>1; mask>>=1 ) {
			addItem( &stack, entry );
			entry = entry->name; }
		SHAKE_CB( entry->name )
		SHAKE_CB( entry->value )
		for ( mask=2; ; mask<<=1 ) {
			Pair *parent = stack->ptr;
			if ( entry==parent->value )
				entry = popListItem( &stack );
			else {
				entry = parent->value;
				break; } } }
	freeListItem( &stack );
	return 0; }

//---------------------------------------------------------------------------
//	btreePick
//---------------------------------------------------------------------------
void *
btreePick( BTree *btree, uint key[2] ) {
	BTreeData *data = btree->data;
	uint ndx = data->type.key[ 1 ];
	uint mask, index = key[1];
	if ( index < ndx ) {
		Pair *entry = data->root;
		for ( mask=1; mask<ndx; mask<<=1 ) {}
		for ( mask>>=1; mask>1; mask>>=1 )
			entry = index&mask ? entry->value : entry->name;
		return index&1 ? entry->value : entry->name; }
	return NULL; }

//---------------------------------------------------------------------------
//	btreePluck
//---------------------------------------------------------------------------
#define PLUCK_CB( entry ) \
	if (( entry )) switch ( cb(key,entry,user_data) ) {	\
		case BT_CACHE:					\
			add_item( &btree->cache, key[1] );	\
			entry = NULL; }
void
btreePluck( BTree *btree, uint key[2], btreeShakeCB cb, void *user_data ) {
	BTreeData *data = btree->data;
	uint ndx = data->type.key[ 1 ];
	uint mask, index = key[1];
	if ( index < ndx ) {
		Pair *entry = data->root;
		for ( mask=1; mask<ndx; mask<<=1 ) {}
		for ( mask>>=1; mask>1; mask>>=1 )
			entry = index&mask ? entry->value : entry->name;
		if ( index&1 ) {
			PLUCK_CB( entry->value ) }
		else {	PLUCK_CB( entry->name ) } } }

