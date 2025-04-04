#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "string_util.h"

// #define DEBUG

/*===========================================================================
|
|			CNString Interface
|
+==========================================================================*/
//---------------------------------------------------------------------------
//	newString, freeString
//---------------------------------------------------------------------------
CNString *
newString( void ) {
	int type = CNStringBytes;
	return (CNString *) newPair( NULL, cast_ptr(type) ); }

void
freeString( CNString *string ) {
	StringReset( string, CNStringAll );
	freePair( (Pair *) string ); }

//---------------------------------------------------------------------------
//	StringStart, StringAppend, StringAffix
//---------------------------------------------------------------------------
int
StringStart( CNString *string, int event ) {
#ifdef DEBUG_2
	output( Debug, "StringStart: '%c'", (char) event );
#endif
	StringReset( string, CNStringAll );
	if ( event ) StringAppend( string, event );
	return 0; }

int
StringAppend( CNString *string, int event )
/*
	Assumption: string->mode == CNStringBytes
*/ {
#ifdef DEBUG_2
	output( Debug, "StringAppend: '%c'", event );
#endif
	add_item( (listItem**) &string->data, event );
	return 0; }

int
StringAffix( CNString *string, int event )
/*
	Assumption: string->mode == CNStringBytes
*/ {
#ifdef DEBUG_2
	output( Debug, "StringAffix: '%c'", event );
#endif
	listItem *list = string->data;
	if ( !list ) return StringAppend( string, event );
	while (( list->next )) list = list->next;
	list->next = new_item( event );
	return 0; }

//---------------------------------------------------------------------------
//	StringInformed, StringAt, StringCompare
//---------------------------------------------------------------------------
int
StringInformed( CNString *string ) {
	return ( string->data != NULL ); }

int
StringAt( CNString *string ) {
	if (( string->data ))
		switch ( string->mode ) {
		case CNStringBytes: ;
			return cast_i( ((listItem *) string->data )->ptr );
		case CNStringText:
			return *(char *)string->data; }
	return 0; }

int
StringCompare( CNString *string, char *cmp )
/*
	kind of strcmp() except starts from end
*/ {
	if ( string->mode==CNStringText )
		return strcmp((char *) string->data, cmp );

	listItem *i = string->data;
	if ( !i ) return 1;
	int len = strlen( cmp );
	if ( !len ) return 1;
	int j;
	for ( j=0, cmp+=len-1; j<len && (i); i=i->next, j++ ) {
		int comparison = cast_i(i->ptr) - *cmp--;
		if ( comparison ) return comparison; }
	return !( j==len && !i ); }

//---------------------------------------------------------------------------
//	StringFinish, StringReset, StringDup
//---------------------------------------------------------------------------
static char *l2s( listItem **list, int trim );

char *
StringFinish( CNString *string, int trim ) {
#ifdef DEBUG_2
	output( Debug, "StringFinish: '%c'", (char) event );
#endif
	if ( string->mode == CNStringBytes ) {
		string->data = l2s((listItem **) &string->data, trim );
		string->mode = CNStringText; }
	return string->data; }

static char *
l2s( listItem **list, int trim ) {
	if ( *list == NULL ) return NULL;
	listItem *i, *next_i;
	int count = 0;

	// remove trailing white space
	if ( trim ) {
		listItem *backup = NULL;
		i = *list;
		if ( cast_i(i->ptr)=='\n' ) {
			backup = i;
			*list = i->next; }	
		for ( i = *list; i!=NULL; i=next_i ) {
			switch ( cast_i(i->ptr) ) {
			case ' ':
			case '\t':
				next_i = i->next;
				if (( next_i )) {
					if ( cast_i(next_i->ptr)=='\\' ) {
						next_i = NULL;
						break; } }
				freeItem( i );
				*list = next_i;
				break;
			default:
				next_i = NULL; } }
		if ( backup ) {
			backup->next = *list;
			*list = backup; } }

	count = reorderListItem( list );

	// remove leading white space
	if ( trim ) {
		for ( i = *list; i!=NULL; i=next_i ) {
			switch ( cast_i(i->ptr) ) {
			case ' ':
			case '\t':
				count--;
				next_i = i->next;
				freeItem( i );
				*list = next_i;
				break;
			default:
				next_i = NULL; } } }

	// allocate & inform string
	char *str = (char *) malloc( count + 1 );
	char *ptr = str;
	for ( i = *list; i!=NULL; i=i->next ) {
		*ptr++ = (char) cast_i(i->ptr); }
	*ptr = 0;
	freeListItem( list );
	return str; }

