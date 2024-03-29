#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "database.h"
#include "expression.h"
#include "traversal.h"

// #define DEBUG

//===========================================================================
//	bm_couple
//===========================================================================
typedef CNInstance *BMCoupleFunc( CNInstance *, CNInstance *, CNDB * );

static listItem *
bm_couple( BMCoupleFunc couple, listItem *sub[2], CNDB *db )
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
				addIfNotThere( &results, couple(e,f,db) );
		}
		else {
			for ( CNInstance *e=db_first(db,&s); e!=NULL; e=db_next(db,e,&s) )
			for ( listItem *j=sub[1]; j!=NULL; j=j->next )
				addIfNotThere( &results, couple(e,j->ptr,db) );
		}
	}
	else if ( sub[1]->ptr == NULL ) {
		listItem *t = NULL;
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( CNInstance *f=db_first(db,&t); f!=NULL; f=db_next(db,f,&t) )
			addIfNotThere( &results, couple(i->ptr,f,db) );
	}
	else {
		for ( listItem *i=sub[0]; i!=NULL; i=i->next )
		for ( listItem *j=sub[1]; j!=NULL; j=j->next )
			addIfNotThere( &results, couple(i->ptr,j->ptr,db) );
	}
	return results;
}

//===========================================================================
//	bm_substantiate
//===========================================================================
static CNInstance *bm_expand( char *, CNDB * );

char *
bm_substantiate( char *expression, CNDB *db )
/*
	Substantiates expression by instantiating all relationships involved.
	Note that none of the created relationship instance will be manifested.
*/
{
	struct { listItem *results, *ndx, *level; } stack;
	memset( &stack, 0, sizeof(stack) );
	listItem *sub[ 2 ] = { NULL, NULL }, *instances;
	CNInstance *e = NULL;
	int level=0, ndx=0, done=0;
	char *p = expression;
	char_s q;
	while ( *p && !done ) {
		switch ( *p ) {
		case '{':
			add_item( &stack.level, level );
			level = 0;
			add_item( &stack.ndx, ndx );
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
			ndx = pop_item( &stack.ndx );
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '(':
			level++;
			if ( p[1]==':' ) {
				e = bm_expand( p, db );
				sub[ ndx ] = newItem( e );
				p = p_prune( PRUNE_TERM, p );
				break;
			}
			add_item( &stack.ndx, ndx );
			if ( ndx ) {
				addItem( &stack.results, sub[ 0 ] );
				ndx=0; sub[ 0 ]=NULL;
			}
			p++; break;
		case ',':
			if ( !level && !(stack.level)) { done=1; break; }
			else if ( level ) ndx = 1;
			else {
				instances = popListItem( &stack.results );
				addItem( &stack.results, catListItem( instances, sub[ 0 ] ));
				sub[ 0 ] = NULL;
			}
			p++; break;
		case ')':
			if ( !level && !(stack.level)) { done=1; break; }
			level--;
			switch ( ndx ) {
			case 0: instances = sub[ 0 ]; break;
			case 1: instances = bm_couple( db_couple, sub, db );
				freeListItem( &sub[ 0 ] );
				freeListItem( &sub[ 1 ] );
			}
			ndx = pop_item( &stack.ndx );
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '\'':
			if ( charscan( p+1, &q ) )
				e = db_register( q.s, db, 0 );
			else // error
				e = db_register( "", db, 0 );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
			break;
		default:
			e = db_register( p, db, 0 );
			sub[ ndx ] = newItem( e );
			if ( !is_separator( *p++ ) )
				p = p_prune( PRUNE_IDENTIFIER, p );
		}
	}
	freeListItem( &sub[ 0 ] );
	return p;
}
static CNInstance *
bm_expand( char *expression, CNDB *db )
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
				e = db_couple( f, e, db );
			goto RETURN;
		case '%':
			p++;
			if ( mod == NULL ) mod = db_register( "%", db, 0 );
			switch ( *p ) {
			case '%':
				e = db_couple( mod, mod, db );
				p++; break;
			default:
				e = db_register( p, db, 0 );
				e = db_couple( mod, e, db );
				if ( !is_separator(*p++) )
					p = p_prune( PRUNE_IDENTIFIER, p );
			}
			addItem( &stack, e );
			if ( *p == '\'' ) p++;
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
				e = db_couple( backslash, e, db );
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
//	bm_instantiate
//===========================================================================
static int bm_void( char *, BMContext * );

