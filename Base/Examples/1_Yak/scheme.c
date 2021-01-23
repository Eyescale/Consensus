#include <stdio.h>
#include <stdlib.h>

#include "string_util.h"
#include "macros.h"
#include "pair.h"
#include "scheme.h"
#include "ansi.h"

// #define SCH_DEBUG

/*===========================================================================

	Scheme public parsing interface -- implementation

===========================================================================*/
typedef enum {
	ErrReadSchemeSyntaxError,
	ErrReadSchemeEOF,
	WarnReadSchemeExtra,
	WarnReadSchemeHyphen,
	ErrReadSchemeNoErr	// always last
} readSchemeErrNum;

static readSchemeErrNum readSchemeFrame( Scheme *, int );

/*---------------------------------------------------------------------------
	readScheme	- interface to File
---------------------------------------------------------------------------*/
Scheme *
readScheme( char *path )
{
	Scheme *scheme = newScheme();
	if ( scheme == NULL ) return NULL;

	FILE *file = fopen( path, "r" );
	if ( file == NULL ) {
		freeScheme( scheme );
		return NULL;
	}
	int event = '\n', abort = 0, line = 0, column;
	do {
		if ( event == '\n' ) {
			line++; column = 1;
		}
		else column++;
		event = fgetc( file );
		switch ( readSchemeFrame( scheme, event ) ) {
			case ErrReadSchemeSyntaxError:
				fprintf( stderr, "Error: readScheme: l%dc%d: syntax error\n", line, column );
				abort = 1;
				break;
			case ErrReadSchemeEOF:
				fprintf( stderr, "Error: readScheme: l%dc%d: premature EOF\n", line, column );
				abort = 1;
				break;
			case WarnReadSchemeExtra:
				fprintf( stderr, "Warning: readScheme: l%dc%c: extraneous information removed\n", line, column );
				break;
			case WarnReadSchemeHyphen:
				fprintf( stderr, "Warning: readScheme: l%dc%c: multiple '%%-'\n", line, column );
				break;
			case ErrReadSchemeNoErr:
				break;
			}
	} while (( event != EOF ) && !abort );
	fclose( file );

	if ( abort ) {
		if ( event != EOF ) // cleanup
			readSchemeFrame( scheme, EOF );
		freeScheme( scheme );
		scheme = NULL;
	}
	else if (( scheme )) {
		reorderListItem( &scheme->rules );
		for ( listItem *i=scheme->rules; i!=NULL; i=i->next ) {
			Rule *r = i->ptr;
			reorderListItem( &r->schemata );
		}
	}
	return scheme;
}

/*---------------------------------------------------------------------------
	readSchemeFrame
---------------------------------------------------------------------------*/
enum {
	RULE = 0,
	SCHEMA,
	TOKEN
};
static void	r_reset( CNString **, int *, listItem ** );
static void 	r_append( CNString **, int );
static Rule *	r_set( CNString **, Scheme * );
static void	s_append( CNString **, int, int * );
static void	t_append( CNString **, int );
static void 	s_finish( CNString **, Rule *, int *, listItem ** );
static void	t_finish( CNString **, Scheme *, listItem ** );
static void	t_wrap( listItem **, int );

