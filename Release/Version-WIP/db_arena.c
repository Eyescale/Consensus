#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"
#include "btree.h"
#include "database.h"

static Pair * DBSharedArena = NULL;

//===========================================================================
//	db_arena_init / db_arena_exit / db_arena_unref
//===========================================================================
void
db_arena_init( void ) {
	if (( DBSharedArena )) return;
	DBSharedArena = newPair( newBTree('$'), newBTree('!') ); }

//---------------------------------------------------------------------------
//	db_arena_exit
//---------------------------------------------------------------------------
static freeBTreeCB free_CB;

void
db_arena_exit( void ) {
	freeBTree( DBSharedArena->name, free_CB );
	freeBTree( DBSharedArena->value, free_CB );
	freePair( DBSharedArena );
	DBSharedArena = NULL; }

static freeRegistryCB deregister_CB;
static int
free_CB( uint key[2], void *value ) {
	switch ( key[0] ) {
	case 's':
	case '$':;
		Pair *entry = value;
		free( entry->name );
		freeRegistry( entry->value, deregister_CB );
		freePair( entry );
		break;
	case '!':
		freeRegistry( value, deregister_CB );
		break; }
	return 1; }

static void
deregister_CB( Registry *registry, Pair *entry ) {
	if (( entry->name )) {
		CNInstance *e = entry->value;
		e->sub[ 1 ] = NULL;
		cn_prune( e ); } }

//---------------------------------------------------------------------------
//	db_arena_unref
//---------------------------------------------------------------------------
static inline uint strtype( Registry *ref );
static btreeShakeCB unref_CB;

void
db_arena_unref( CNDB *db )
/*
	Assumption: called by freeContext() right before freeCNDB()
*/ {
	btreeShake( DBSharedArena->name, unref_CB, db );
	btreeShake( DBSharedArena->value, unref_CB, db ); }

static int
unref_CB( uint key[2], void *entry, void *user_data ) {
	CNDB *db = user_data;
	Registry *ref;
	switch ( key[0] ) {
	case '$':
		ref = ((Pair *) entry )->value;
		key[ 0 ] = strtype( ref );
		registryCBDeregister( ref, deregister_CB, db );
		if ( !ref->entries && key[0]=='s' ) {
			free( ((Pair *) entry )->name );
			freeRegistry( ref, NULL );
			freePair((Pair *) entry );
			return BT_CACHE; }
		break;
	case '!':
		ref = (Registry *) entry;
		registryCBDeregister( ref, deregister_CB, db );
		if ( !ref->entries ) {
			freeRegistry( ref, NULL );
			return BT_CACHE; }
		break; }
	return BT_CONTINUE; }

static inline uint
strtype( Registry *ref ) {
	if ( !ref->entries ) return '$';
	Pair *entry = ref->entries->ptr;
	if ( !entry->name ) return '$';
	return CNSharedKey( entry->value )[0]; }

//===========================================================================
//	db_arena_register
//===========================================================================
/*
	assumption: called from instantiate.c
*/
typedef struct { uint index; char *s; Registry *ref; } IndexData;

CNInstance *
db_arena_register( char *code, CNDB *db ) {
	union { uint value[2]; void *ptr; } key;
	IndexData data;
	data.s = NULL;
	if (( code )) {
		key.ptr = db_arena_key( code );
		Pair *entry = btreePick( DBSharedArena->name, key.value );
		data.ref = entry->value;
		entry = registryLookup( data.ref, db );
		if (( entry )) return entry->value; }
	else {
		key.value[ 0 ] = '!';
		data.ref = newRegistry( IndexedByAddress );
		key.value[ 1 ] = btreeAdd( DBSharedArena->value, data.ref ); }
	return db_share( key.value, data.ref, db ); }

//===========================================================================
//	db_arena_deregister
//===========================================================================
static btreeShakeCB pluck_CB;

void
db_arena_deregister( CNInstance *e, CNDB *db )
/*
	Assumptions
		. called upon entity removal => e is no longer connected
		. e->sub[ 0 ]==NULL
*/ {
	void *entry;
	uint *master = CNSharedKey( e );
	uint key[2] = { master[0], master[1] };
	switch ( key[0] ) {
	case 's':
	case '$': btreePluck( DBSharedArena->name, key, pluck_CB, db ); break;
	case '!': btreePluck( DBSharedArena->value, key, pluck_CB, db ); } }