void
bm_instantiate( char *expression, BMContext *ctx )
/*
	same as bm_substantiate, except manifests new relationship instances.
*/
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
	CNDB *	db = ctx->db;
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
				CNInstance *e = lookup_mark_register( ctx, '?' );
				if (( e )) {
					sub[ ndx ] = newItem( e );
				}
				else { done=-1; break; }
				p+=2; break;
			}
			else if ( p[1]=='!' ) {
				listItem *i = lookup_mark_register( ctx, '!' );
				if ( !i ) { done=-1; break; }
				for ( ; i!=NULL; i=i->next )
					addItem( &sub[ ndx ], i->ptr );
				p+=2; break;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) ) {
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
			case 1: instances = bm_couple( db_instantiate, sub, db );
				freeListItem( &sub[ 0 ] );
				freeListItem( &sub[ 1 ] );
			}
			int flags = pop_item( &stack.flags );
			ndx = flags & 1;
			piped = flags & 2;
			sub[ ndx ] = instances;
			if ( ndx ) sub[ 0 ] = popListItem( &stack.results );
			p++; break;
		case '?':
		case '.':
			if ( !empty ) {
				sub[ ndx ] = newItem( NULL );
				p++; break;
			}
			else { done=-1; break; }
		default:
			e = bm_register( ctx, p );
			sub[ ndx ] = newItem( e );
			p = p_prune( PRUNE_IDENTIFIER, p );
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