void
StringReset( CNString *string, CNStringReset target ) {
	switch ( target ) {
	case CNStringMode:
		string->data = NULL;
		string->mode = CNStringBytes;
		break;
	case CNStringAll:
		switch ( string->mode ) {
		case CNStringBytes:
			freeListItem( (listItem **) &string->data );
			break;
		case CNStringText:
			free( string->data );
			string->data = NULL;
			string->mode = CNStringBytes;
			break; } } }

CNString *
StringDup( CNString *string ) {
	CNString *s = newString();
	int mode = s->mode = string->mode;
	if ( !string->data ) s->data = NULL;
	else switch ( mode ) {
	case CNStringBytes: ;
		listItem **data = (listItem **) &s->data;
		for ( listItem *i=string->data; i!=NULL; i=i->next )
			addItem( data, i->ptr );
		reorderListItem((listItem **) &s->data );
		break;
	case CNStringText:
		s->data = strdup(string->data); }
	return s; }

/*===========================================================================
|
|  			C string utilities
|
+==========================================================================*/
//---------------------------------------------------------------------------
//	is_separator, is_printable, is_space, is_escapable, is_xdigit
//---------------------------------------------------------------------------
int
is_separator( int event ) {
	return  ((( event > 96 ) && ( event < 123 )) ||		/* a-z */
		 (( event > 64 ) && ( event < 91 )) ||		/* A-Z */
		 (( event > 47 ) && ( event < 58 )) ||		/* 0-9 */
		  ( event == 95 )) ? 0 : 1; }			/* _ */

int
is_letter( int event ) {
	return  ((( event > 96 ) && ( event < 123 )) ||		/* a-z */
		 (( event > 64 ) && ( event < 91 )) ||		/* A-Z */
		  ( event == 95 )); }				/* _ */

int
is_digit( int event ) {
	return  (( event > 47 ) && ( event < 58 )); }		/* 0-9 */

int
is_xdigit( int event ) {
	return  ((( event > 47 ) && ( event < 58 )) ||		/* 0-9 */
		 (( event > 64 ) && ( event < 71 )) ||		/* A-F */
		 (( event > 97 ) && ( event < 104 ))); }	/* a-f */

int
is_printable( int event ) {
	return ((event > 31 ) && ( event < 127 )); }

int
is_space( int event ) {
	return ( event==9 || event==32 ); }

int
is_escapable( int event ) {
	// Note: includes neither 'x' nor '"'
	return strmatch( "0tn\'\\", event ); }

//---------------------------------------------------------------------------
//	isanumber, charscan
//---------------------------------------------------------------------------
int
isanumber( char *p ) {
	if (!((p) && *p )) return 0;
	do {
		if (( *p > 47 ) && ( *p < 58 )) { p++; }	/* 0-9 */
		else return 0;
		} while ( *p );
	return 1; }

#define HVAL(c) (c-((c<58)?48:(c<97)?55:87))
int
charscan( char *p, char_s *q ) {
	switch ( *p ) {
	case '\\':
		switch ( p[1] ) {
		case 'x':  q->value = HVAL(p[2])<<4 | HVAL(p[3]); return 4;
		case '0':  q->value = '\0'; return 2;
		case 't':  q->value = '\t'; return 2;
		case 'n':  q->value = '\n'; return 2;
		case '\\': q->value = '\\'; return 2;
		case '\'': q->value = '\''; return 2;
		case '"': q->value = '"'; return 2;
		default: return 0; }
		break;
	default:
		q->value = p[0]; return 1; } }

//---------------------------------------------------------------------------
//	strmake
//---------------------------------------------------------------------------
char *
strmake( char *p ) {
	CNString *s = newString();
	if ( is_separator(*p) )
		StringAppend( s, *p );
	else do {
		StringAppend( s, *p++ );
		} while ( !is_separator(*p) );
	p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return p; }

//---------------------------------------------------------------------------
//	strcomp
//---------------------------------------------------------------------------
static int rxcomp( char *regex, char *p );