static int
pluck_CB( uint key[2], void *entry, void *user_data ) {
	CNDB *db = user_data;
	Registry *ref;
	switch ( key[0] ) {
	case 's':
	case '$':
		ref = ((Pair *) entry )->value;
		registryCBDeregister( ref, deregister_CB, db );
		if ( !ref->entries && key[0]=='s' ) {
			free( ((Pair *) entry )->name );
			freeRegistry( ref, NULL );
			freePair((Pair *) entry );
			return BT_CACHE; }
		break;
	case '!':
		ref = (Registry *) entry;
		registryCBDeregister( ref, deregister_CB, db );
		if ( !ref->entries ) {
			freeRegistry( ref, NULL );
			return BT_CACHE; }
		break; }
	return BT_CONTINUE; }

//===========================================================================
//	db_arena_encode
//===========================================================================
/*
	Assumption: called from parser.c
*/
static inline void set_origin( IndexData *, char * );
static inline int i2hex( int i );
static btreeShakeCB strcmp_CB;

void
db_arena_encode( CNString *code, CNString *string, Pair *origin )
/*
	Note: code string starts with least significant hextet
*/ {
	// register string and/or get index
	IndexData data;
	data.s = StringFinish( string, 0 );
	if ( !data.s ) data.s = strdup( "" );
	StringReset( string, CNStringMode );
	if ( btreeShake( DBSharedArena->name, strcmp_CB, &data ) ) {
		if (( origin )) set_origin( &data, origin->name );
		free( data.s ); }
	else {
		data.ref = newRegistry( IndexedByAddress );
		Pair *entry = newPair( data.s, data.ref );
		data.index = btreeAdd( DBSharedArena->name, entry );
		if (( origin )) set_origin( &data, origin->name ); }
	// got index => now encode
	do StringAppend( code, i2hex(data.index&63) ); while ((data.index>>=6)); }

static inline void
set_origin( IndexData *data, char *name ) {
	if (( name )) registryRegister( data->ref, NULL, name ); }

static int
strcmp_CB( uint key[2], void *value, void *user_data ) {
	// important: key[0] here is the arena key - i.e. '$'
	IndexData *data = user_data;
	Pair *entry = value;
	if ( !strcmp(entry->name,data->s) ) {
		data->index = key[ 1 ];
		data->ref = entry->value;
		return BT_DONE; }
	return BT_CONTINUE; }

static inline int
i2hex( int i ) {
	return i + (
		i < 10 ? 48 :	// 00-09: 0-9
		i < 36 ? 55 :	// 10-35: A-Z
		i < 62 ? 61 :	// 36-61: a-z
		32 ); }		// 62-63: ^_

//===========================================================================
//	db_arena_origin
//===========================================================================
char *
db_arena_origin( CNInstance *e )
/*
	return class name of shared string entity's narrative - if it has
*/ {
	if ( !cnIsShared(e) ) return NULL;
	uint *key = CNSharedKey( e );
	if ( key[0]!='$' ) return NULL;
	Pair *entry = btreePick( DBSharedArena->name, key );
	if ( !entry ) return NULL;
	Registry *ref = entry->value;
	if ( !ref->entries ) return NULL;
	entry = ref->entries->ptr;
	if (( entry->name )) return NULL;
	return entry->value; }

//===========================================================================
//	db_arena_key
//===========================================================================
static inline int hex2i( int h );

void *
db_arena_key( char *code ) {
	union { uint value[2]; void *ptr; } key;
	key.value[ 0 ] = '$';
	key.value[ 1 ] = 0;
	if ( *code=='"' ) code++;
	for ( int shift=0; *code && *code!='"'; code++, shift+=6 )
		key.value[ 1 ] |= hex2i(*code) << shift;
	return key.ptr; }

static inline int
hex2i( int h ) {
	return h - (
		h < 58 ? 48 :	// 0-9: 00-09
		h < 91 ? 55 :	// A-Z: 10-35
		h > 96 ? 61 :	// a-z: 36-61
		32 ); }		//  ^_: 62-63

