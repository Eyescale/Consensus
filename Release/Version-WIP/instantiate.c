#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "narrative.h"
#include "expression.h"
#include "locate.h"
#include "feel.h"

// #define DEBUG

//===========================================================================
//	bm_instantiate
//===========================================================================
static listItem *bm_couple( listItem *sub[2], CNDB * );
static CNInstance *bm_literal( char *, CNDB * );

void
bm_instantiate( char *expression, BMContext *ctx )
{
#ifdef DEBUG
	if ( bm_void( expression, ctx ) ) {
		fprintf( stderr, ">>>>> B%%: bm_instantiate(): VOID: %s\n", expression );
		exit( -1 );
	}
	else fprintf( stderr, "bm_instantiate: %s {\n", expression );
#endif
	struct { listItem *results, *flags, *level; } stack;
	memset( &stack, 0, sizeof(stack) );
	listItem *sub[ 2 ] = { NULL, NULL }, *instances;
	CNInstance *e;
	CNDB *	db = BMContextDB( ctx );
	int	done=0, level=0, ndx=0, piped=0,
		empty = db_is_empty(0,db);
	char *	p = expression;
	while ( *p && !done ) {
		if ( p_filtered( p )  ) {
			sub[ ndx ] = bm_query( p, ctx );
			if (( sub[ndx] )) {
				p = p_prune( PRUNE_TERM, p );
				continue;
			}
			else { done=-1; break; }
		}
		switch ( *p ) {
		case '%':
			if ( p[1]=='?' ) {
				CNInstance *e = bm_context_lookup( ctx, "?" );
				if (( e )) {
					sub[ ndx ] = newItem( e );
				}
				else { done=-1; break; }
				p+=2; break;
			}
			else if ( p[1]=='!' ) {
				listItem *i = bm_context_lookup( ctx, "!" );
				if ( !i ) { done=-1; break; }
				for ( ; i!=NULL; i=i->next )
					addItem( &sub[ ndx ], i->ptr );
				p+=2; break;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)}", p[1] ) ) {
				e = bm_register( ctx, p );
				sub[ ndx ] = newItem( e );
				p++; break;
			}
			// no break
		case '~':
			if (( sub[ ndx ] = bm_query( p, ctx ) )) {
				p = p_prune( PRUNE_TERM, p+1 );
				break;
			}
			else { done=-1; break; }
		case '{':
			add_item( &stack.level, level );
			level = 0;
			add_item( &stack.flags, ndx );
			if ( ndx ) {
				addItem( &stack.results, sub[ 0 ] );
				ndx=0; sub[ 0 ]=NULL;
			}
			addItem( &stack.results, NULL );
			p++; break;
		case '}':
			/* assumption here: level==0
			*/
			if ( !(stack.level)) { done=1; break; }
			level = pop_item( &stack.level );
			instances = popListItem( &stack.results );
			instances = catListItem( instances, sub[ 0 ] );
			ndx = pop_item( &stack.flags );
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '|':
			bm_push_mark( ctx, '!', sub[ ndx ] );
			add_item( &stack.flags, ndx );
			add_item( &stack.level, level );
			addItem( &stack.results, sub[ 0 ] );
			if ( ndx ) {
				addItem( &stack.results, sub[ 1 ] );
				ndx = 0; sub[ 1 ] = NULL;
			}
			sub[ 0 ] = NULL;
			level = 0;
			piped = 2;
			p++; break;
		case '(':
			level++;
			if ( p[1]==':' ) {
				e = bm_literal( p, db );
				sub[ ndx ] = newItem( e );
				p = p_prune( PRUNE_TERM, p );
				break;
			}
			add_item( &stack.flags, ndx|piped );
			if ( ndx ) {
				addItem( &stack.results, sub[ 0 ] );
				ndx = 0; sub[ 0 ]=NULL;
			}
			piped = 0;
			p++; break;
		case ',':
			if ( !level && !stack.level ) { done=1; break; }
			else if ( level ) ndx = 1;
			else {
				instances = popListItem( &stack.results );
				addItem( &stack.results, catListItem( instances, sub[ 0 ] ));
				sub[ 0 ] = NULL;
			}
			p++; break;
		case ')':
			if ( piped && !level ) {
				bm_pop_mark( ctx, '!' );
				freeListItem( &sub[ 0 ] );
				level = pop_item( &stack.level );
				ndx = pop_item( &stack.flags );
				if ( ndx ) sub[ 1 ] = popListItem( &stack.results );
				sub[ 0 ] = popListItem( &stack.results );
				piped = 0;
			}
			if ( !level && !(stack.level)) { done=1; break; }
			level--;
			switch ( ndx ) {
			case 0: instances = sub[ 0 ]; break;
			case 1: instances = bm_couple( sub, db );
				freeListItem( &sub[ 0 ] );
				freeListItem( &sub[ 1 ] );
			}
			int flags = pop_item( &stack.flags );
			ndx = flags & 1;
			piped = flags & 2;
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '.':
			if ( empty ) { done=-1; break; }
			sub[ ndx ] = newItem( NULL );
			p++; break;
		default:
			e = bm_register( ctx, p );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
			if ( *p=='~' ) {
				db_signal( e, BMContextDB(ctx) );
				p++;
			}
		}
	}
	if ( done == -1 ) {
		freeListItem( &stack.flags );
		freeListItem( &stack.level );
		freeListItem( &sub[ 1 ] );
		listItem *results = NULL;
		while (( results = popListItem(&stack.results) ))
			freeListItem( &results );
	}