int
strcomp( char *p, char *q, int cmptype ) {
	switch ( cmptype ) {
	case 0:	// both p and q are null-terminated
		for ( ; *p; p++, q++ ) {
			if ( *p == *q ) continue;
			return *(const unsigned char*)p - *(const unsigned char*)q; }
		return - *(const unsigned char*)q;
	case 1:	// both p and q are separator-terminated
		if ( is_separator(*p) ) {
			if ( !is_separator(*q) ) return - *(const unsigned char*)q;
			return *(const unsigned char*)p - *(const unsigned char*)q; }
		else if ( is_separator(*q) ) {
			return *(const unsigned char*)p; }
		else for ( ; !is_separator(*p); p++, q++ ) {
			if ( *p == *q ) continue;
			return is_separator(*q) ?
				*(const unsigned char*)p :
				*(const unsigned char*)p - *(const unsigned char*)q; }
		return is_separator(*q) ? 0 : - *(const unsigned char*)q;
	case 2:
		return rxcomp( p, q );
	default:
		return 1; } }

static inline char *rxnext( char *r );
static int
rxcomp( char *regex, char *p )
/*
	regex is assumed syntactically correct
	p is null-terminated
*/ {
	listItem *backup = NULL;
	int cmp=0, level=0, prune=0;
	char *r = regex + 1;
	for ( ; ; ) {
		switch ( *r ) {
		case '/':
			return *p;
		case '|':
			switch ( prune ) {
			case -1: prune=0; p=backup->ptr; break;
			case 0: prune=1; break; }
			r++; break;
		case '(':
			level++;
			if ( !prune ) addItem( &backup, p );
			else if ( prune > 0 ) prune++;
			else if ( prune < 0 ) prune--;
			r++; break;
		case ')':
			level--;
			if ( !prune ) popListItem( &backup );
			else if ( prune > 0 ) {
				if ( !--prune )
					popListItem( &backup ); }
			else if ( prune < 0 ) {
				if ( !++prune ) {
					popListItem( &backup );
					if ( level ) prune = -1;
					else return cmp; } }
			r++; break;
		default:
			if ( !prune ) {
				if ( !*p ) {
					freeListItem( &backup );
					return rxcmp( r, '\0' ); }
				else if (( cmp = rxcmp( r, *p++ ) )) {
					if ( level ) prune = -1;
					else return cmp; } }
			r = rxnext( r ); } } }

#define r_step ( *r=='\\' ? r[1]=='x' ? 4:2:1 )
static inline char *
rxnext( char *r ) {
	if ( *r=='[' ) {
		do r += r_step; while ( *r!=']' );
		return r+1; }
	else return ( r + r_step ); }

//---------------------------------------------------------------------------
//	rxcmp
//---------------------------------------------------------------------------
int
rxcmp( char *r, int event ) {
	int delta, not;
	char_s q;
	switch ( *r ) {
	case '.':
		return 0;
	case '[':
		if ( r[1]=='^' ) { not=1; r++; }
		else not = 0;
		for ( r++; *r && *r!=']'; ) {
			int range[ 2 ];
			switch ( *r ) {
			case '\\': 
				delta = charscan( r, &q );
				if ( delta ) { range[0]=q.value; r+=delta; }
				else { range[0]=r[1]; r+=2; }
				break;
			default:
				range[0] = *r++; }
			if ( *r == '-' ) {
				r++;
				switch ( *r ) {
				case '\0': break;
				case '\\': 
					delta = charscan( r, &q );
					if ( delta ) { range[1]=q.value; r+=delta; }
					else { range[1]=r[1]; r+=2; }
					break;
				default:
					range[1] = *r++; }
				if ( event>=range[0] && event<=range[1] )
					return ( not ? -1 : 0 ); }
			else if ( event==range[0] ) return ( not ? -1 : 0 ); }
		return ( not ? 0 : -1 ); // no match
	case '\\':
		switch ( r[1] ) {
		case 'd': return is_digit(event) ? 0 : -1;
		case 'l': return is_letter(event) ? 0 : -1;
		case 'h': return is_xdigit(event) ? 0 : -1; }
		return event - ( charscan(r,&q) ?
			*(const unsigned char*)q.s : *(const unsigned char*)(r+1) );
	default:
		return event - *(const unsigned char*)r; } }

