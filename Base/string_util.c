#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "string_util.h"
#include "pair.h"

// #define DEBUG

/*===========================================================================
|
|			CNString Interface
|
+==========================================================================*/
//---------------------------------------------------------------------------
//	newString, freeString, ...
//---------------------------------------------------------------------------
CNString *
newString( void )
{
	union { int value; char *ptr; } icast;
	icast.value = CNStringBytes;
	return (CNString *) newPair( NULL, icast.ptr );
}
void
freeString( CNString *string )
{
	StringReset( string, CNStringAll );
	freePair( (Pair *) string );
}
int
StringStart( CNString *string, int event )
{
#ifdef DEBUG_2
	output( Debug, "StringStart: '%c'", (char) event );
#endif
	StringReset( string, CNStringAll );
	if ( event ) StringAppend( string, event );
	return 0;
}
int
StringAppend( CNString *string, int event )
/*
	Assumption: string->mode == CNStringBytes
*/
{
#ifdef DEBUG_2
	output( Debug, "StringAppend: '%c'", (char) event );
#endif
	add_item( (listItem**) &string->data, event );
	return 0;
}
char *
StringFinish( CNString *string, int trim )
{
#ifdef DEBUG_2
	output( Debug, "StringFinish: '%c'", (char) event );
#endif
	if ( string->mode == CNStringBytes ) {
		string->data = l2s((listItem **) &string->data, trim );
		string->mode = CNStringText; }
	return string->data;
}
char *
l2s( listItem **list, int trim )
{
	if ( *list == NULL ) return NULL;
	union { int value; void *ptr; } icast;
	listItem *i, *next_i;
	int count = 0;

	// remove trailing white space
	if ( trim ) {
		listItem *backup = NULL;
		i = *list;
		icast.ptr = i->ptr;
		if ((char) icast.value == '\n' ) {
			backup = i;
			*list = i->next; }	
		for ( i = *list; i!=NULL; i=next_i ) {
			icast.ptr = i->ptr;
			switch ( icast.value ) {
			case ' ':
			case '\t':
				next_i = i->next;
				if (( next_i )) {
					icast.ptr = next_i->ptr;
					if ( icast.value == '\\' ) {
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
			icast.ptr = i->ptr;
			switch ( icast.value ) {
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
		icast.ptr = i->ptr;
		*ptr++ = (char) icast.value; }
	*ptr = 0;
	freeListItem( list );
	return str;
}
void
StringReset( CNString *string, int target )
{
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
			break; } }
}
int
StringInformed( CNString *string )
{
	return ( string->data != NULL );
}
int
StringAt( CNString *string )
{
	if (( string->data ))
		switch ( string->mode ) {
		case CNStringBytes: ;
			union { int value; void *ptr; } icast;
			icast.ptr = ((listItem *) string->data )->ptr;
			return icast.value;
		case CNStringText:
			return *(char *)string->data; }
	return 0;
}
int
StringDiffer( CNString *string, char *cmp )
/*
	kind of strcmp() except starts from end
*/
{
	if ( string->mode==CNStringText )
		return strcmp((char *) string->data, cmp );

	union { int value; void *ptr; } icast;
	listItem *i = string->data;
	int j;
	for ( j=strlen(cmp), cmp+=j-1; j-- && (i); i=i->next ) {
		icast.ptr = i->ptr;
		int comparison = icast.value - *cmp--;
		if ( comparison ) return comparison; }
	return 0;
}

/*===========================================================================
|
|  			C string utilities
|
+==========================================================================*/
//---------------------------------------------------------------------------
//	is_separator, is_printable, is_space, is_escapable, is_xdigit
//---------------------------------------------------------------------------
int
is_separator( int event )
{
	return  ((( event > 96 ) && ( event < 123 )) ||		/* a-z */
		 (( event > 64 ) && ( event < 91 )) ||		/* A-Z */
		 (( event > 47 ) && ( event < 58 )) ||		/* 0-9 */
		  ( event == 95 )) ? 0 : 1;			/* _ */
}
int
is_letter( int event )
{
	return  ((( event > 96 ) && ( event < 123 )) ||		/* a-z */
		 (( event > 64 ) && ( event < 91 )) ||		/* A-Z */
		  ( event == 95 ));				/* _ */
}
int
is_digit( int event )
{
	return  (( event > 47 ) && ( event < 58 ));		/* 0-9 */
}
int
is_xdigit( int event )
{
	return  ((( event > 47 ) && ( event < 58 )) ||		/* 0-9 */
		 (( event > 64 ) && ( event < 71 )) ||		/* A-F */
		 (( event > 97 ) && ( event < 104 )));		/* a-f */
}
int
is_printable( int event )
{
	return ((event > 31 ) && ( event < 127 ));
}
int
is_space( int event )
{
	return ( event==9 || event==32 );
}
int
is_escapable( int event )
{
	// Note: includes neither 'x' nor '"'
	return strmatch( "0tn\'\\", event );
}

//---------------------------------------------------------------------------
//	isanumber, charscan
//---------------------------------------------------------------------------
int
isanumber( char *p )
{
	if (!((p) && *p )) return 0;
	do {
		if (( *p > 47 ) && ( *p < 58 )) { p++; }	/* 0-9 */
		else return 0;
	} while ( *p );
	return 1; 
}
#define HVAL(c) (c-((c<58)?48:(c<97)?55:87))
int
charscan( char *p, char_s *q )
{
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
		q->value = p[0]; return 1; }
}

//---------------------------------------------------------------------------
//	p_prune
//---------------------------------------------------------------------------
static char *prune_ternary( char * );
static char *prune_level( char *, int );
static char *prune_mod( char * );
static char *prune_literal( char * );
static char *prune_list( char * );
static char *prune_format( char * );
static char *prune_character( char * );
static char *prune_regex( char * );

char *
p_prune( PruneType type, char *p )
/*
	Assumption: *p==':' => assume we start inside of TERNARY
*/
{
	switch ( type ) {
	case PRUNE_TERM:
		if ( *p==':' ) // return on closing ')'
			return prune_ternary( p );
		// no break
	case PRUNE_FILTER: ;
		int informed = 0;
		while ( *p ) {
			switch ( *p ) {
			case '(':
				p = prune_level( p, 0 );
				informed = 1; break;
			case '~':
			case '@':
				if ( p[1]=='<' )
					return p;
				p++; break;
			case ',':
			case ')':
			case '<':
			case '>':
			case '}':
			case '|':
				return p;
			case ':':
				if ( type==PRUNE_FILTER )
					return p;
				informed = 0;
				p++; break;
			case '?':
				if ( informed ) return p;
				informed = 1;
				p++; break;
			case '"':
				p = prune_format( p );
				informed = 1; break;
			case '\'':
				p = prune_character( p );
				informed = 1; break;
			case '/':
				p = prune_regex( p );
				informed = 1; break;
			case '%':
				if ( p[1]=='(' )
					{ p++; break; }
				p = prune_mod( p );
				informed = 1; break;
			case '*':
				if ( p[1]=='?' ) p++;
				// no break
			default:
				// including case '.'
				do p++; while ( !is_separator(*p) );
				informed = 1; } }
		return p;
	case PRUNE_TERNARY:
		return prune_ternary( p );
	case PRUNE_LITERAL:
		return prune_literal( p );
	case PRUNE_LIST:
		return prune_list( p );
	case PRUNE_IDENTIFIER:
		switch ( *p ) {
		case '"':  p=prune_format( p ); break;
		case '\'': p=prune_character( p ); break;
		case '/':  p=prune_regex( p ); break;
		default:
			while ( !is_separator(*p) ) p++; }
		return p;
	}
}
static char *
prune_ternary( char *p )
/*
	Assumption: *p == either '(' or '?' or ':'
	if *p=='(' returns on
		the inner '?' operator if the enclosed is ternary
		the separating ',' or closing ')' otherwise
	if *p=='?' finds and returns the corresponding ':'
		assuming working from inside a ternary expression
	if *p==':' finds and returns the closing ')'
		assuming working from inside a ternary expression
	Generally speaking, prune_ternary() can be said to proceed from inside
	a [potential ternary] expression, whereas p_prune proceeds from outside
	the expression it is passed. In both cases, the syntax is assumed to
	have been checked beforehand.
*/
{
	char *	candidate = NULL;
	int	start = *p++,
		informed = 0,
		ternary = 0;
	if ( start=='?' || start==':' )
		ternary = 1;
	while ( *p ) {
		switch ( *p ) {
		case '(':
			p = prune_level( p, 0 );
			informed = 1; break;
		case ',':
		case ')':
			goto RETURN;
		case '?':
			if ( informed ) {
				if ( start=='(' ) goto RETURN;
				ternary++;
				informed = 0; }
			else informed = 1;
			p++; break;
		case ':':
			if ( ternary ) {
				ternary--;
				if ( start=='?' ) {
					candidate = p;
					if ( !ternary )
						goto RETURN; } }
			informed = 0;
			p++; break;
		case '\'':
			p = prune_character( p );
			informed = 1; break;
		case '/':
			p = prune_regex( p );
			informed = 1; break;
		case '%':
			if ( p[1]=='(' )
				{ p++; break; }
			p = prune_mod( p );
			informed = 1; break;
		case '*':
			if ( p[1]=='?' ) p++;
			// no break
		default:
			do p++; while ( !is_separator(*p) );
			informed = 1; } }
RETURN:
	if (( candidate )) return candidate;
	else return p;
}
static char *
prune_level( char *p, int level )
/*
	Assumption: *p=='('
	return after closing ')'
*/
{
	while ( *p ) {
		switch ( *p ) {
		case '(':
			if ( p[1]==':' ) {
				p = prune_literal( p );
				if ( !level ) return p;
				break; }
			level++;
			p++; break;
		case ')':
			level--;
			if ( !level ) return p+1;
			p++; break;
		case '\'':
			p = prune_character( p );
			break;
		case '/':
			p = prune_regex( p );
			break;
		case '.':
			if ( p[1]=='.' ) {
				if ( p[2]=='.' ) {
					// level should be at least 2 for list
					if ( level && !strncmp( p+3, "):", 2 ) ) {
						p = prune_list( p );
						level--; // here *p==')'
						if ( !level ) return p; }
					else p+=3; }
				else p+=2;
				break; }
			// no break
		default:
			do p++; while ( !is_separator(*p) ); } }
	return p;
}
static char *
prune_mod( char *p )
/*
	Assumption: p[1]!='('
*/
{
	switch ( p[1] ) {
	case '<':
		switch ( p[2] ) {
		case '?':
		case '!':
			for ( p+=3; *p!='>'; )
				switch ( *p ) {
				case '(':  p = prune_level( p, 0 ); break;
				case '\'': p = prune_character( p ); break;
				case '/':  p = prune_regex( p ); break;
				default:
					p++; }
			p++; break;
		default:
			p+=2; }
		break;
	case '?':
	case '!':
	case '|':
	case '%':
		p+=2; break;
	default:
		do p++; while ( !is_separator(*p) ); }
	return p;
}
static char *
prune_literal( char *p )
/*
	Assumption: p==(:sequence:) or p==(:sequence)
*/
{
	p+=2; // skip opening "(:"
	while ( *p ) {
		switch ( *p ) {
		case ':':
			if ( p[1]==')' )
				return p+2;
			p++; break;
		case ')':
			return p+1;
		case '\\':
			if ( p[1] ) p++;
			// no break
		default:
			p++; } }
	return p;
}
static char *
prune_list( char *p )
/*
	Assumption: p can be point to either one of the following
		((expression,...):sequence:)
		|	     ^------------------ p (Ellipsis)
		^------------------------------- p (expression)
	In the first case (Ellipsis) we return on the closing ')'
	In the second case (expression) we return after the closing ')'
*/
{
	if ( *p=='(' ) // not called from prune_level
		return prune_level( p, 0 );
	p += 5;
	while ( *p ) {
		switch ( *p ) {
		case ':':
			if ( p[1]==')' )
				return p+1;
			p++; break;
		case '\\':
			if ( p[1] ) p++;
			// no break
		default:
			p++; } }
	return p;
}
static char *
prune_format( char *p )
/*
	Assumption: *p=='"'
*/
{
	p++; // skip opening '"'
	while ( *p ) {
		switch ( *p ) {
		case '"': return p+1;
		case '\\':
			if ( p[1] ) p++;
			// no break
		default:
			p++; } }
	return p;
}
static char *
prune_character( char *p )
/*
	Assumption: *p=='\''
*/
{
	return p + ( p[1]=='\\' ? p[2]=='x' ? 6 : 4 : 3 );
}
static char *
prune_regex( char *p )
/*
	Assumption: *p=='/'
*/
{
	p++; // skip opening '/'
	int bracket = 0;
	while ( *p ) {
		switch ( *p ) {
		case '/':
			if ( !bracket )
				return p+1;
			p++; break;
		case '\\':
			if ( p[1] ) p++;
			p++; break;
		case '[':
			bracket = 1;
			p++; break;
		case ']':
			bracket = 0;
			p++; break;
		default:
			p++; } }
	return p;
}

//---------------------------------------------------------------------------
//	p_ternary, p_single, p_filtered, p_list
//---------------------------------------------------------------------------
int
p_ternary( char *p )
/*
	Assumption: *p=='('
*/
{
	p = p_prune( PRUNE_TERNARY, p );
	return ( *p=='?' );
}
int
p_single( char *p )
/*
	Assumption: *p=='('
*/
{
	p = p_prune( PRUNE_TERM, p+1 );
	return ( *p!=',' );
}
int
p_filtered( char *p )
{
	p = p_prune( PRUNE_FILTER, p );
	return ( *p==':' );
}
int
p_list( char *p )
/*
	Assumption: *p=='('
	We want to determine if p is ((expression,...):sequence:)
		Ellipsis -------------------------^
*/
{
	if ( p[1]=='(' ) {
		p = p_prune( PRUNE_TERM, p+2 );
		return !strncmp( p, ",...", 4 ); }
	return 0;
}

//---------------------------------------------------------------------------
//	strmake
//---------------------------------------------------------------------------
char *
strmake( char *p )
{
	CNString *s = newString();
	if ( is_separator(*p) )
		StringAppend( s, *p );
	else do {
		StringAppend( s, *p++ );
	} while ( !is_separator(*p) );
	p = StringFinish( s, 0 );
	StringReset( s, CNStringMode );
	freeString( s );
	return p;
}

//---------------------------------------------------------------------------
//	strcomp
//---------------------------------------------------------------------------
static int equivocal( char *regex, char *p );
static int rxnext( char *regex, char **r );
int
strcomp( char *p, char *q, int cmptype )
{
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
	case 2:; // p is a regex, q is null-terminated
		char *r = p+1; // skip the leading '/'
#define RXCMP(r,e) \
		(( *r=='\0' || *r=='/' ) ? e : ( e ? rxcmp(r,e) : -1 ))
		int comparison;
		for ( ; *q; q++, rxnext(p,&r) ) {
			if (( comparison = RXCMP( r, *q ) )) {
				if ( equivocal(p,r) ) return -1;
				else return comparison; } }
		if (( comparison = RXCMP( r, *q ) )) {
			if ( equivocal(p,r) ) return -1;
			else return comparison; }
		return 0;
	default:
		return 1; }
}
int
rxcmp( char *r, int event )
{
	int delta, not;
	char_s q;
	switch ( *r ) {
	case '.':
		return 0;
	case '[':
		if ( r[1]=='^' ) { not=1; r++; }
		else not=0;
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
		return event - *(const unsigned char*)r; }
}
static int
equivocal( char *regex, char *p )
{
	switch ( *p ) {
	case '.':
	case '[':
		return 1;
	case '\\':
		switch ( p[1] ) {
		case 'd':
		case 'h':
		case 'l':
			return 1; } }
	char *r = regex+1; // skip the leading '/'
	while ( r != p ) {
		switch ( *r ) {
		case '.':
		case '[':
			return 1;
		case '\\':
			switch ( r[1] ) {
			case 'd':
			case 'h':
			case 'l':
				return 1;
			}
			r += ( r[1]=='x' ? 4 : 2 );
			break;
		default:
			r++; } }
	return 0;
}
static int
rxnext( char *regex, char **p )
{
	char *r = *p;
	if ( *r=='/' ) return 0;
	int bracket = 0;
	do {
		switch ( *r ) {
		case '\0':
			return 0;
		case '[':
			bracket = 1;
			r++; break;
		case ']':
			bracket = 0;
			r++; break;
		case '\\':
			r += ( r[1]=='x' ? 4 : 2 );
			break;
		default:
			r++; }
	} while ( bracket );
	*p = r;
	return 1;
}

//---------------------------------------------------------------------------
//	strscanid
//---------------------------------------------------------------------------
char *
strscanid( char *str, char **identifier )
{
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
	return str + n;
}

//---------------------------------------------------------------------------
//	strmatch
//---------------------------------------------------------------------------
int
strmatch( char *s, int event )
{
	if ( *s ) {
		do { if ( event == *s++ ) return 1; } while ( *s );
		return 0; }
	return event ? 0 : 1;
}

//---------------------------------------------------------------------------
//	strskip
//---------------------------------------------------------------------------
char *
strskip( char *fmt, char *str )
{
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
	return str;
}

//---------------------------------------------------------------------------
//	strxchange
//---------------------------------------------------------------------------
char *
strxchange( char *expression, char *identifier, char *value )
{
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
	return retval;
}