static readSchemeErrNum
readSchemeFrame( Scheme *scheme, int event )
{
	static Rule *rule;
	static CNString *string[ 3 ] = { NULL, NULL, NULL };
	static int position = 0;
	static char *state = "base";
	static listItem *list[ 2 ] = { NULL, NULL };
	static int regex_position = 0;

	if ( string[ RULE ] == NULL ) {
		string[ RULE ] = newString();
		string[ SCHEMA ] = newString();
		string[ TOKEN ] = newString();
	}
	int errnum = ErrReadSchemeNoErr;

	BEGIN
#ifdef SCH_DEBUG
	fprintf( stderr, "readSchemeFrame: state=\"%s\" event='%c'\n", state, event );
#endif
	bgn_
	on_( EOF ) bgn_
		in_( "base" )	do_( same )
		in_( "\\n" )	do_( "base" )
		in_( ": \\n" )	do_( "base" )
					s_finish( string, rule, &position, list );
		in_( ": \\n_" )	do_( "base" )
					s_finish( string, rule, &position, list );
		in_other	do_( "base" )
					errnum = ErrReadSchemeEOF;
		end
		r_reset( string, &position, list );
	in_( "error" )
		r_reset( string, &position, list );
		do_( "base" )
			errnum = ErrReadSchemeSyntaxError;
	in_( "base" ) bgn_
		on_( '#' )	do_( "#" )
		on_( '\n' )	do_( "\\n" )
		on_space	do_( "space" )
		on_other	do_( "rule" )	REENTER
		end
		in_( "#" ) bgn_
			on_( '\n' )	do_( "\\n" )
			on_other	do_( same )
			end
		in_( "\\n" ) bgn_
			on_( '#' )	do_( "#" )
			on_other	do_( "base" )	REENTER
			end
		in_( "space" ) bgn_
			on_space	do_( same )
			on_other	do_( "base" )	REENTER
			end
	in_( "rule" ) bgn_
		on_space	do_( "rule_" )
		on_( ':' )	do_( "schema" )
					rule = r_set( string, scheme );
		on_separator	do_( "error" )	REENTER
		on_other	do_( same )
					r_append( string, event );
		end
		in_( "rule_" ) bgn_
			on_space	do_( same )
			on_( ':' )	do_( "schema" )
						rule = r_set( string, scheme );
			on_other	do_( "error" )	REENTER
			end
	in_( "schema" ) bgn_
		on_space	do_( ": space" )
		on_( '%' )	do_( ": %" )
		on_( '\n' )	do_( ": \\n" )
		on_( '\\' )	do_( ": \\" )
		on_other	do_( ": _" )
					s_append( string, event, &position );
		end
		in_( ": space" ) bgn_
			on_space	do_( same )
			on_( '%' )	do_( ": space%" )
			on_( '\n' )	do_( ": \\n" )
						regex_position = 0;
						// space characters are ignored at the end of a schema
			on_( '\\' )	do_( ": \\" )
						regex_position = 0;
						if ( isRuleBase( rule ) || ( string[ SCHEMA ]->data ))
							s_append( string, ' ', &position );
			on_other	do_( ": _" )
						regex_position = 0;
						if ( isRuleBase( rule ) || ( string[ SCHEMA ]->data ))
							s_append( string, ' ', &position );
						s_append( string, event, &position );
			end
		in_( ": space%" ) bgn_
			on_( '{' )	do_( "tokens" )
						// space characters are ignored before the Token sign
						// EXCEPT following regex rule
						if ( regex_position ) {
							regex_position = 0;
							s_append( string, ' ', &position );
						}
			on_other	do_( ": %" )	REENTER
						if ( isRuleBase( rule ) || ( string[ SCHEMA ]->data ))
							s_append( string, ' ', &position );
			end
		in_( ": \\n" ) bgn_
			on_( '#' )	do_( ": #" )
			on_other	do_( ": \\n_" )	REENTER
			end
			in_( ": #" ) bgn_
				on_( '\n' )	do_( ": \\n" )
				on_other	do_( same )
				end
			in_( ": \\n_" ) bgn_
				on_space	do_( same )
				on_( '|' )	do_( "schema" )
							s_finish( string, rule, &position, list );
				on_other	do_( "base" )	REENTER
							s_finish( string, rule, &position, list );
				end
		in_( ": \\" ) bgn_
			on_( '\n' )	do_( "schema" )
			on_other	do_( "schema" )
						s_append( string, '\\', &position );
						s_append( string, event, &position );
			end
		in_( ": _" ) bgn_
			on_separator	do_( "schema" )	REENTER
			on_other	do_( same )
						s_append( string, event, &position );
			end
		in_( ": %" ) bgn_
			on_( '{' )	do_( "tokens" )
			on_( '-' )	do_( ": %-" )
			on_( '/' )	do_( ": regex" )
						s_append( string, '%', &position );
						regex_position = position;
						s_append( string, '/', &position );
			on_( '\n' )	do_( ": \\n" )
						errnum = WarnReadSchemeExtra;
			on_space	do_( ": % " )
						s_append( string, '%', &position );
						s_append( string, ' ', &position );
			on_separator	do_( "schema" ) 
						s_append( string, '%', &position );
						s_append( string, event, &position );
			on_other	do_( ": %_" )
						s_append( string, '%', &position );
						s_append( string, event, &position );
			end
			in_( ": % " ) bgn_
				on_space	do_( same )
				on_( '%' )	do_( ": %" )
				on_( '\n' )	do_( ": \\n" )
				on_( '\\' )	do_( ": \\" )
				on_other	do_( ": _" )
				end
			in_( ": %_" ) bgn_
				on_separator	do_( "schema" )	REENTER
				on_other	do_( same )
							s_append( string, event, &position );
				end
			in_( ": %-" ) bgn_
				on_space	do_( same )
				on_( '%' )	do_( ": %-%" )
				on_( '\n' )	do_( ": \\n" )
							if ( isRuleBase( rule ) && !( string[ SCHEMA ]->data )) {
								s_append( string, '%', &position );
								s_append( string, '-', &position );
							}
							else errnum = WarnReadSchemeExtra;
				on_( '\\' )	do_( ": %-\\" )
				on_other	do_( ": _" )
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, event, &position );
				end
			in_( ": %-\\" ) bgn_
				on_( '\n' )	do_( ": %-" )
				on_other	do_( "schema" )
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, '\\', &position );
							s_append( string, event, &position );
				end
			in_( ": %-%" ) bgn_
				on_( '{' )	do_( "tokens" )
							errnum = WarnReadSchemeExtra;
				on_( '-' )	do_( ": %-" )
							errnum = WarnReadSchemeHyphen;
				on_( '/' )	do_( ": regex" )
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, '%', &position );
							regex_position = position;
							s_append( string, '/', &position );
				on_( '\n' ) 	do_( ": \\n" )
							errnum = WarnReadSchemeExtra;
				on_space	do_( ": % " )
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, '%', &position );
							s_append( string, ' ', &position );
				on_separator	do_( "schema" ) 
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, '%', &position );
							s_append( string, event, &position );
				on_other	do_( ": %_" )
							s_append( string, '%', &position );
							s_append( string, '-', &position );
							s_append( string, '%', &position );
							s_append( string, event, &position );
				end
		in_( ": regex" ) bgn_
			on_( '\n' )	do_( "error" )	REENTER
						regex_position = 0;
			on_( '\\' )	do_( ": %/\\" )
			on_( '/' )	do_( ": %//" )
						s_append( string, event, &position );
			on_other	do_( same )
						s_append( string, event, &position );
			end
			in_( ": %/\\" ) bgn_
				on_( '\n' )	do_( ": %/\\\\n" )
				on_other	do_( ": regex" )
							s_append( string, '\\', &position );
							s_append( string, event, &position );
				end
				in_( ": %/\\\\n" ) bgn_
					on_space	do_( same )
					on_other	do_( ": regex" )	REENTER
					end
			in_( ": %//" ) bgn_
				on_space	do_( ": space" )
				on_( '%' )	do_( ": %//%" )
				on_other	do_( "schema" )	REENTER
							regex_position = 0;
				end
				in_( ": %//%" ) bgn_
					on_( '{' )	do_( "tokens" )
					on_other	do_( ": %" )	REENTER
								regex_position = 0;
					end
	in_( "tokens" ) bgn_
		on_( '\n' )	do_( "error" )	REENTER
		on_space	do_( "token " )
					t_finish( string, scheme, list );
		on_( '\\' )	do_( "token\\" )
					t_finish( string, scheme, list );
		on_( '}' )	do_( "%{_}" )
					t_finish( string, scheme, list );
					if ( regex_position )
						t_wrap( list, regex_position );
					else
						t_wrap( list, position );
		on_other	do_( same )
					t_append( string, event );
		end
		in_( "token " ) bgn_
			on_space	do_( same )
			on_other	do_( "tokens" )	REENTER
			end
		in_( "token\\" ) bgn_
			on_( '\n' )	do_( "tokens" )
			on_other	do_( "tokens" )
						t_append( string, '\\' );
						t_append( string, event );
			end
		in_( "%{_}" ) bgn_
			on_space	do_( ": space" )
			on_( '%' )	do_( "%{_}%" )
			on_other	do_( "schema" )	REENTER
						regex_position = 0;
			end
			in_( "%{_}%" ) bgn_
				on_( '{' )	do_( "tokens" )
				on_( '-' )	do_( "%{_}" )
				on_other	do_( ": %" )	REENTER
							regex_position = 0;
				end
	END
	return errnum;
}

