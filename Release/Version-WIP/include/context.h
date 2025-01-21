#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"
#include "story.h"

typedef Registry BMContext;
typedef struct {
	struct { void *type; listItem *xpn; } *data;
	union { listItem *list; Pair *record; } match;
} BMMark;

int		bm_context_load( BMContext *ctx, char *path );
void		bm_context_init( BMContext * );
int		bm_context_update( BMContext *, CNStory * );
void 		bm_context_actualize( BMContext *, char *, CNInstance * );
void		bm_context_release( BMContext * );

BMMark *	bm_mark( char *, char *, unsigned int flags, void * );
BMMark *	bm_lmark( Pair * );
void		bm_context_mark( BMContext *, BMMark * );
BMMark *	bm_context_unmark( BMContext *, BMMark * );
int 		bm_context_check( BMContext *, BMMark * );
Pair *		bm_context_push( BMContext *, char *, void * );
void *		bm_context_pop( BMContext *, char * );
listItem *	bm_context_take( BMContext *, char *, void * );
void		bm_context_debug( BMContext *, char * );

void *		bm_lookup( int, char *, BMContext *, CNDB * );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
void		bm_untag( BMContext *, char * );
CNInstance *	bm_register( BMContext *, char *, CNDB * );
int		bm_register_locale( BMContext *, char * );

BMContext *	newContext( CNEntity *cell );
void		freeContext( BMContext * );

static inline Pair * bm_tag( BMContext *ctx, char *tag, void *value ) {
	return registryRegister( ctx, strmake(tag), value ); }
static inline Pair * BMTag( BMContext *ctx, char *tag ) {
	return registryLookup( ctx, tag ); }
static inline void * BMVal( BMContext *ctx, char *p ) {
	return bm_lookup( 0, p, ctx, NULL ); }

static inline void bm_cache_string( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	addIfNotThere((listItem **)&entry->name, e->sub[1] ); }
static inline void bm_cache_ube( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	addIfNotThere((listItem **)&entry->value, e->sub[1] ); }
static inline CNInstance * bm_lookup_string( BMContext *ctx, char *s ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	for ( listItem *i=entry->name; i!=NULL; i=i->next ) {
		entry = i->ptr;	// [ string, ref:{[db,e]} ]
		if ( strcmp( s, entry->name ) ) continue;
		CNDB *db = registryLookup( ctx, "" )->value;
		entry = registryLookup((Registry *) entry->value, db );
		return ((entry) ? entry->value : NULL ); }
	return NULL; }
static inline void bm_uncache( BMContext *ctx, CNInstance *e ) {
	Pair *entry = registryLookup( ctx, "$" )->value;
	if ( cnIsUnnamed(e) )
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
static inline Pair * BMContextShared( BMContext *ctx ) {
	return registryLookup( ctx, "$" )->value; }
static inline Pair * BMContextId( BMContext *ctx ) {
	return registryLookup( ctx, "%" )->value; }
static inline CNInstance * BMContextSelf( BMContext *ctx ) {
	return BMContextId( ctx )->name; }
static inline CNInstance * BMContextParent( BMContext *ctx ) {
	return BMContextId( ctx )->value; }
static inline CNEntity * BMContextCell( BMContext *ctx ) {
	return CNProxyThat( BMContextSelf(ctx) ); }
static inline ActiveRV * BMContextActive( BMContext *ctx ) {
	return registryLookup( ctx, "@" )->value; }
static inline void * BMContextEENOVCurrent( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "<" );
        return (( entry->value ) ? ((listItem *) entry->value )->ptr : NULL ); }

static inline Pair * BMContextCurrent( BMContext *ctx ) {
	return ((listItem *) registryLookup( ctx, "." )->value )->ptr; }
static inline CNInstance * BMContextPerso( BMContext *ctx ) {
	return BMContextCurrent( ctx )->name; }
static inline Registry * BMContextLocales( BMContext *ctx ) {
	return BMContextCurrent( ctx )->value; }
static inline Pair *Mset( BMContext *ctx, void *value ) {
	return registryRegister( BMContextLocales(ctx), "", value ); }
static inline void Mclr( BMContext *ctx ) {
	registryDeregister( BMContextLocales(ctx), "" ); }

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
