#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "story.h"

typedef Registry BMContext;

void 		bm_context_actualize( BMContext *, char *, CNInstance * );
int		bm_context_load( BMContext *ctx, char *path );
void		bm_context_init( BMContext * );
int		bm_context_update( BMContext *, CNStory * );
void		bm_context_release( BMContext * );

Pair *		bm_mark( int, char *, char *, void * );
void		bm_context_mark( BMContext *, void * );
Pair * 		bm_context_unmark( BMContext *, void * );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );
void		bm_reset_mark( BMContext *, char *, void * );

CNInstance *	bm_register( BMContext *, char *, CNDB * );
int		bm_register_locale( BMContext *, char * );
void *		bm_lookup( BMContext *, char *, CNDB *, int );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
void *		bm_inform( int, BMContext *, void *, CNDB * );
CNInstance *	bm_intake( BMContext *, CNDB *, CNInstance *, CNDB * );

BMContext *	newContext( CNEntity *cell );
void		freeContext( BMContext * );

inline void * bm_context_lookup( BMContext *ctx, char *p ) {
	return bm_lookup( ctx, p, NULL, 0 ); }

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

typedef struct {
	Pair *event;
	CNInstance *src;
} EEnoRV;

inline CNDB * BMContextDB( BMContext *ctx ) {
	return registryLookup( ctx, "" )->value; }
inline Pair * BMContextId( BMContext *ctx ) {
	return registryLookup( ctx, "%" )->value; }
inline CNInstance * BMContextSelf( BMContext *ctx ) {
	return BMContextId( ctx )->name; }
inline CNInstance * BMContextParent( BMContext *ctx ) {
	return BMContextId( ctx )->value; }
inline CNEntity * BMContextCell( BMContext *ctx ) {
	return DBProxyThat( BMContextSelf(ctx) ); }
inline ActiveRV * BMContextActive( BMContext *ctx ) {
	return registryLookup( ctx, "@" )->value; }
inline void * BMContextEENOVCurrent( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "<" );
        return (( entry->value ) ? ((listItem *) entry->value )->ptr : NULL ); }

inline Pair * BMContextCurrent( BMContext *ctx ) {
	return ((listItem *) registryLookup( ctx, "." )->value )->ptr; }
inline CNInstance * BMContextPerso( BMContext *ctx ) {
	return BMContextCurrent( ctx )->name; }
inline Registry *BMContextLocales( BMContext *ctx ) {
	return BMContextCurrent( ctx )->value; }

//===========================================================================
//	Utilities
//===========================================================================
static inline void bm_context_pipe_flush( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value ); }

inline void context_rebase( BMContext *ctx ) {
	CNDB *db = BMContextDB( ctx );
	CNInstance *self = BMContextSelf( ctx );
	Pair *entry = BMContextCurrent( ctx );
	entry->name = db_instantiate( self, entry->name, db ); }


#endif	// CONTEXT_H