static void
r_reset( CNString **s, int *position, listItem **list )
{
	*position = 0;
	StringReset( s[ RULE ], CNStringAll );
	StringReset( s[ SCHEMA ], CNStringAll );
	StringReset( s[ TOKEN ], CNStringAll );
	for ( listItem *i=list[ 1 ]; i!=NULL; i=i->next ) {
		Token *token = i->ptr;
		freeListItem( &token->values );
		freePair( i->ptr );
	}
	freeListItem( &list[ 1 ] );
	freeListItem( &list[ 0 ] );
}
static void
r_append( CNString **s, int event )
{
	StringAppend( s[ RULE ], event );
}
static Rule *
r_set( CNString **s, Scheme *scheme )
{
	Rule *rule;
	char *identifier = StringFinish( s[ RULE ], 0 );
	rule = findRule( scheme, identifier );
	if (( rule )) {
		fprintf( stderr, "Warning: rule '%s' double definition\n", identifier );
		StringReset( s[ RULE ], CNStringAll );
		return rule;
	}
	StringReset( s[ RULE ], CNStringMode );
	rule = newRule( identifier );
	addRule( scheme, rule );
	return rule;
}
static void
s_append( CNString **s, int event, int *position )
{
	StringAppend( s[ SCHEMA ], event );
	*position += 1;
}
static void
s_finish( CNString **s, Rule *rule, int *position, listItem **list )
{
	Schema *schema;
	char *string = StringFinish( s[ SCHEMA ], 0 );
	schema = findSchema( rule, string );
	if (( schema )) {
		fprintf( stderr, "Warning: rule '%s' double schema '%s'\n",
			rule->identifier, string );
		r_reset( s, position, list );
	}
	else if (( list[ 1 ] )) {
		StringReset( s[ SCHEMA ], CNStringMode );
		reorderListItem( &list[ 1 ] );
		for ( listItem *i=list[ 1 ]; i!=NULL; i=i->next ) {
			Token *token = i->ptr;
			int p = (int) token->position;
			token->position = ( p ? &string[ p ] : NULL );
		}
		schema = newSchema( string, list[ 1 ] );
		addSchema( rule, schema );
		list[ 1 ] = NULL;
		*position = 0;
	}
	else {
		StringReset( s[ SCHEMA ], CNStringMode );
		schema = newSchema( string, NULL );
		addSchema( rule, schema );
		*position = 0;
	}
}
static void
t_append( CNString **s, int event )
{
	StringAppend( s[ TOKEN ], event );
}
static void
t_finish( CNString **s, Scheme *scheme, listItem **list )
{
	char *identifier = StringFinish( s[ TOKEN ], 0 );
	if ( identifier == NULL ) return;

	listItem *token = findToken( scheme, identifier );
	if (( token ))
		StringReset( s[ TOKEN ], CNStringAll );
	else {
		StringReset( s[ TOKEN ], CNStringMode );
		token = addToken( scheme, identifier );
	}
	addItem( &list[ 0 ], token->ptr );
}
static void
t_wrap( listItem **list, int position )
{
	if ( list[ 0 ] == NULL ) return;

	reorderListItem( &list[ 0 ] );
	union { void *ptr; int value; } icast;
	icast.value = position;
	Pair *pair = newPair( icast.ptr, list[ 0 ] );
	addItem( &list[ 1 ], pair );
	list[ 0 ] = NULL;
}

