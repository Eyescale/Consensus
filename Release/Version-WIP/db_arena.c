#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string_util.h"
#include "btree.h"
#include "database.h"

static Pair * DBSharedArena = NULL;

//===========================================================================
//	db_arena_init / db_arena_exit
//===========================================================================
void
db_arena_init( void ) {
	if ( !DBSharedArena )
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
	case '$': ;
		Pair *entry = value;
		free( entry->name );
		freeRegistry( entry->value, deregister_CB );
		freePair( entry );
		break;
	case '!':
		freeRegistry( value, deregister_CB ); }
	return 1; }

static void
deregister_CB( Registry *registry, Pair *entry ) {
	CNInstance *e = entry->value;
	e->sub[ 1 ] = NULL;
	cn_prune( e ); }

//===========================================================================
//	db_arena_encode / db_arena_key
//===========================================================================
/*
	note that code stores least significant hextet first
*/
typedef struct { char *s; uint index; Registry *ref; } StrcmpData;
static inline int i2hex( int i );
static inline int hex2i( int h );
static shakeBTreeCB strcmp_CB;

void
db_arena_encode( CNString *code, CNString *string ) { 
	uint index;
	// register string and/or get index
	StrcmpData data;
	data.s = StringFinish( string, 0 );
	if ( !data.s ) data.s = strdup( "" );
	StringReset( string, CNStringMode );
	if ( btreeShake( DBSharedArena->name, strcmp_CB, &data ) ) {
		index = data.index;
		free( data.s ); }
	else {
		Registry *ref = newRegistry( IndexedByAddress );
		index = btreeAdd( DBSharedArena->name, newPair(data.s,ref) ); }
	// got index => now encode
	do StringAppend( code, i2hex(index&63) ); while ((index>>=6)); }

static int
strcmp_CB( uint key[2], void *value, void *user_data ) {
	StrcmpData *data = user_data;
	Pair *entry = value;
	if ( !strcmp(entry->name,data->s) ) {
		data->index = key[ 1 ];
		data->ref = entry->value;
		return 0; }
	return 1; }

//---------------------------------------------------------------------------
//	db_arena_key
//---------------------------------------------------------------------------
void *
db_arena_key( char *code ) {
	union { uint value[2]; void *ptr; } key;
	key.value[ 0 ] = '$';
	key.value[ 1 ] = 0;
	if ( *code=='"' ) code++;
	for ( int shift=0; *code && *code!='"'; code++, shift+=6 )
		key.value[ 1 ] |= hex2i(*code) << shift;
	return key.ptr; }

//---------------------------------------------------------------------------
//	i2hex / hex2i
//---------------------------------------------------------------------------
static inline int i2hex( int i ) {
	return i + (
		i < 10 ? 48 :	// 00-09: 0-9
		i < 36 ? 55 :	// 10-35: A-Z
		i < 62 ? 61 :	// 36-61: a-z
		32 ); }		// 62-63: ^_

static inline int hex2i( int h ) {
	return h - (
		h < 58 ? 48 :	// 0-9: 00-09
		h < 91 ? 55 :	// A-Z: 10-35
		h > 96 ? 61 :	// a-z: 36-61
		32 ); }		//  ^_: 62-63

//===========================================================================
//	db_arena_register
//===========================================================================
CNInstance *
db_arena_register( char *code, CNDB *db ) {
	union { uint value[2]; void *ptr; } key;
	Registry *ref;
	if (( code )) {
		key.ptr = db_arena_key( code );
		Pair *entry = btreePick( DBSharedArena->name, key.value );
		entry = registryLookup( (ref=entry->value), db );
		if (( entry )) return entry->value; }
	else {
		key.value[ 0 ] = '!';
		ref = newRegistry( IndexedByAddress );
		key.value[ 1 ] = btreeAdd( DBSharedArena->value, ref ); }
	return db_share( key.value, ref, db ); }

//===========================================================================
//	db_arena_lookup
//===========================================================================
CNInstance *
db_arena_lookup( char *code, CNDB *db ) {
	union { uint value[2]; void *ptr; } key;
	key.ptr = db_arena_key( code );
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
			int event = *CNIdentifier( f );
			if ( !event ) break;
			StringAppend( s, event );
			i = e->as_sub[ 0 ]; }
		else i = i->next; }
	if ( !StringInformed(s) ) return NULL;
	StrcmpData data;
	data.s = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	if ( btreeShake( DBSharedArena->name, strcmp_CB, &data ) ) {
		free( data.s );
		Pair *entry = registryLookup( data.ref, db_dst );
		if (( entry )) { e=entry->value; goto FOUND; } }
	else {
		data.ref = newRegistry( IndexedByAddress );
		Pair *entry = newPair( data.s, data.ref );
		data.index = btreeAdd( DBSharedArena->name, entry ); }
	freeString( s );
	union { uint value[2]; void *ptr; } key;
	key.value[ 0 ] = '$';
	key.value[ 1 ] = data.index;
	return db_share( key.value, data.ref, db_dst );
FOUND:
	freeString( s );
	return e; }

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
	Pair *entry = btreePick( DBSharedArena->name, key.value );
	if (( entry )) fprintf( stream, "\"%s\"", entry->name ); }

//===========================================================================
//	db_arena_deregister
//===========================================================================
void
db_arena_deregister( CNInstance *e, CNDB *db )
/*
	Assumptions
		. called upon entity removal => e is no longer connected
		. e->sub[ 0 ]==NULL
*/ {
	uint *key = CNSharedKey( e );
	switch ( key[0] ) {
	case '$': ;
		Pair *entry = btreePick( DBSharedArena->name, key );
		if (( entry ))
			registryCBDeregister( entry->value, deregister_CB, db );
		break;
	case '!': ;
		Registry *ref = btreePick( DBSharedArena->value, key );
		if (( ref ))
			registryCBDeregister( ref, deregister_CB, db );
		break; } }

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
db_arena_translate( CNInstance *e, CNDB *db, int transpose )
/*
	Assumption: e->sub[ 0 ]==NULL
*/ {
	uint *key = CNSharedKey( e );
	Registry *ref;
	Pair *entry;
	switch ( key[0] ) {
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
	else if ( transpose )
		return db_share(key,ref,db);
	else return NULL; }

//===========================================================================
//	db_arena_unref
//===========================================================================
static shakeBTreeCB unref_CB;

void
db_arena_unref( CNDB *db )
/*
	Assumption: called by freeContext() right before freeCNDB()
*/ {
	btreeShake( DBSharedArena->name, unref_CB, NULL );
	btreeShake( DBSharedArena->value, unref_CB, NULL ); }

static int
unref_CB( uint key[2], void *value, void *user_data ) {
	switch ( key[0] ) {
	case '$':;
		Pair *entry = value;
		registryCBDeregister( entry->value, deregister_CB, NULL );
		break;
	case '!':;
		Registry *ref = value;
		registryCBDeregister( ref, deregister_CB, NULL );
		break; }
	return 1; }