#ifdef DEBUG
	if (( sub[ 0 ] )) {
		fprintf( stderr, "bm_instantiate: } first=" );
		if ( done == -1 ) fprintf( stderr, "***INCOMPLETE***" );
		else db_output( stderr, "", sub[0]->ptr, db );
		fprintf( stderr, "\n" );
	}
	else fprintf( stderr, "bm_instantiate: } no result\n" );
#endif
	freeListItem( &sub[ 0 ] );
}

static listItem *
bm_couple( listItem *sub[2], CNDB *db )
/*
	Instantiates and/or returns all ( sub[0], sub[1] )
*/
{
	listItem *results = NULL, *s = NULL;
	if ( sub[0]->ptr == NULL ) {
		if ( sub[1]->ptr == NULL ) {
			listItem *t = NULL;
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
				addIfNotThere( &results, db_instantiate(e,f,db) );
		}
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, db_instantiate(e,j->ptr,db) );
		}
	}
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, db_instantiate(i->ptr,f,db) );
	}
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, db_instantiate(i->ptr,j->ptr,db) );
	}
	return results;
}

static CNInstance *
bm_literal( char *expression, CNDB *db )
/*
	Assumption: expression = (:ab...yz) resp. (:ab...z:)
	converts into (a,(b,...(y,z))) resp. (a,(b,...(z,'\0')))
*/
{
	listItem *stack = NULL;
	CNInstance *e=NULL, *mod=NULL, *backslash=NULL, *f;
	char *p = expression + 2;
	char_s q;
	for ( ; ; ) {
		switch ( *p ) {
		case ')':
			e = popListItem(&stack);
			while (( f = popListItem(&stack) ))
				e = db_instantiate( f, e, db );
			goto RETURN;
		case '%':
			if ( mod == NULL ) mod = db_register( "%", db, 0 );
			switch ( p[1] ) {
			case '%':
				e = db_instantiate( mod, mod, db );
				p+=2; break;
			default:
				p++;
				e = db_register( p, db, 0 );
				e = db_instantiate( mod, e, db );
				if ( !is_separator(*p) ) {
					p = p_prune( PRUNE_IDENTIFIER, p+1 );
					if ( *p=='\'' ) p++;
				}
			}
			addItem( &stack, e );
			break;
		case ':':
			if ( p[1]==')' )
				e = db_register( "\0", db, 0 );
			else {
				q.value = p[ 0 ];
				e = db_register( q.s, db, 0 );
			}
			addItem( &stack, e );
			p++; break;
		case '\\':
			switch (( q.value = p[1] )) {
			case '0':
			case ' ':
			case 'w':
				if ( backslash == NULL ) {
					backslash = db_register( "\\", db, 0 );
				}
				e = db_register( q.s, db, 0 );
				e = db_instantiate( backslash, e, db );
				p+=2; break;
			case ')':
				e = db_register( q.s, db, 0 );
				p+=2; break;
			default:
				p += charscan( p, &q );
				e = db_register( q.s, db, 0 );
			}
			addItem( &stack, e );
			break;
		default:
			q.value = p[0];
			e = db_register( q.s, db, 0 );
			addItem( &stack, e );
			p++;
		}
	}
RETURN:
	return e;
}

//===========================================================================
//	bm_void
//===========================================================================
int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is instantiable, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	CNDB *db = BMContextDB( ctx );
	int done=0, level=0, empty=db_is_empty(0,db);
	char *p = expression;
	while ( *p && !done ) {
		if ( p_filtered( p ) ) {
			if ( !empty && bm_feel( p, ctx, BM_CONDITION ) ) {
				p = p_prune( PRUNE_TERM, p );
				continue;
			}
			else return 1;
		}
		switch ( *p ) {
		case '{':
		case '}':
		case '|':
			p++; break;
		case '%':
			if ( p[1]=='!' ) {
				p+=2; break;
			}
			if ( p[1]=='?' ) {
				if ( !bm_context_lookup( ctx, "?" ) )
					return 1;
				p+=2; break;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) )
				{ p++; break; }
			// no break
		case '~':;
			int target = xp_target( p, EMARK );
			if ( target&EMARK && target!=EMARK ) {
				fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at '%s' - "
					"dubious combination of query terms with %%!\n", p );
			}
			if ( !empty && bm_feel( p, ctx, BM_CONDITION ) ) {
				p = p_prune( PRUNE_TERM, p );
				break;
			}
			else return 1;
		case '(':
			level++;
			p++; break;
		case ',':
			if ( !level ) { done=1; break; }
			p++; break;
		case ')':
			if ( !level ) { done=1; break; }
			level--;
			p++; break;
		case '?':
		case '.':
			if ( empty ) return 1;
			p++; break;
		default:
			p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	return 0;
}

