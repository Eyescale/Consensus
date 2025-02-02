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

#define NUM	17

static freeBTreeCB free_CB;
int
main( int argc, char **argv ) {
	BTree *btree = newBTree( '$' );
	int value[ NUM ];
	for ( int i=0; i<NUM; i++ )
		value[i] = 48-i;
	uint index[ NUM ];
	for ( int i=0; i<NUM; i++ )
		index[ i ] = btreeAdd( btree, value+i );
	for ( int i=0; i<NUM; i++ ) {
		uint key[2] = { '$', index[i] };
		printf( "storing: %d : %c%d\n", *(int*)btreeLookup( btree, key ), key[0], key[1] ); }
	printf( "--\n" );
	freeBTree( btree, free_CB );
	if ( CNMemoryUsed ) {
                fprintf( stderr, "Memory Leak: %llu/%llu Pair items unaccounted for\n",
                        CNMemoryUsed, CNMemorySize ); } }

static int
free_CB( uint key[2], void *value ) {
	printf( "freeing: %c%d : %d\n", key[0], key[1], *(int*)value );
	return 1; }