static int
bm_void( char *expression, BMContext *ctx )
/*
	tests if expression is instantiable, ie. that
	. all inner queries have results
	. all the associations it contains can be made 
	  e.g. in case the CNDB is empty, '.' fails
	returns 0 if it is, 1 otherwise
*/
{
	// fprintf( stderr, "bm_void: %s\n", expression );
	CNDB *db = ctx->db;
	int done=0, level=0, empty=db_is_empty(0,db);
	char *p = expression;
	while ( *p && !done ) {
		if ( p_filtered( p ) ) {
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_TERM, p );
			continue;
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
			else if ( p[1]=='?' ) {
				if (( lookup_mark_register( ctx, p[1] ) ))
					{ p+=2; break; }
				else return 1;
			}
			// no break
		case '*':
			if ( !p[1] || strmatch( ":,)", p[1] ) )
				{ p++; break; }
			// no break
		case '~':;
			int target = xp_target( p, EMARK );
			if ( target & EMARK ) {
				if (( target - EMARK ))
					fprintf( stderr, ">>>>> B%%:: Warning: bm_void, at %s - "
						"expression query mixing terms with %%!\n", p );
				p = p_prune( PRUNE_TERM, p+1 );
				break;
			}
			if ( empty || !bm_feel( p, ctx, BM_CONDITION ) )
				return 1;
			p = p_prune( PRUNE_TERM, p+1 );
			break;
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

//===========================================================================
//	bm_query
//===========================================================================
static BMTraverseCB query_CB;

listItem *
bm_query( char *expression, BMContext *ctx )
{
	listItem *results = NULL;
	bm_traverse( expression, ctx, query_CB, &results );
	return results;
}
static int
query_CB( CNInstance *e, BMContext *ctx, void *results )
{
	addIfNotThere((listItem **) results, e );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_release
//===========================================================================
static BMTraverseCB release_CB;

void
bm_release( char *expression, BMContext *ctx )
{
	bm_traverse( expression, ctx, release_CB, NULL );
}
static int
release_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	db_deprecate( e, ctx->db );
	return BM_CONTINUE;
}

//===========================================================================
//	bm_inputf
//===========================================================================
static int bm_input( char *format, char *expression, BMContext * );

#define DEFAULT_FORMAT "_"

int
bm_inputf( char *format, listItem *args, BMContext *ctx )
/*
	Assumption: format starts and finishes with \" or \0
*/
{
	int event, delta;
	char_s q;
	if ( *format ) {
		format++; // skip opening double-quote
		while ( *format ) {
			switch (*format) {
			case '\"':
				goto RETURN;
			case '%':
				format++;
				if ( *format == '\0' )
					break;
				else if ( *format == '%' ) {
					event = fgetc( stdin );
					if ( event==EOF || event!='%' )
						goto RETURN;
				}
				else if ((args)) {
					Pair *pair = args->ptr;
					char *expression = pair->name;
					event = bm_input( format, expression, ctx );
					if ( event==EOF ) goto RETURN;
					args = args->next;
				}
				format++;
				break;
			default:
				delta = charscan( format, &q );
				if ( delta ) {
					event = fgetc( stdin );
					if ( event==EOF || event!=q.value )
						goto RETURN;
					format += delta;
				}
				else format++;
			}
		}
	}
	else {
		while (( args )) {
			Pair *pair = args->ptr;
			char *expression = pair->name;
			event = bm_input( DEFAULT_FORMAT, expression, ctx );
			if ( event == EOF ) break;
			args = args->next;
		}
	}
RETURN:
	if ( event == EOF ) {
		while (( args )) {
			Pair *pair = args->ptr;
			char *expression = pair->name;
			asprintf( &expression, "(*,%s)", expression );
			bm_release( expression, ctx );
			free( expression );
			args = args->next;
		}
	}
	else if ( *format && *format!='\"' ) {
		ungetc( event, stdin );
	}
	return 0;
}
static int
bm_input( char *format, char *expression, BMContext *ctx )
{
	char *input;
	int event;
	switch ( *format ) {
	case 'c':
		event = fgetc( stdin );
		if ( event == EOF ) {
			return EOF;
		}
		switch ( event ) {
		case '\0': asprintf( &input, "'\\0'" ); break;
		case '\t': asprintf( &input, "'\\t'" ); break;
		case '\n': asprintf( &input, "'\\n'" ); break;
		case '\'': asprintf( &input, "'\\\''" ); break;
		case '\\': asprintf( &input, "'\\\\'" ); break;
		default:
			if ( is_printable( event ) ) asprintf( &input, "'%c'", event );
			else asprintf( &input, "'\\x%.2X'", event );
		}
		break;
	case '_':
		input = db_input( stdin, 0 );
		if ( input == NULL ) return EOF;
		break;
	default:
		return 0;
	}
	asprintf( &expression, "((*,%s),%s)", expression, input );
	free( input );
	bm_instantiate( expression, ctx );
	free( expression );
	return 0;
}

//===========================================================================
//	bm_outputf
//===========================================================================
static void bm_output( char *format, char *expression, BMContext *);
static BMTraverseCB output_CB;
typedef struct {
	char *format;
	int first;
	CNInstance *last;
} OutputData;

int
bm_outputf( char *format, listItem *args, BMContext *ctx )
/*
	Assumption: format starts and finishes with \" or \0
*/
{
	int delta; char_s q;
	if ( *format ) {
		format++; // skip opening double-quote
		while ( *format ) {
			switch ( *format ) {
			case '\"':
				goto RETURN;
			case '%':
				format++;
				if ( *format == '\0')
					break;
				else if ( *format == '%' )
					putchar( '%' );
				else if ((args)) {
					char *expression = args->ptr;
					bm_output( format, expression, ctx );
					args = args->next;
				}
				format++;
				break;
			default:
				delta = charscan( format, &q );
				if ( delta ) {
					printf( "%c", q.value );
					format += delta;
				}
				else format++;
			}
		}
	}
	else if ((args)) {
		do {
			char *expression = args->ptr;
			bm_output( DEFAULT_FORMAT, expression, ctx );
			args = args->next;
		} while ((args));
	}
	else printf( "\n" );
RETURN:
	return 0;
}
static void
bm_output( char *format, char *expression, BMContext *ctx )
/*
	outputs expression's results
	note that we rely on bm_traverse to eliminate doublons
*/
{
	OutputData data = { format, 1, NULL };
	bm_traverse( expression, ctx, output_CB, &data );
	if ( data.first )
		db_output( stdout, format, data.last, ctx->db );
	else {
		printf( ", " );
		db_output( stdout, DEFAULT_FORMAT, data.last, ctx->db );
		printf( " }" );
	}
}
static int
output_CB( CNInstance *e, BMContext *ctx, void *user_data )
{
	OutputData *data = user_data;
	if (( data->last )) {
		if ( data->first ) {
			if ( *data->format == 's' )
				printf( "\\" );
			printf( "{ " );
			data->first = 0;
		}
		else printf( ", " );
		db_output( stdout, DEFAULT_FORMAT, data->last, ctx->db );
	}
	data->last = e;
	return BM_CONTINUE;
}
