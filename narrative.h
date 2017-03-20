#ifndef NARRATIVE_H
#define NARRATIVE_H

/*---------------------------------------------------------------------------
	narrative actions	- public
---------------------------------------------------------------------------*/

_action	read_narrative;

/*---------------------------------------------------------------------------
	narrative utilities	- public
---------------------------------------------------------------------------*/

void	registerNarrative( Narrative *narrative, _context *context );

registryEntry *addNarrative( Entity *entity, char *name, Narrative *narrative );
void	removeNarrative( Entity *entity, Narrative *narrative );

registryEntry *addToNarrative( Narrative *narrative, Entity *entity );
void	removeFromNarrative( Narrative *narrative, Entity *entity );

Narrative *lookupNarrative( Entity *entity, char *name );
Narrative *activateNarrative( Entity *entity, Narrative *narrative );
void	deactivateNarrative( Entity *entity, Narrative *narrative );

void freeNarrative( Narrative *narrative );

#endif	// NARRATIVE_H
