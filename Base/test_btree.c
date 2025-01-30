/*
	make:
		cc -o test -I./include test_btree.c libcn.a
	run:
		./test
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"

typedef struct {
	uint key;
	void *value; } lookup;

#define NUM	17

static freeBTreeCB free_CB;
int
main( int argc, char **argv ) {
	BTree *btree = newBTree( '$' );
	int value[ NUM ];
	for ( int i=0; i<NUM; i++ )
		value[i] = 48-i;
	uint key[ NUM ];
	for ( int i=0; i<NUM; i++ )
		key[ i ] = btreeAdd( btree, value+i );
	for ( int i=0; i<NUM; i++ )
		printf( "key=%d, value=%d\n", key[i], *(int*)btreeLookup( btree, key[i] ) );
	freeBTree( btree, free_CB );
	if ( CNMemoryUsed ) {
                fprintf( stderr, "Memory Leak: %llu/%llu Pair items unaccounted for\n",
                        CNMemoryUsed, CNMemorySize ); } }

static void
free_CB( int type, int ndx, void *value ) {
	printf( "freeing: key=%d value=%d\n", ndx, *(int*)value ); }

