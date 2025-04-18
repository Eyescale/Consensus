#ifndef CONTEXT_H
#define CONTEXT_H

#include "database.h"

typedef Registry BMContext;
typedef struct {
	struct { void *type; listItem *xpn; } *data;
	union { listItem *list; Pair *record; } match;
} BMMark;

BMContext *	newContext( CNEntity *cell );
void		freeContext( BMContext * );
void 		bm_context_actualize( BMContext *, char *, CNInstance * );
void		bm_context_flush( BMContext * );

int		proto_verify( char *, CNInstance *, BMContext * );

BMMark *	bm_mark( char *, char *, unsigned int flags, void * );
BMMark *	bm_lmark( Pair * );
void		bm_vmark( BMContext *, char *, BMMark * );
void		bm_context_mark( BMContext *, BMMark * );
BMMark *	bm_context_unmark( BMContext *, BMMark * );
int 		bm_context_check( BMContext *, BMMark * );
Pair *		bm_context_push( BMContext *, char *, void * );
void *		bm_context_pop( BMContext *, char * );
listItem *	bm_context_take( BMContext *, char *, void * );
void		bm_context_debug( BMContext *, char * );

void *		bm_lookup( int, char *, BMContext *, CNDB * );
int		bm_match( BMContext *, CNDB *, char *, CNInstance *, CNDB * );
CNInstance *	bm_register( BMContext *, char *, CNDB * );
int		bm_register_locales( BMContext *, char * );
listItem *	bm_inform( BMContext *, listItem **, CNDB * );
CNInstance *	bm_translate( BMContext *, CNInstance *, CNDB *, int );
void		bm_untag( BMContext *, char * );

void *		BMContextRVTest( BMContext *, char *, int * );

static inline Pair * bm_tag( BMContext *ctx, char *tag, void *value ) {
	return registryRegister( ctx, strmake(tag), value ); }
static inline Pair * BMTag( BMContext *ctx, char *tag ) {
	return registryLookup( ctx, tag ); }
static inline void * BMContextRVV( BMContext *ctx, char *p ) {
	return bm_lookup( 0, p, ctx, NULL ); }

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
	return (Pair *) BMContextDB(ctx)->nil->sub; }
static inline CNInstance * BMContextSelf( BMContext *ctx ) {
	return BMContextId( ctx )->name; }
static inline CNInstance * BMContextParent( BMContext *ctx ) {
	return BMContextId( ctx )->value; }
static inline CNEntity * BMContextCell( BMContext *ctx ) {
	return CNProxyThat( BMContextSelf(ctx) ); }
static inline ActiveRV * BMContextActive( BMContext *ctx ) {
	return registryLookup( ctx, "@" )->value; }
static inline void * BMContextEENOV( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "<" );
        return (( entry->value ) ? ((listItem *) entry->value )->ptr : NULL ); }
static inline void *BMContextBaseClass( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, ">" );
	return (entry) ? entry->value : NULL; }
static inline void *BMContextBase( listItem *narratives, BMContext *ctx ) {
	Pair *entry = BMContextBaseClass( ctx );
	if (( entry )) narratives = entry->value;
	return narratives->ptr; }

static inline void bm_subclass( BMContext *ctx, Pair *base ) {
	if (( base )) registryRegister( ctx, ">", base ); }

static inline Registry * BMContextCurrent( BMContext *ctx ) {
	return ((listItem *) registryLookup( ctx, "." )->value )->ptr; }
static inline Pair * BMLocaleEntry( BMContext *ctx, char *p ) {
	listItem *stack = registryLookup( ctx, "." )->value;
	for ( listItem *i=stack; i!=NULL; i=i->next ) {
		Pair *entry = registryLookup( i->ptr, p );
		if (( entry )) return entry; }
	return NULL; }
static inline CNInstance * BMContextPerso( BMContext *ctx ) {
	return BMLocaleEntry(ctx,".")->value; }
static inline void bm_context_rebase( BMContext *ctx, CNInstance *perso ) {
	BMLocaleEntry(ctx,".")->value = (perso)?perso:BMContextSelf(ctx); }

static inline Pair *Mset( BMContext *ctx, void *value ) {
	return registryRegister( ctx, "~", value ); }
static inline void Mclr( BMContext *ctx ) {
	registryDeregister( ctx, "~" ); }


//===========================================================================
//	Utilities
//===========================================================================
static inline void bm_context_pipe_flush( BMContext *ctx ) {
	Pair *entry = registryLookup( ctx, "|" );
	freeListItem((listItem **) &entry->value ); }


#endif	// CONTEXT_H
