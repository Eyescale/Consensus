#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "btree.h"

//===========================================================================
//	main()	- TEST
//===========================================================================
char *sequence =
//	"(hello,world)"
	"(hello):(titi,toto)"
//	"alpha:beta((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	":alpha:beta:((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	"{alpha,beta:({hello,hi},(world)),gamma,delta}"
//	"{alpha,beta:(hello,(world):(titi,toto)),gamma,delta}"
//	"((%?,*toto:~(billy,.boy)),%(tata:%ernest,%)))"
//	"alpha:beta"
//	"alpha:beta"
//	"%(**?)"
	;

static void output_closure( int opening, int level, int previous );
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
	output_closure( *sequence, 1, data.level );
	fprintf( stderr, "\n" );
	freeBTree( btree );
}
static int
output_CB( listItem **path, int position, listItem *sub, void *user_data )
{
	OutputData *data = user_data;

	BTreeNode *node = (*path)->ptr;
	if ( !node->data ) // root node
		return BT_CONTINUE;

	int level = -1;
	for ( listItem *i=*path; i!=NULL; i=i->next )
		level++;
	output_closure( *(char *)node->data, level, data->level );

	int first = 1;
	for ( char *p=node->data; *p; p++ ) {
		switch ( *p ) {
		case '|':
		case '{':
		case '(':
			if ( first ) {
				output_level( level );
			}
			printf( "%c\n", *p );
			goto RETURN;
		case '}':
		case ')':
			printf( "\n" );
			goto RETURN;
		case ':':
			if ( first ) {
				first = 0;
				output_level( level );
				printf( ":" );
			}
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
output_closure( int opening, int level, int previous )
{
	int closure = opening=='{' ? '}' : ')';

	for ( int i=previous; i > level; i-- ) {
		for ( int j=1; j < i; j++ )
			printf( "\t" );
		printf( "%c\n", closure );
	}
}
static void
output_level( int level )
{
	for ( int i=0; i<level; i++ )
		printf( "\t" );
}

