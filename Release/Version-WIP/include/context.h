#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef Registry BMContext;

BMContext *	newContext( CNEntity *cell, CNEntity *parent );
void		freeContext( BMContext * );
void		bm_context_init( BMContext *ctx );
int		bm_context_update( CNEntity *this, BMContext *ctx );
void 		bm_context_actualize( BMContext *, char *, CNInstance * );
void		bm_context_release( BMContext * );

Pair *		bm_mark( int, char *, char *, void * );
void		bm_context_mark( BMContext *, void * );
Pair * 		bm_context_unmark( BMContext *, void * );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );

CNInstance *	bm_register( BMContext *, char *, CNDB * );
int		bm_register_locale( BMContext *, char * );
void *		bm_lookup( BMContext *, char *, CNDB *, int );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
void *		bm_inform( int, BMContext *, void *, CNDB * );
CNInstance *	bm_intake( BMContext *, CNDB *, CNInstance *, CNDB * );

static inline void * bm_context_lookup( BMContext *ctx, char *p ) {
	return bm_lookup( ctx, p, NULL, 0 ); }

typedef struct {
	struct { listItem *activated, *deactivated; } *buffer;
	listItem *value;
} ActiveRV;

typedef struct {
	Pair *event;
	CNInstance *src;
} EEnoRV;

static inline CNDB * BMContextDB( BMContext *ctx ) {
	return registryLookup( ctx, "" )->value; }
static inline Pair * BMContextId( BMContext *ctx ) {
	return registryLookup( ctx, "%" )->value; }
static inline CNInstance * BMContextSelf( BMContext *ctx ) {
	return BMContextId( ctx )->name; }
static inline CNInstance * BMContextParent( BMContext *ctx ) {
	return BMContextId( ctx )->value; }
static inline CNEntity * BMContextCell( BMContext *ctx ) {
	return DBProxyThat( BMContextSelf(ctx) ); }
static inline ActiveRV * BMContextActive( BMContext *ctx ) {
	return registryLookup( ctx, "@" )->value; }
static inline void * BMContextEENOVCurrent( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "<" );
        return (( entry->value ) ? ((listItem *) entry->value )->ptr : NULL ); }
static inline CNInstance * BMContextPerso( BMContext *ctx ) {
	listItem *i = registryLookup( ctx, "." )->value;
	return (( i ) ? ((Pair *) i->ptr )->name : NULL ); }
static inline Registry *BMContextCurrent( BMContext *ctx ) {
	listItem *i = registryLookup( ctx, "." )->value;
	return (( i ) ? ((Pair *) i->ptr )->value : NULL ); }

//===========================================================================
//	Utilities
//===========================================================================
static inline void bm_context_pipe_flush( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value ); }

static inline void context_rebase( BMContext *ctx ) {
	CNDB *db = BMContextDB( ctx );
	CNInstance *self = BMContextSelf( ctx );
	Pair *entry = ((listItem *) registryLookup( ctx, "." )->value )->ptr;
	entry->name = db_instantiate( self, entry->name, db ); }


#endif	// CONTEXT_H