/*---------------------------------------------------------------------------
	outputScheme
---------------------------------------------------------------------------*/
static void outputToken( listItem *values );
static void regex_set( void *, char * );

void
outputScheme( Scheme *scheme, int tok )
{
	if ( scheme == NULL ) return;

	printf( "~~~~~~\n" );
	if ( !tok ) {
		for ( listItem *i=scheme->rules; i!=NULL; i=i->next ) {
			Rule *rule = i->ptr;
			int base = isRuleBase( rule );
			printf( " %s\t:", rule->identifier );
			if ( !base ) printf( " " );
			for ( listItem *j=rule->schemata; j!=NULL; j=j->next ) {
				Schema *schema = j->ptr;
				if ( base && ( schema->string[ 0 ] != ' ' ))
					printf( " " );
				printf( "%s\n", schema->string );
				if ( j->next ) {
					printf( "\t|" );
					if ( !base ) printf( " " );
				}
			}
			if ( i->next ) printf( "\n" );
		}
	}
	else for ( listItem *i=scheme->rules; i!=NULL; i=i->next ) {
		Rule *rule = i->ptr;
		char *r_id = rule->identifier;
		printf( " %s\t:", r_id );
		for ( listItem *j=rule->schemata; j!=NULL; j=j->next ) {
			Schema *schema = j->ptr;
			/* output schema leading tokens
			*/
			listItem *t = schema->data;
			for ( int sp=0; (( t )); t=t->next ) {
				Token *token = t->ptr;
				if (( token->position ))
					break;
				if (!sp) { sp = 1; printf( " " ); }
				outputToken( token->values );
			}
			/* output schema leading space if not already there
			   and not in base rule
			*/
			char *str = schema->string;
			switch ( *str ) {
				case '\0':
				case ' ':
					break;
				default:
					if ( *r_id ) printf( " " );
			}
			struct { int state; char *regexpos; } output = { 0, NULL };
			for ( ; *str; str++ ) {
				printf( "%c", *str );
				switch ( output.state ) {
				case 0:
					if ( *str == '\\' )
						output.state = 1;
					else if ( *str == '%' )
						output.state = 2;
					break;
				case 1:
					output.state = 0;
					break;
				case 2:
					if ( *str == '/' ) {
						output.state = 3;
						output.regexpos = str;
					}
					else output.state = 0;
					break;
				case 3:
					switch ( *str ) {
					case '\\':
						output.state = 4;
						break;
					case '/':
						output.state = 5;
						break;
					}
					break;
				case 4:
					output.state = 3;	
					break;
				}
				/* output tokens at position
				*/
				char *tpos = ( output.state == 5 ) ? output.regexpos : &str[ 1 ];
				int sp = ( output.state != 5 ) && ( *str != ' ' );
				switch ( output.state ) {
				case 0:
				case 5:
					for ( ; (( t )); t=t->next ) {
						Token *token = t->ptr;
						if ( token->position != tpos )
							break;
						if (sp) { sp = 0; printf( " " ); }
						outputToken( token->values );
					}
				}
				if ( output.state == 5 ) output.state = 0;
			}
			if ( j->next ) printf( "\n\t|" );
		}
		printf( "\n" );
		if ( i->next ) printf( "\n" );
	}
	printf( "~~~~~~\n" );
}

