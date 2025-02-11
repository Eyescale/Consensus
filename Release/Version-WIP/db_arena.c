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

static freeRegistryCB deregister_CB; // see below db_arena_deregister
static int
free_CB( uint key[2], void *value ) {
	switch ( key[0] ) {
	case '$': ;
		Pair *entry = value;
		free( entry->name );
		freeRegistry( entry->value, deregister_CB );
		freePair((Pair *) entry );
		break;
	case '!':
		freeRegistry( value, deregister_CB ); }
	return 1; }

//===========================================================================
//	db_arena_encode / db_arena_key
//===========================================================================
/*
	note that code stores least significant hextet first
*/
#define i2hex( i ) (i&=63,i+offset(i))
static inline int offset( int );
static shakeBTreeCB strcmp_CB;
typedef struct {
	char *s; uint index; Registry *ref; } StrcmpData;

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
	do StringAppend( code, i2hex(index) ); while (( index>>=6 )); }

static int
strcmp_CB( uint key[2], void *value, void *user_data ) {
	StrcmpData *data = user_data;
	Pair *entry = value;
	if ( !strcmp(entry->name,data->s) ) {
		data->index = key[ 1 ];
		data->ref = entry->value;
		return 0; }
	return 1; }

static inline int
offset( int tmp ) {
	return	( tmp < 58 ? 48 : // 0-9: 00-09
		  tmp < 91 ? 55 : // A-Z: 10-35
		  tmp > 96 ? 61 : // a-z: 36-61
		  32 ); } // ^_: 62-63

//---------------------------------------------------------------------------
//	db_arena_key
//---------------------------------------------------------------------------
#define hex2i( h ) (h-offset(h))
void *
db_arena_key( char *code ) {
	union { uint value[2]; void *ptr; } key;
	key.value[ 0 ] = '$';
	key.value[ 1 ] = 0;
	code++; // skip leading '"'
	for ( int shift=0; *code!='"'; code++, shift+=6 )
		key.value[ 1 ] |= hex2i(*code) << shift;
	return key.ptr; }

//===========================================================================
//	db_arena_register
//===========================================================================
CNInstance *
db_arena_register( char *code, CNDB *db ) {
	union { uint value[2]; void *ptr; } key;
	Registry *ref;
	if (( code )) {
		key.ptr = db_arena_key( code );
		Pair *entry = btreeLookup( DBSharedArena->name, key.value );
		ref = entry->value; }
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
	Pair *entry = btreeLookup( DBSharedArena->name, key.value );
	if (( entry )) {
		entry = registryLookup( entry->value, db );
		return (( entry )? entry->value : NULL ); }
	return NULL; }

//===========================================================================
//	db_arena_makeup
//===========================================================================
CNInstance *
db_arena_makeup( CNInstance *e, CNDB *db, CNDB *db_dst )
/*
	build $(e,?:...) and return shared arena string
*/ {
	if ( !e || db_private(0,e,db)) return NULL;
	CNString *string = newString();
	for ( listItem *i=e->as_sub[ 0 ]; i!=NULL; ) {
		if ( e=i->ptr, !db_private(0,e,db) ) {
			CNInstance *f = e->sub[ 1 ];
			if ( !cnIsIdentifier(f) ) break;
			int event = *CNIdentifier( f );
			if ( !event ) break;
			StringAppend( string, event );
			i = e->as_sub[ 0 ]; }
		else i = i->next; }
	if ( !StringInformed(string) ) return NULL;
	CNString *code = newString();
	StringAppend( code, '"' );
	db_arena_encode( code, string );
	StringAppend( code, '"' );
	freeString( string );
	char *s = StringFinish(code,0);
	e = db_arena_register( s, db_dst );
	freeString( code );
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
	Pair *entry = btreeLookup( DBSharedArena->name, key.value );
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
		Pair *entry = btreeLookup( DBSharedArena->name, key );
		if (( entry ))
			registryCBDeregister( entry->value, deregister_CB, db );
		break;
	case '!': ;
		Registry *ref = btreeLookup( DBSharedArena->value, key );
		if (( ref ))
			registryCBDeregister( ref, deregister_CB, db );
		break; } }

static void
deregister_CB( Registry *registry, Pair *entry ) {
	CNInstance *e = entry->value;
	e->sub[ 1 ] = NULL;
	cn_prune( e ); }

//===========================================================================
//	db_arena_identifier
//===========================================================================
char *
db_arena_identifier( CNInstance *e )
/*
	Assumption: e->sub[0]==NULL
*/ {
	uint *key = CNSharedKey( e );
	Pair *entry = btreeLookup( DBSharedArena->name, key );
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
		entry = btreeLookup( DBSharedArena->name, key );
		if ( !entry ) return NULL;
		ref = entry->value;
		break;
	case '!':
		ref = btreeLookup( DBSharedArena->value, key );
		if ( !ref ) return NULL;
		break; }

	entry = registryLookup( ref, db );
	if (( entry )) return entry->value;
	else if ( transpose )
		return db_share( key, ref, db );
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

