#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "story.h"

typedef Registry BMContext;
typedef struct {
	struct { void *type; listItem *xpn; } *mark;
	union { listItem *list; Pair *record; } match;
} MarkData;

void 		bm_context_actualize( BMContext *, char *, CNInstance * );
int		bm_context_load( BMContext *ctx, char *path );
void		bm_context_init( BMContext * );
int		bm_context_update( BMContext *, CNStory * );
void		bm_context_release( BMContext * );

Pair *		bm_mark( char *, char * );
void		bm_context_mark( BMContext *, MarkData * );
MarkData *	bm_context_remark( BMContext *, MarkData * );
MarkData *	bm_context_unmark( BMContext *, MarkData * );
listItem *	bm_push_mark( BMContext *, char *, void * );	
void		bm_pop_mark( BMContext *, char * );
void		bm_reset_mark( BMContext *, char *, void * );

void *		bm_lookup( BMContext *, char *, CNDB *, int );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
CNInstance *	bm_register( BMContext *, char *, CNDB * );
int		bm_register_locale( BMContext *, char * );

BMContext *	newContext( CNEntity *cell );
void		freeContext( BMContext * );

static inline void * bm_context_lookup( BMContext *ctx, char *p ) {
	return bm_lookup( ctx, p, NULL, 0 ); }
static inline void bm_register_string( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	addIfNotThere((listItem **)&entry->name, e->sub[1] ); }
static inline void bm_register_ube( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	addIfNotThere((listItem **)&entry->value, e->sub[1] ); }
static inline void bm_deregister( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	if ( isUnnamed(e) )
		removeIfThere((listItem **)&entry->value, e->sub[1] );
	else	removeIfThere((listItem **)&entry->name, e->sub[1] ); }

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

static inline Pair * BMContextCurrent( BMContext *ctx ) {
	return ((listItem *) registryLookup( ctx, "." )->value )->ptr; }
static inline CNInstance * BMContextPerso( BMContext *ctx ) {
	return BMContextCurrent( ctx )->name; }
static inline Registry *BMContextLocales( BMContext *ctx ) {
	return BMContextCurrent( ctx )->value; }

//===========================================================================
//	Utilities
//===========================================================================
static inline void bm_context_pipe_flush( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value ); }

static inline void context_rebase( BMContext *ctx ) {
	CNDB *db = BMContextDB( ctx );
	CNInstance *self = BMContextSelf( ctx );
	Pair *entry = BMContextCurrent( ctx );
	entry->name = db_instantiate( self, entry->name, db ); }


#endif	// CONTEXT_H
