#ifndef DETERNARIZE_PRIVATE_H
#define DETERNARIZE_PRIVATE_H

typedef int BMTernaryCB( char *, void * );
typedef struct {
	BMTernaryCB *user_CB;
	void *user_data;
	int ternary;
	struct { listItem *sequence, *flags; } stack;
	listItem *sequence;
	Pair *segment;
} DeternarizeData;

static char * deternarize( char *, listItem **, BMTraverseData *, char * );
static void s_scan( CNString *, listItem * );


#endif	// DETERNARIZE_PRIVATE_H