//===========================================================================
//	db_arena_lookup
//===========================================================================
CNInstance *
db_arena_lookup( char *code, CNDB *db ) {
	union { uint value[2]; void *ptr; } key;
	key.ptr = db_arena_key( code );
	// note that key.value[ 0 ] is ignored here
	Pair *entry = btreePick( DBSharedArena->name, key.value );
	if (( entry )) {
		entry = registryLookup( entry->value, db );
		if (( entry )) return entry->value; }
	return NULL; }

//===========================================================================
//	db_arena_makeup
//===========================================================================
CNInstance *
db_arena_makeup( CNInstance *e, CNDB *db, CNDB *db_dst )
/*
	build $(e,?:...) and return shared arena string entity
*/ {
	
	if ( !e || db_private(0,e,db) ) return NULL;
	CNString *s = newString();
	for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; ) {
		e = i->ptr;
		if ( !db_private(0,e,db) ) {
			CNInstance *f = e->sub[ 1 ];
			if ( !cnIsIdentifier(f) ) break;
			s_add( CNIdentifier(f) )
			i = e->as_sub[ 0 ]; }
		else i = i->next; }
	if ( !StringInformed(s) ) { freeString(s); return NULL; }
	uint key[2];
	IndexData data;
	data.s = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	if ( btreeShake( DBSharedArena->name, strcmp_CB, &data ) ) {
		free( data.s );
		Pair *entry = registryLookup( data.ref, db_dst );
		if (( entry )) { freeString(s); return entry->value; }
		key[ 0 ] = strtype( data.ref );
		key[ 1 ] = data.index; }
	else {
		data.ref = newRegistry( IndexedByAddress );
		Pair *entry = newPair( data.s, data.ref );
		key[ 1 ] = btreeAdd( DBSharedArena->name, entry );
		key[ 0 ] = 's'; }
	freeString( s );
	return db_share( key, data.ref, db_dst ); }

//===========================================================================
//	db_arena_output
//===========================================================================
static inline void output( FILE *, char * );
void
db_arena_output( FILE *stream, char *fmt, char *code ) {
	for ( ; *fmt; fmt++ )
		switch ( *fmt ) {
		case '%':
			fmt++;
			switch ( *fmt ) {
			case '%': fputc( '%', stream ); break;
			case '_': output( stream, code ); break; }
			break;
		case '\\':
			fmt++;
			switch ( *fmt ) {
			case 'n': fputc( '\n', stream ); break;
			case 't': fputc( '\t', stream ); break; }
			break;
		default:
			fputc( *fmt, stream ); } }

static inline void
output( FILE *stream, char *code ) {
	union { uint value[2]; void *ptr; } key;
	key.ptr = db_arena_key( code );
	// note that key.value[ 0 ] is ignored here
	Pair *entry = btreePick( DBSharedArena->name, key.value );
	if (( entry )) fprintf( stream, "\"%s\"", entry->name ); }

//===========================================================================
//	db_arena_identifier
//===========================================================================
char *
db_arena_identifier( CNInstance *e )
/*
	Assumption: e->sub[0]==NULL
*/ {
	uint *key = CNSharedKey( e );
	Pair *entry = btreePick( DBSharedArena->name, key );
	if (( entry )) return entry->name;
	else return NULL; }

//===========================================================================
//	db_arena_translate
//===========================================================================
CNInstance *
db_arena_translate( CNInstance *e, CNDB *db, int inform )
/*
	Assumption: e->sub[ 0 ]==NULL
*/ {
	uint *key = CNSharedKey( e );
	Registry *ref;
	Pair *entry;
	switch ( key[0] ) {
	case 's':
	case '$':
		entry = btreePick( DBSharedArena->name, key );
		if ( !entry ) return NULL;
		ref = entry->value;
		break;
	case '!':
		ref = btreePick( DBSharedArena->value, key );
		if ( !ref ) return NULL;
		break; }

	entry = registryLookup( ref, db );
	if (( entry ))
		return entry->value;
	else if ( inform )
		return db_share( key, ref, db );
	else return NULL; }

