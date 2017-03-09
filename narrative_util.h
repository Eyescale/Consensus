#ifndef NARRATIVE_UTIL_H
#define NARRATIVE_UTIL_H

/*---------------------------------------------------------------------------
	narrative utilities	- private
---------------------------------------------------------------------------*/

void	reorderNarrative( Occurrence *occurrence );
int	is_narrative_dangling( Occurrence *occurrence, int out );
void	deregisterNarrative( Narrative *narrative, _context *context );


#endif	// NARRATIVE_UTIL_H