//---------------------------------------------------------------------------
//	strscanid
//---------------------------------------------------------------------------
char *
strscanid( char *str, char **identifier ) {
	*identifier = NULL;

	// measure identifier size
	char *ptr = strskip( "%.", str++ );
	if ( ptr == NULL ) return NULL;

	int n = (uintptr_t) ptr - (uintptr_t) str;
	if ( !n ) return NULL;

	// allocate and inform identifier
	ptr = malloc( n + 1 );
	memcpy( ptr, str, n );
	ptr[ n ] = (char) 0;

	*identifier = ptr;
	return str + n; }

//---------------------------------------------------------------------------
//	strmatch
//---------------------------------------------------------------------------
int
strmatch( char *s, int event ) {
	if ( *s=='\0' )
		return !event;
	else do {
		if ( event==*s++ ) return 1;
		} while ( *s );
	return 0; }

//---------------------------------------------------------------------------
//	strskip
//---------------------------------------------------------------------------
char *
strskip( char *fmt, char *str ) {
	if ( str == NULL ) return NULL;
	for ( ; *fmt; fmt++ ) {
		switch ( *fmt ) {
		case ' ': // skip space: { \ \t }
			for ( int done=0; *str && !done; )
				switch ( *str ) {
				case ' ':
				case '\t':
					str++;
					break;
				default:
					done = 1; }
			break;
		case '.': // skip identifier: { [A-Za-z0-9_] }
			for ( int done=0; *str && !done; ) {
				if ( !is_separator( *str ) )
					str++;
				else done = 1; }
			break;
		case '\\': // to allow the above in the format
			fmt++;
			switch ( *str ) {
			case '\0': if ( *fmt == '0' )
					str++;
				else return NULL;
				break;
			default:
				switch ( *fmt ) {
				case '0':
					return NULL;
				case 't': if (*str++!='\t')
						return NULL;
					break;
				case 'n': if (*str++ != '\n' )
						return NULL;
					break;
				case '\\': if (*str++ != '\\' )
						return NULL;
					break;
				default: if ( *str++ != *fmt )
						return NULL; }
				break; }
			break;
		default: // must match
			switch ( *str ) {
			case '\0':
				return NULL;
			default: if ( *str++ != *fmt )
				return NULL; } } }
	return str; }

//---------------------------------------------------------------------------
//	strxchange
//---------------------------------------------------------------------------
char *
strxchange( char *expression, char *identifier, char *value ) {
	int	i, pos,
		m = strlen( value ),
		n = strlen( identifier ),
		maxpos = strlen( expression ) - n;
	char *	ptr1 = expression, *ptr2, *ptr3, *ptr4;

	CNString *result = newString();
	for ( pos=0; ( pos < maxpos ) && *ptr1; ) {
		pos++;
		if ( *ptr1 != '%' ) {
			if ( result->data != NULL )
				StringAppend( result, *ptr1 );
			ptr1++;
			continue; }
		ptr1++;

		int found = 1;
		ptr2 = identifier;
		for ( i=0; (i<n) && found; i++ ) {
			// output( Debug, "\ncomparing %c %c\n", ptr1[i], *ptr2 );
			if ( ptr1[ i ] != *ptr2++ )
				found = 0; }
		if ( !found ) {
			if ( result->data != NULL )
				StringAppend( result, '%' );
			continue; }

		if ( pos == 1 ) {
			ptr4 = value;
			StringStart( result, *ptr4++ );
			for ( i=1; i<m; i++ )
				StringAppend( result, *ptr4++ ); }
		else {
			if ( result->data == NULL ) {
				ptr3 = expression;
				StringStart( result, *ptr3++ );
				for ( i=2; i<pos; i++ )
					StringAppend( result, *ptr3++ ); }
			ptr4 = value;
			for ( i=0; i<m; i++ ) {
				StringAppend( result, *ptr4++ ); } }
		pos += n; ptr1 += n; }

	char *retval;
	if ( result->data != NULL ) {
		n = strlen( expression );
		for ( ; pos<n; pos++ ) {
			StringAppend( result, *ptr1++ ); }
		retval = StringFinish( result, 0 ); }
	else retval = expression;

	freeString( result );
	return retval; }

