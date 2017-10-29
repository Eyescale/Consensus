#ifndef NARRATIVE_UTIL_H
#define NARRATIVE_UTIL_H

/*---------------------------------------------------------------------------
	narrative utilities	- private
---------------------------------------------------------------------------*/

Narrative *newNarrative( void );
void freeNarrative( Narrative *narrative );

Occurrence *newOccurrence( OccurrenceType type );
void freeOccurrence( Occurrence *occurrence );

void	reorderNarrative( Occurrence *occurrence );
int	is_narrative_dangling( Occurrence *occurrence, int out );
void	deregisterNarrative( Narrative *narrative, _context *context );
void	freeOccurrence( Occurrence *occurrence );


#endif	// NARRATIVE_UTIL_H
