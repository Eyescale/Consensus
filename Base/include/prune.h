#ifndef PRUNE_H
#define PRUNE_H

typedef enum {
	PRUNE_TERNARY = 1,
	PRUNE_TERM,
	PRUNE_FILTER,
	PRUNE_LITERAL,
	PRUNE_LIST,
	PRUNE_IDENTIFIER
} PruneType;

char *	p_prune( PruneType type, char * );

//===========================================================================
//	p_ternary, p_single, p_filtered, p_list
//===========================================================================
static inline int p_ternary( char *p ) {
	// Assumption: *p=='('
	p = p_prune( PRUNE_TERNARY, p );
	return ( *p=='?' ); }

static inline int p_single( char *p ) {
	// Assumption: *p=='('
	p = p_prune( PRUNE_TERM, p+1 );
	return ( *p!=',' ); }

static inline int p_filtered( char *p ) {
	p = p_prune( PRUNE_FILTER, p );
	return ( *p==':' ); }

static inline int p_list( char *p ) {
	/* Assumption: *p=='('
	   We want to determine if p is ((expression,...):sequence:)
	           Ellipsis -------------------------^
	*/
	if ( p[1]=='(' ) {
		p = p_prune( PRUNE_TERM, p+2 );
		return !strncmp( p, ",...", 4 ); }
	return 0; }


#endif	// PRUNE_H
