#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "list.h"

//===========================================================================
//	Occurrence, Narrative, Story types
//===========================================================================
// Occurrence types

#define ROOT		0
#define	IN		1
#define ON		(1<<1)
#define ON_X		(1<<2)
#define DO		(1<<3)
#define EN		(1<<4)
#define ELSE		(1<<5)
#define INPUT		(1<<6)
#define OUTPUT		(1<<7)
#define LOCALE		(1<<8)
#define PER		(1<<9)
#define SWITCH		(1<<10)
#define CASE		(1<<11)

#define IN_S		(IN|SWITCH)
#define ON_S		(ON|SWITCH)
#define IN_X		(ON|PER)
#define PER_X		(ON_X|PER)
#define EN_X		(EN|PER)
#define C_IN		(CASE|IN)
#define C_ON		(CASE|ON)

typedef struct {
	struct {
		void *type;
		char *expression;
	} *data;
	listItem *sub;
} CNOccurrence;

typedef struct {
	char *proto;
	CNOccurrence *root;
} CNNarrative;

//===========================================================================
//	Public Interface
//===========================================================================
CNNarrative *	newNarrative( void );
void		freeNarrative( CNNarrative * );

CNOccurrence *	newOccurrence( int type );
void		freeOccurrence( CNOccurrence * );

void		narrative_reorder( CNNarrative * );
int		narrative_output( FILE *, CNNarrative *, int level );


#endif	// NARRATIVE_H
