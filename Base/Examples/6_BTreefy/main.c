#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "btree.h"

//===========================================================================
//	main()	- TEST
//===========================================================================
char *sequence =
	"((%?,*toto:(billy,.boy)),%(tata:%ernest,%))"
//	"%(**?)"
	;

static void output_closure( int level, int previous );
static void output_level( int level );
typedef struct { int level; } OutputData;
static BTreeTraverseCB output_CB;

int
main( int argc, char *argv[] )
{
	if ( argc > 1 ) sequence = argv[ 1 ];
	BTreeNode *btree = btreefy( sequence );
	printf( "Sequence\n\t%s\nTree\n", sequence );
	OutputData data;
	data.level = 0;
	bt_traverse( btree, output_CB, &data );
	output_closure( 1, data.level );
	if ( data.level == 1 ) printf( "\n" );
	freeBTree( btree );
}
static int
output_CB( listItem **path, int position, listItem *sub, void *user_data )
{
	OutputData *data = user_data;

	int level = 0;
	for ( listItem *i=*path; i!=NULL; i=i->next )
		level++;
	output_closure( level, data->level );

	int first = 1;
	BTreeNode *node = (*path)->ptr;
	for ( char *p=node->data; *p; p++ ) {
		switch ( *p ) {
		case '(':
			if ( first ) {
				output_level( level );
			}
			printf( "(\n" );
			goto RETURN;
		case ')':
			printf( "\n" );
			goto RETURN;
		case ':':
			if ( first ) continue;
			else {
				printf( "\n" );
				goto RETURN;
			}
			break;
		case ',':
			if ( first ) {
				output_level( level-1 );
				printf( ",\n" );
			} else {
				printf( "\n" );
				goto RETURN;
			}
			break;
		default:
			if ( first ) {
				first = 0;
				output_level( level );
			}
			printf( "%c", *p );
		}
	}
RETURN:
	data->level = level;
	return BT_CONTINUE;
}
static void
output_closure( int level, int previous )
{
	for ( int i=previous; i > level; i-- ) {
		for ( int j=1; j < i; j++ )
			printf( "\t" );
		printf( ")\n" );
	}
}
static void
output_level( int level )
{
	for ( int i=0; i<level; i++ )
		printf( "\t" );
}

