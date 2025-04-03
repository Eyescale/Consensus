#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "string_util.h"

//===========================================================================
//	p_prune
//===========================================================================
static char *prune_level( char * );
static char *prune_term( char *, PruneType );
static char *prune_ternary( char * );
static char *prune_sub( char * );
static char *prune_selection( char * );
static char *prune_mod( char * );
static char *prune_format( char * );
static char *prune_char( char * );
static char *prune_regex( char * );
static char *prune_list( char * );

char *
p_prune( PruneType type, char *p ) {
	switch ( type ) {
	case PRUNE_LEVEL:
		return prune_level( p );
	case PRUNE_TERM:
	case PRUNE_FILTER:
		return prune_term( p, type );
	case PRUNE_TERNARY:
		return prune_ternary( p );
	case PRUNE_LITERAL:
		return prune_sub( p );
	case PRUNE_LIST:
		return prune_list( p );
	case PRUNE_IDENTIFIER:
		switch ( *p ) {
		case '"':  return prune_format( p );
		case '\'': return prune_char( p );
		case '/':  return prune_regex( p );
		default:
			while ( !is_separator(*p) ) p++;
			return p; } } }

//---------------------------------------------------------------------------
//	prune_level
//---------------------------------------------------------------------------
static char *
prune_level( char *p ) {
	p = prune_term( p, PRUNE_TERM );
	for ( ; ; ) {
		switch ( *p ) {
		case '|':
			if ( *++p=='{' ) break;
			// no break
		case '(':
			p = prune_term( p, PRUNE_TERM );
			break;
		case '{':
			do p = prune_term( p+1, PRUNE_TERM );
			while ( *p!='}' );
			p++; break;
		case '<':
			do p = prune_term( p+1, PRUNE_TERM );
			while ( *p!='>' );
			p++; break;
		default:
			return p; } } }

