#ifndef NARRATIVE_UTIL_H
#define NARRATIVE_UTIL_H

/*---------------------------------------------------------------------------
	narrative utilities	- public
---------------------------------------------------------------------------*/

int	narrative_active( _context *context );
void	registerNarrative( Narrative *narrative, _context *context );

registryEntry *addNarrative( Entity *entity, char *name, Narrative *narrative );
void	removeNarrative( Entity *entity, Narrative *narrative );

Narrative *activateNarrative( Entity *entity, Narrative *narrative );
void	deactivateNarrative( Entity *entity, Narrative *narrative );
Narrative *lookupNarrative( Entity *entity, char *name );

registryEntry *addToNarrative( Narrative *narrative, Entity *entity );
void	removeFromNarrative( Narrative *narrative, Entity *entity );

void	reorderNarrative( Occurrence *occurrence );
int	is_narrative_dangling( Occurrence *occurrence, int out );
void	deregisterNarrative( Narrative *narrative, _context *context );
void	freeOccurrence( Occurrence *occurrence );

Narrative *newNarrative( void );
Occurrence *newOccurrence( OccurrenceType type );
void	freeNarrative( Narrative *narrative );
void	freeOccurrence( Occurrence *occurrence );


#endif	// NARRATIVE_UTIL_H
