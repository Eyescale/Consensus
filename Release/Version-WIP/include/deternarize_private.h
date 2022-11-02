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

typedef int BMTernaryCB( char *, void * );
typedef struct {
	BMTernaryCB *user_CB;
	void *user_data;
	int ternary;
	struct { listItem *sequence, *flags; } stack;
	listItem *sequence;
	Pair *segment;
} DeternarizeData;


#endif	// DETERNARIZE_PRIVATE_H