//---------------------------------------------------------------------------
//	prune_term
//---------------------------------------------------------------------------
static char *
prune_term( char *p, PruneType type ) {
	int informed = 0;
	while ( *p ) {
		switch ( *p ) {
		case '(':
		case '{':
			p = prune_sub( p );
			if ( *p=='(' || *p=='{' || *p=='/' ) {
				return p; }
			informed = 1;
			break;
		case '?':
			if ( informed ) return p;
			informed = 1;
			p++; break;
		case ':':
			if ( p[1]=='|' || p[1]==':' ) {
				p+=2; break; }
			if ( type==PRUNE_FILTER )
				return p;
			informed = 0;
			p++; break;
		case '%':
			if ( p[1]=='(' ) {
				p++; break; }
			if ( p[1]=='!' ) {
				if ( p[2]=='/' )
					p = prune_selection( p );
				else p+=2; }
			else p = prune_mod( p );
			informed = 1;
			break;
		case '*':
			if ( p[1]=='^' ) {
				p += p[2]=='%'? 2 : 1;
				break; }
			// no break
		case '.':
			if ( p[1]=='?' || p[1]=='.' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			informed = 1; break;
		case '^':
			if ( p[1]=='^' || p[1]=='.' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			if ( *p=='~' ) p++;
			informed = 1; break;
		case '\'':
			p = prune_char( p );
			informed = 1; break;
		case '/':
			p = prune_regex( p );
			informed = 1;
			break;
		case '|':
			if ( type==PRUNE_FILTER )
				return p;
			else if ( p[1]=='^' ) {
				p++; break; }
			else return p;
		case '~':
		case '@':
			if ( p[1]=='<' )
				return p;
			p++; break;
		case '$':
		case '!':
			p++; break;
		case '"':
			p = prune_format( p );
			informed = 1; break;
		default:
			if ( !is_separator( *p ) ) {
				do p++; while ( !is_separator( *p ) );
				informed = 1; }
			else return p; } } // cases ,<>)}
	return p; }

//---------------------------------------------------------------------------
//	prune_ternary
//---------------------------------------------------------------------------
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
*/ {
	int start = *p++;
	int ternary = ( start=='?' || start==':' );
	char *candidate = NULL;
	int informed = 0;
	while ( *p ) {
		switch ( *p ) {
		case '(':
		case '{':
			p = prune_sub( p );
			if ( *p=='/' ) p++;
			informed = 1; break;
		case '?':
			if ( informed ) {
				if ( start=='(' ) goto RETURN;
				informed = 0; ternary++; }
			else informed = 1;
			p++; break;
		case ':':
			if ( p[1]=='|' || p[1]==':' ) {
				p+=2; break; }
			if ( ternary ) {
				ternary--;
				if ( start=='?' ) {
					candidate = p;
					if ( !ternary )
						goto RETURN; } }
			informed = 0;
			p++; break;
		case '%':
			if ( p[1]=='(' ) {
				p++; break; }
			else if ( p[1]=='!' ) {
				if ( p[2]=='/' )
					p = prune_selection( p );
				else p+=2; }
			else p = prune_mod( p );
			informed = 1;
			break;
		case '*':
			if ( p[1]=='^' ) {
				p += p[2]=='%'? 2 : 1;
				break; }
			// no break
		case '.':
			if ( p[1]=='?' || p[1]=='.' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			informed = 1; break;
		case '^':
			if ( p[1]=='^' || p[1]=='.' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			if ( *p=='~' ) p++;
			informed = 1; break;
		case '\'':
			p = prune_char( p );
			informed = 1; break;
		case '/':
			p = prune_regex( p );
			informed = 1;
			break;
		case '|':
			informed = 0;
			p++; break;
		case ',':
		case ')':
			goto RETURN;
		case '"':
			p = prune_format( p );
			informed = 1; break;
		default:
			do p++; while ( !is_separator(*p) );
			informed = 1; } }
RETURN:
	if (( candidate )) return candidate;
	else return p; }

//---------------------------------------------------------------------------
//	prune_sub
//---------------------------------------------------------------------------
static char *
prune_sub( char *p )
/*
	Assumption: *p=='(' or '{' and no format string authorized in level.
	return after closing ')' resp. '}'
*/ {
	int level = 0;
	int curly = *p=='{';
	while ( *p ) {
		switch ( *p ) {
		case '{':
			if ( !curly ) {
				p++; break; }
			// no break
		case '(':
			level++;
			p++; break;
		case '}':
			if ( !curly ) {
				p++; break; }
			// no break
		case ')':
			level--; p++;
			if ( !level ) return p;
			if ( *p=='/' ) p++;
			break;
		case '\'':
			p = prune_char( p );
			break;
		case '/':
			p = prune_regex( p );
			break;
		case '%':
			if ( p[1]=='(' ) p++; 
			else if ( p[1]=='!' )
				p += p[2]=='/' ? 3 : 2;
			else p = prune_mod( p );
			break;
		case '"':
			p = prune_format( p );
			break;
		case '.':
			if ( !strncmp( p, "...", 3 ) ) {
				if ( !strncmp( p+3, "):", 2 ) ) {
					p = prune_list( p );
					level--; // prune_list did skip "):"
					if ( level ) break;
					else return p; }
				else { p+=3; break; } }
			else if ( p[1]=='?' || p[1]=='.' ) {
				p+=2; break; }
			// no break;
		default:
			do p++; while ( !is_separator(*p) ); } }
	return p; }

//---------------------------------------------------------------------------
//	prune_selection
//---------------------------------------------------------------------------
static char *
prune_selection( char *p )
/*
	assumption: p = %!/(_)[:term[:term...]]/ where term: %(_) or ~%(_)
*/ {
	for ( p+=3; ; ) {
		p = prune_sub( p );
		if ( *p=='/' ) return p+1;
		p += p[1]=='%' ? 2 : 3; } }

//---------------------------------------------------------------------------
//	prune_mod
//---------------------------------------------------------------------------
static char *
prune_mod( char *p )
/*
	Assumption: p[1]!='('
*/ {
	switch ( p[1] ) {
	case '<':
		p+=2;
		switch ( *p ) {
		case '?': case '!':
			for ( p++; *p!='>'; )
				switch ( *p ) {
				case '(':  p = prune_sub( p ); break;
				case '\'': p = prune_char( p ); break;
				case '/':  p = prune_regex( p ); break;
				default: p++; }
			p++; break;
		case '(': p = prune_sub( p ) + 1; break;
		case ':': if ( p[1]==':' ) p+=3; break;
		case '.': p+=2; break; }
		break;
	case '?': case '!': case '|': case '%': case '@':
		p+=2; break;
	default:
		do p++; while ( !is_separator(*p) ); }
	return p; }

//---------------------------------------------------------------------------
//	prune_format
//---------------------------------------------------------------------------
static char *
prune_format( char *p )
/*
	Assumption: *p=='"'
*/ {
	p++; // skip opening '"'
	while ( *p ) {
		switch ( *p ) {
		case '"': return p+1;
		case '\\':
			if ( p[1] ) p++;
			// no break
		default:
			p++; } }
	return p; }

//---------------------------------------------------------------------------
//	prune_char
//---------------------------------------------------------------------------
static char *
prune_char( char *p )
/*
	Assumption: *p=='\''
*/ {
	return p + ( p[1]=='\\' ? p[2]=='x' ? 6 : 4 : 3 ); }

//---------------------------------------------------------------------------
//	prune_regex
//---------------------------------------------------------------------------
static char *
prune_regex( char *p )
/*
	Assumption: *p=='/'
*/ {
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
	return p; }

//---------------------------------------------------------------------------
//	prune_list
//---------------------------------------------------------------------------
static char *
prune_list( char *p )
/*
	Assumption: p points to the first dot of Ellipsis in
		((expression,...):sequence:)
			     ^-------- p
	return on first non-backslased ')', or '(' or '\0'
*/ {
	p += 5;
	while ( *p ) {
		switch ( *p ) {
		case ')':
		case '(':
			return p;
		case '\\':
			if ( p[1] ) p++;
			p++; break;
		default:
			p++; } }
	return p; }

