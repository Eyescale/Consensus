#ifndef NARRATIVE_H
#define NARRATIVE_H

/*---------------------------------------------------------------------------
	narrative actions	- public
---------------------------------------------------------------------------*/

_action	read_narrative;

/*---------------------------------------------------------------------------
	narrative utilities	- public
---------------------------------------------------------------------------*/

Narrative *lookupNarrative( Entity *entity, char *name );
Narrative *activateNarrative( Entity *entity, Narrative *narrative );

registryEntry *addNarrative( Entity *entity, char *name, Narrative *narrative );
registryEntry *addToNarrative( Narrative *narrative, Entity *entity );
void	registerNarrative( Narrative *narrative, _context *context );
void	removeNarrative( Narrative *narrative, Entity *e );
void	removeFromNarrative( Narrative *narrative, Entity *entity );

void freeNarrative( Narrative *narrative );

#endif	// NARRATIVE_H