#define TOKEN_COLOR	BOLD_COLOR_YELLOW
#define BOX_COLOR	BOLD_COLOR_YELLOW
static void
outputToken( listItem *values )
{
	printf ( BOX_COLOR "%%{ " ANSI_COLOR_RESET );
	for ( listItem *k=values; k!=NULL; k=k->next ) {
		printf( TOKEN_COLOR "%s" ANSI_COLOR_RESET, k->ptr );
		if ( k->next ) printf( " " );
	}
	printf( BOX_COLOR " }" ANSI_COLOR_RESET );
}

/*===========================================================================

	Scheme public model-building interface

===========================================================================*/
typedef struct {
	Rule *base;
	listItem *tokens;
} internals;

Scheme *
newScheme( void )
{
	internals *i = (internals *) newPair( NULL, NULL );
	return (Scheme *) newPair( NULL, i );
}
void
freeScheme( Scheme *s )
{
	if ( s == NULL ) return;
	for ( listItem *i=s->rules; i!=NULL; i=i->next )
		freeRule( i->ptr );
	freeListItem( &s->rules );

	internals *intern = (internals *) s->internals;
	for ( listItem *i=intern->tokens; i!=NULL; i=i->next )
		free( i->ptr );
	freeListItem( &intern->tokens );

	freePair((Pair *) s->internals );
	freePair((Pair *) s );
}
Rule *
SchemeBase( Scheme *s )
{
	return ((internals *) s->internals)->base;
}
Rule *
findRule( Scheme *s, char *identifier )
{
	if ( identifier == NULL ) identifier = "";
	for ( listItem *i=s->rules; i!=NULL; i=i->next ) {
		Rule *rule = (Rule *) i->ptr;
		if ( !strcmp( rule->identifier, identifier ) )
			return rule;
	}
	return NULL;
}
Rule *
fetchRule( Scheme *s, char *position )
{
	Rule *rule;
	char *identifier;
	strscanid( position, &identifier );
	if (( identifier )) {
		rule = findRule( s, identifier );
		free( identifier );
		return rule;
	}
	return NULL;
}
void
addRule( Scheme *s, Rule *r )
{
	addItem( &s->rules, r );
	if ( !strcmp( r->identifier, "" ) )
		((internals *) s->internals)->base = r;
}
void
removeRule( Scheme *s, Rule *r )
{
	removeItem( &s->rules, r );
}
listItem *
findToken( Scheme *s, char *t )
{
	listItem *tokens = ((internals *) s->internals)->tokens;
	for ( listItem *i=tokens; i!=NULL; i=i->next ) {
		char *token = i->ptr;
		if ( !strcmp( token, t ) )
			return i;
	}
	return NULL;
}
listItem *
addToken( Scheme *s, char *t )
{
	return addItem( &((internals *) s->internals)->tokens, t );
}
void
removeToken( Scheme *s, listItem *t )
{
	clipItem( &((internals *) s->internals)->tokens, t );
}

