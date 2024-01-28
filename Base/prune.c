#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#include "string_util.h"

//===========================================================================
//	p_prune
//===========================================================================
static char *prune_ternary( char * );
static char *prune_base( char *, PruneType );
static char *prune_level( char *, int );
static char *prune_mod( char * );
static char *prune_format( char * );
static char *prune_char( char * );
static char *prune_regex( char * );
static char *prune_list( char * );

char *
p_prune( PruneType type, char *p )
/*
	Assumption: *p==':' => assume we start inside of TERNARY
*/ {
	switch ( type ) {
	case PRUNE_TERM:
		if ( *p==':' )
			return prune_ternary( p ); // return on closing ')'
		// no break
	case PRUNE_FILTER: ;
		return prune_base( p, type );
	case PRUNE_TERNARY:
		return prune_ternary( p );
	case PRUNE_LITERAL:
		return prune_level( p, 0 );
	case PRUNE_LIST:
		return prune_list( p );
	case PRUNE_IDENTIFIER:
		switch ( *p ) {
		case '"':  return prune_format( p );
		case '\'': return prune_char( p );
		case '/':  return prune_regex( p );
		case '(':  return prune_level( p, 0 );
		default:
			while ( !is_separator(*p) ) p++;
			return p; } } }

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
		case '%':
			if ( p[1]=='(' )
				{ p++; break; }
			p = prune_mod( p );
			informed = 1; break;
		case '\'':
			p = prune_char( p );
			informed = 1; break;
		case '/':
			p = prune_regex( p );
			informed = 1; break;
		case '*':
			if ( p[1]=='^' ) {
				p+=2; break; }
		case '.':
			if ( p[1]=='?' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			informed = 1; break;
		default:
			do p++; while ( !is_separator(*p) );
			informed = 1; } }
RETURN:
	if (( candidate )) return candidate;
	else return p; }

//---------------------------------------------------------------------------
//	prune_base
//---------------------------------------------------------------------------
static char *
prune_base( char *p, PruneType type ) {
	int informed = 0;
	while ( *p ) {
		switch ( *p ) {
		case '{':
		case '!':
		case '^':
			p++; break;
		case '(':
			p = prune_level( p, 0 );
			if ( *p=='(' )
				return p;
			informed = 1;
			break;
		case '~':
		case '@':
			if ( p[1]=='<' )
				return p;
			p++; break;
		case ':':
			if ( type==PRUNE_FILTER )
				return p;
			informed = 0;
			p++; break;
		case '?':
			if ( informed ) return p;
			informed = 1;
			p++; break;
		case '%':
			if ( p[1]=='(' )
				{ p++; break; }
			p = prune_mod( p );
			informed = 1; break;
		case '"':
			p = prune_format( p );
			informed = 1; break;
		case '\'':
			p = prune_char( p );
			informed = 1; break;
		case '/':
			p = prune_regex( p );
			informed = 1; break;
		case '*':
			if ( p[1]=='^' ) {
				p+=2; break; }
		case '.':
			if ( p[1]=='?' ) p+=2;
			else do p++; while ( !is_separator(*p) );
			informed = 1; break;
		default:
			if ( !is_separator( *p ) ) {
				do p++; while ( !is_separator( *p ) );
				informed = 1; }
			else {
				return p; } } } // cases ,<>)}|
	return p; }

//---------------------------------------------------------------------------
//	prune_level
//---------------------------------------------------------------------------
static char *
prune_level( char *p, int level )
/*
	Assumption: *p=='(' and no format string authorized in level
	return after closing ')'
*/ {
	while ( *p ) {
		switch ( *p ) {
		case '(':
			level++;
			p++; break;
		case ')':
			level--;
			if ( !level ) return p+1;
			p++; break;
		case '\\':
			if ( p[1] ) p++;
			p++; break;
		case '\'':
			p = prune_char( p );
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
		case '^':
		default:
			do p++; while ( !is_separator(*p) ); } }
	return p; }

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
		switch ( p[2] ) {
		case '?':
		case '!':
			for ( p+=3; *p!='>'; )
				switch ( *p ) {
				case '(':  p = prune_level( p, 0 ); break;
				case '\'': p = prune_char( p ); break;
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
	Assumption: p can be point to either one of the following
		((expression,...):sequence:)
		|	     ^------------------ p (Ellipsis)
		^------------------------------- p (expression)
	In the first case (Ellipsis) we return on the closing ')'
	In the second case (expression) we return after the closing ')'
*/ {
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
	return p; }

