#ifndef NARRATIVE_H
#define	NARRATIVE_H

#include "list.h"

typedef enum {
	ROOT,
	IN,
	ON,
	DO,
	INPUT,
	OUTPUT,
	ELSE,
	ELSE_IN,
	ELSE_ON,
	ELSE_DO,
	ELSE_INPUT,
	ELSE_OUTPUT
} CNOccurrenceType;

typedef struct {
	CNOccurrenceType type;
	struct {
		char *expression;
		listItem *sub;
	} *data;
} CNOccurrence;

typedef CNOccurrence * CNNarrative;

CNNarrative * readNarrative( char *path );
CNNarrative * newNarrative( void );
void	freeNarrative( CNNarrative * );

int	narrative_output( FILE *, CNNarrative *, int );


#endif	// NARRATIVE_H
