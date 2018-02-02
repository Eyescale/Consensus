#ifndef NARRATIVE_UTIL_H
#define NARRATIVE_UTIL_H

/*---------------------------------------------------------------------------
	narrative utilities	- public
---------------------------------------------------------------------------*/

Narrative *newNarrative( void );
Occurrence *newOccurrence( OccurrenceType type );
void	freeNarrative( Narrative * );
void	freeOccurrence( Occurrence * );

void	registerNarrative( Narrative *, _context * );
void	deregisterNarrative( Narrative *, _context * );

registryEntry *addNarrative( Entity *, char *, Narrative * );
void	removeNarrative( Entity *, Narrative * );
registryEntry *addToNarrative( Narrative *, Entity * );
void	removeFromNarrative( Narrative *, Entity * );

Narrative *lookupNarrative( Entity *entity, char * );

Narrative *activateNarrative( Entity *, Narrative * );
void	deactivateNarrative( Entity *, Narrative * );
int	narrative_active( Entity *, Narrative * );
int	any_narrative_active( _context * );

void	reorderNarrative( Occurrence * );
int	is_narrative_dangling( Occurrence *, int out );


#endif	// NARRATIVE_UTIL_H
