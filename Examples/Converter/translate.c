#include <stdio.h>
#include <stdlib.h>

#include "sequence.h"
#include "parser.h"

/*---------------------------------------------------------------------------
	translate
---------------------------------------------------------------------------*/
static void translate_raw( Parser *, Sequence * );
enum {
	OccurrenceName = 1,
	OccurrenceValue,
	BooleanAnd,
	BooleanOr
};
static union {
	int value;
	void *ptr;
} icast;
static struct {
	int op;
	listItem *stack;
} bool;
static struct {
	int field, not;
	void *name, *value;
} occurrence;

void
translate( Parser *parser, Sequence *sequence )
{
	if (!( parser->options & P_TOK )) {
		translate_raw( parser, sequence );
		return;
	}
	int opened = 0;
	bool.op = 0;
	bool.stack = NULL;
	occurrence.not = 0;
	occurrence.field = 0;
	occurrence.name = NULL;
	occurrence.value = NULL;

	/* Here sequence is a list of tokens, where
	   each token is a list of strings (cf. parser scheme)
	*/
	for ( listItem *i=sequence; i!=NULL; i=i->next ) {
		for ( listItem *j=i->ptr; j!=NULL; j=j->next ) {
			char *token = j->ptr;
			/* occurrence.field is set upon the "name" or "value" tokens.
			   we know then the next token to be the string captured by '%%'
			*/
			if ( occurrence.field == OccurrenceName ) {
				occurrence.name = token;
				occurrence.field = 0;
				continue;
			}
			else if ( occurrence.field == OccurrenceValue ) {
				occurrence.value = token;
				occurrence.field = 0;
				continue;
			}
			else if ( !strcmp( token, "in_" ) ) {
				printf( "if " );
			}
			else if ( !strcmp( token, "not" ) ) {
				occurrence.not = 1;
			}
			else if ( !strcmp( token, "name" ) ) {
				occurrence.field = OccurrenceName;
			}
			else if ( !strcmp( token, "value" ) ) {
				occurrence.field = OccurrenceValue;
			}
			else if ( !strcmp( token, "take" ) ) {
				char *name = occurrence.name;
				char *value = occurrence.value;
				if ( !opened ) printf( "(" );
				if (( value ))
					printf( "CNIni(%s,%d)", name, atoi(value) );
				else if ( occurrence.not )
					printf( "CNIni(%s,0)", name );
				else
					printf( "CNIni(%s,1)", name );
				if ( !opened ) printf( ")" );
				occurrence.not = 0;
				occurrence.name = NULL;
				occurrence.value = NULL;
			}
			else if ( !strcmp( token, "and_" ) || !strcmp( token, "or_" ) ) {
				icast.value = bool.op;
				addItem( &bool.stack, icast.ptr );
				bool.op = !strcmp( token, "and_" ) ? BooleanAnd : BooleanOr;
				if ( occurrence.not ) {
					if ( !opened ) {
						printf( "(" );
						opened = 1;
					}
					printf( "!" );
					occurrence.not = 0;
				}
				printf( "(" );
				if ( !opened ) opened = 2;
			}
			else if ( !strcmp( token, "comma" ) ) {
				switch ( bool.op ) {
				case BooleanAnd:
					printf( " && " );
					break;
				case BooleanOr:
					printf( " || " );
					break;
				}
			}
			else if ( !strcmp( token, "_and" ) || !strcmp( token, "_or" ) ) {
				bool.op = (int) popListItem( &bool.stack );
				printf( ")" );
			}
			else if ( !strcmp( token, "_in" ) ) {
				if ( opened & 1 ) printf( ")" );
				printf( " {" );
			}
			else {
				printf( "%s", token );
				if ( j->next ) printf( " " );
			}
		}
	}
}