/*---------------------------------------------------------------------------
	Rule interface
---------------------------------------------------------------------------*/
Rule *
newRule( char *identifier )
{
	return ( identifier == NULL ) ?
		(Rule *) newPair( "", NULL ) :
		(Rule *) newPair( identifier, NULL );
}
void
freeRule( Rule *r )
{
	if ( strcmp( r->identifier, "" ) )
		free( r->identifier );

	for ( listItem *i=r->schemata; i!=NULL; i=i->next )
		freeSchema( i->ptr );

	freeListItem( &r->schemata );
	freePair((Pair *) r );
}
Schema *
findSchema( Rule *r, char *string )
{
	if (( string )) {
		for ( listItem *i=r->schemata; i!=NULL; i=i->next ) {
			Schema *schema = i->ptr;
			if ( !strcmp( schema->string, string ) )
				return schema;
		}
	}
	else {
		for ( listItem *i=r->schemata; i!=NULL; i=i->next ) {
			Schema *schema = i->ptr;
			if ( !strcmp( schema->string, "" ) )
				return schema;
		}
	}
	return NULL;
}
void
addSchema( Rule *r, Schema *s )
{
	addItem( &r->schemata, s );
}
void
removeSchema( Rule *r, Schema *s )
{
	removeItem( &r->schemata, s );
}

/*---------------------------------------------------------------------------
	Schema interface
---------------------------------------------------------------------------*/
Schema *
newSchema( char *string, listItem *data )
{
	return ( string == NULL ) ?
		(Schema *) newPair( "", data ) :
		(Schema *) newPair( string, data );
}
void
freeSchema( Schema *s )
{
	if ( strcmp( s->string, "" ) )
		free( s->string );
	for ( listItem *i=s->data; i!=NULL; i=i->next ) {
		Token *token = i->ptr;
		freeListItem( &token->values );
		freePair((Pair *) token );
	}
	freeListItem((listItem **) &s->data );
	freePair((Pair *) s );
}

