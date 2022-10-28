#ifndef DETERNARIZE_PRIVATE_H
#define DETERNARIZE_PRIVATE_H

#include "string_util.h"
#include "traverse.h"

static void free_deternarized( listItem *sequence );

#define s_append( bgn, end ) \
	for ( char *_c=bgn; _c!=end; StringAppend(s,*_c++) );
#define s_add( str ) \
	for ( char *_c=str; *_c; StringAppend(s,*_c++) );
static void s_scan( CNString *, listItem * );

static BMTraverseCB
	open_CB, ternary_operator_CB, filter_CB, close_CB;
typedef int BMTernaryCB( char *, void * );
typedef struct {
	BMTernaryCB *user_CB;
	void *user_data;
	int ternary;
	struct { listItem *sequence, *flags; } stack;
	listItem *sequence;
	Pair *segment;
} DeternarizeData;

#define case_( func ) \
	} static BMCBTake func( BMTraverseData *traverse_data, char **q, int flags, int f_next ) { \
		DeternarizeData *data = traverse_data->user_data; char *p = *q;


#endif	// DETERNARIZE_PRIVATE_H