static void
translate_raw( Parser *parser, Sequence *sequence )
/*
	Here sequence is a list of pairs, where
	for each pair
		if pair->name is null, then
			pair->value holds an event value (character)
		else (if pair->name is not null) then
			pair->name is a rule identifier (cf. parser scheme)
			pair->value is the sequence captured from that rule
		endif
	end for
*/
{
	int active = 0;
	int opened = 0;
	bool.op = 0;
	bool.stack = NULL;
	occurrence.name = newString();
	occurrence.value = newString();

	CNSequenceBegin( s, stream, sequence )
	CNOnSequencePush( s ) bgn_
		CNInSequenceName( s, "" )
			if ( !active ) {
				active = 1;
				printf( "if " );
			}
		CNInSequenceNextName( s, "state" )
			occurrence.not = 0;
			occurrence.field = 0;
			StringReset( occurrence.name, CNStringAll );
			StringReset( occurrence.value, CNStringAll );
		end
	CNOnSequenceValue( s ) bgn_
		CNInSequenceName( s, "" ) bgn_
			CNInSequenceValue( s, '/' )
				printf( "good bye!" );
			CNInSequenceValue( s, '\n' )
				if ( active ) {
					if ( opened & 1 ) printf( ")" );
					printf( " {" );
				}
			end
		CNInSequenceName( s, "state" ) bgn_
			CNInSequenceValue( s, '~' )
				occurrence.not = 1;
			CNInSequenceValue( s, ':' )
				switch ( occurrence.field ) {
				case 0:
					occurrence.field = OccurrenceName;
					break;
				case OccurrenceName:
					StringFinish( occurrence.name, 0 );
					occurrence.field = OccurrenceValue;
					break;
				}
			end
		CNInSequenceName( s, "%" )
			switch ( occurrence.field ) {
			case OccurrenceName:
				StringAppend( occurrence.name, CNSequenceValue( s ) );
				break;
			case OccurrenceValue:
				StringAppend( occurrence.value, CNSequenceValue( s ) );
				break;
			}
		CNInSequenceName( s, "op" ) bgn_
			CNInSequenceValue( s, '~' )
				if ( !opened ) {
					printf( "(" );
					opened = 1;
				}
				printf( "!" );
			CNInSequenceValue( s, '(' )
				icast.value = bool.op;
				addItem( &bool.stack, icast.ptr );
				bool.op = BooleanAnd;
				printf( "(" );
				if ( !opened ) opened = 2;
			CNInSequenceValue( s, ')' )
				printf( ")" );
				bool.op = (int) popListItem( &bool.stack );
			CNInSequenceValue( s, '{' )
				icast.value = bool.op;
				addItem( &bool.stack, icast.ptr );
				bool.op = BooleanOr;
				printf( "(" );
				if ( !opened ) opened = 2;
			CNInSequenceValue( s, '}' )
				printf( ")" );
				bool.op = (int) popListItem( &bool.stack );
			end
		CNInSequenceName( s, "expr" ) bgn_
			CNInSequenceValue( s, ',' )
				switch ( bool.op ) {
				case BooleanAnd:
					printf( " && " );
					break;
				case BooleanOr:
					printf( " || " );
					break;
				}
			end
		end
	CNOnSequencePop( s ) bgn_
		CNInSequenceName( s, "state" )
			switch ( occurrence.field ) {
			case OccurrenceName:
				StringFinish( occurrence.name, 0 );
				break;
			case OccurrenceValue:
				StringFinish( occurrence.value, 0 );
				break;
			}
			char *name = ((CNString *) occurrence.name)->data;
			char *value = ((CNString *) occurrence.value)->data;
			if ( !opened ) printf( "(" );
			if (( value ))
				printf( "CNIni(%s,%d)", name, atoi(value) );
			else if ( occurrence.not )
				printf( "CNIni(%s,0)", name );
			else
				printf( "CNIni(%s,1)", name );
			if ( !opened ) printf( ")" );
		end
	CNSequenceEnd( s )

	freeString( occurrence.name );
	freeString( occurrence.value );
}
