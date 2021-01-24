/*
	Converts regex into a simple state machine - aka. finite automaton
	The key to this algorithm is to consider a state as a unique combination of
	active positions, all of them reached, and of which each successor
	represents a trigger to the next state, so that eventually we have:

	StateMachine:{[ state:{p}, table:{ transition:[ trigger, state ] } ]}

	Note that, input not being bufferized, "(bc)?" will fail (~) on input 'bd'
*/
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "pair.h"
#include "registry.h"
#include "smgen.h"
#include "string_util.h"

static void
	* DONE = "done",
	* PASS = "pass",
	* FAIL = "~";

#define BEAUTIFY

//---------------------------------------------------------------------------
//	main	- TEST
//---------------------------------------------------------------------------
#ifdef TEST
char *expression[] = { // Attention: backslashes must be re-doubled here
#if 0
//	"a+",
//	"(a|b)+",
//	"(a|bc)*",
//	"[^\\\\']",
//	"(\\\\.|[^\\\\'])",
//	"(\\\\.|[^\\\\'])+",
//	"([+\\-]?\\d+)"
//	"x(ab)*"
//	"A(ab|cd)*B"
	"(ab|cd)*B"
//	"A*"
#else
	"L?'(\\\\.|[^\\\\'])+'",
#if 0
	"0[xX]\\h+[uUlL]*",
	"[+\\-]?\\d+[uUlL]*",
	"[+\\-]?\\d+(\\.\\d*)?([eE][+\\-]?\\d+)?[fFlL]?",
#endif
	"[+\\-]?\\.\\d+([eE][+\\-]?\\d+)?[fFlL]?"
#endif
};
#define NUMS (sizeof(expression)/sizeof(char*))

int
main( int argc, char *argv[] )
{
	printf( "Regular Expression(s)\n" );
	for ( int i=0; i<NUMS; i++ ) {
		printf( "\t%s\n", expression[ i ] );
	}
	smVerify( expression, NUMS );
	printf( "State Machine\n" );
	listItem *sm = smBuild( expression, NUMS );
	smOutput( sm, 1 );
	smFree( &sm );
}
#endif	// TEST

//---------------------------------------------------------------------------
//	smVerify
//---------------------------------------------------------------------------
int
smVerify( char *expr[], int nums )
{
	int level=0, informed=0;
	SMBegin( expr, nums )
	on_( '\0' ) bgn_
		in_( "base" )
			if ( !level ) {
				do_( "" )
			}
		end
	in_( "base" ) bgn_
		on_( '\\' )	do_( "\\" )	informed = 0;
		on_( '(' )	do_( same )	level++;
						informed = 0;
		on_( ')' )
			if ( level && informed ) {
				do_( same )	level--;
						informed = 1;
			}
		on_( '|' )
			if ( level && informed ) {
				do_( same )	informed = 0;
			}
		ons( "?*+" )
			if ( informed == 1 ) {
				do_( same )	informed = 2;
			}
		on_( '[' )	do_( "[" )	informed = 0;
		on_other	do_( same )	informed = 1;
		end
	in_( "\\" ) bgn_
		on_( 'x' )	do_( "\\x" )
		on_other	do_( "base" )	informed = 1;
		end
		in_( "\\x" ) bgn_
			on_xdigit	do_( "\\x_" )
			end
		in_( "\\x_" ) bgn_
			on_xdigit	do_( "base" ) informed = 1;
			end
	in_( "[" ) bgn_
		ons( "[-]" )	; // err
		on_( '\\' )	do_( "[\\" )
		on_( '^' )	do_( "[^" )
		on_other 	do_( "[_" )
		end
		in_( "[\\" ) bgn_
			on_( 'x' )	do_( "[\\x" )
			on_other	do_( "[_" )
			end
		in_( "[\\x" ) bgn_
			on_xdigit	do_( "[\\x_" )
			end
		in_( "[\\x_" ) bgn_
			on_xdigit	do_( "[_" )
			end
	in_( "[^" ) bgn_
		ons( "[-]" )	; // err
		on_( '\\' )	do_( "[\\" )
		on_other 	do_( "[_" )
		end
	in_( "[_" ) bgn_
		on_( '[' )	; // err
		on_( ']' )	do_( "base" )	informed = 1;
		on_( '-' )	do_( "[_-" )
		on_( '\\' )	do_( "[\\" )
		on_other	do_( same )
		end
	in_( "[_-" ) bgn_
		ons( "[-]" )	; // err
		on_( '\\' )	do_( "[_-\\" )
		on_other	do_( "[_-_" )
		end
		in_( "[_-\\" ) bgn_
			on_( 'x' )	do_( "[_-\\x" )
			on_other	do_( "[_-_" )
			end
		in_( "[_-\\x" ) bgn_
			on_xdigit	do_( "[_-\\x_" )
			end
		in_( "[_-\\x_" ) bgn_
			on_xdigit	do_( "[_-_" )
			end
		in_( "[_-_" ) bgn_
			ons( "[-" )	; // err
			on_( ']' )	do_( "base" )	informed = 1;
			on_( '\\' )	do_( "[\\" )
			on_other 	do_( "[_" )
			end
	SMDefault
		in_none_sofar	do_( "" )	fprintf( stderr, ">>>>>> smgen: Error: "
							"unknown state \"%s\" <<<<<<\n", state );
						exit( - 1 );
		in_other	do_( "" )	fprintf( stderr, ">>>>>> smgen: Syntax Error: "
							"l%dc%d\n", line, column );
						exit( - 1 );
	SMEnd
	return 1;
}

//---------------------------------------------------------------------------
//	smBuild
//---------------------------------------------------------------------------
static int table_compare( listItem *t1, listItem *t2 );
static void reduce( listItem **, Pair *, listItem **, char *, listItem ** );
static char *post_p( char *p );
static char *post_s( char *s );
static char *pre_s( char *s, char *expr[], int nums );

listItem *
smBuild( char *expr[], int nums )
/*
	build and return
		StateMachine:{ entry:[ state:{p}, table:{ transition:[ trigger, state ] } ] }
	where
		. trigger is either a valid [min,max] range of events, or NULL
		. state is either
			. if trigger is NULL: PASS or FAIL
			. otherwise: either DONE or a unique combination of {p}
*/
{
	listItem *SM = NULL;
	Pair *init = newPair( NULL, NULL );

	/* determine transition table for init entry = [ NULL, NULL ]
	*/
	Pair *entry = init;
	char *flag = FAIL;
	listItem *trigger = NULL;
	for ( int i=0; i<nums; i++ ) {
		char *p = expr[i];
		if ( *p ) addItem( &trigger, p );
		else flag = PASS;
	}
	listItem *active_next = NULL;
	reduce( &SM, entry, &trigger, flag, &active_next );

	/* determine transition table for newly created SM entries
	*/
	listItem *active;
	while (( active=active_next )) {
		active_next = NULL;
		while (( entry=popListItem( &active ) )) {
			listItem *state = entry->name;
			flag = FAIL;
			for ( listItem *j=state; j!=NULL; j=j->next ) {
				char *p = j->ptr;
				char *s = post_p( p );
				char *next_p;
				for ( int done=0; !done; ) {
					switch ( *s ) {
					case '|':
						for ( s=post_p(s+1); *s!=')'; s=post_p(post_s(s)) ) {}
						s++; break;
					case ')':
					case '?':
						s++; break;
					case '*':
					case '+':
						addItem( &trigger, pre_s(s,expr,nums) );
						s++; break;
					default:
						next_p = s;
						done = 1;
					}
				}
				if ( *next_p ) addItem( &trigger, next_p );
				else flag = PASS;
			}
			reduce( &SM, entry, &trigger, flag, &active_next );
		}
	}
	reorderListItem( &SM );

	/* Reduce init state if find match
	*/
	listItem *match = NULL;
	for ( listItem *i=SM; i!=NULL; i=i->next ) {
		Pair *r = i->ptr;
		if ( !table_compare( r->value, init->value ) ) {
			match = r->name;
			break;
		}
	}
	if (( match )) {
		init->name = match;
		// free init transition table
		for ( listItem *i=init->value; i!=NULL; i=i->next ) {
			Pair *transition = i->ptr;
			Pair *range = transition->name;
			if (( range )) freePair( range );
			freePair( transition );
		}
		freeListItem((listItem **) &init->value );
	}
	addItem( &SM, init );
	return SM;
}

static int compare( listItem *state1, listItem *state2 );
static int
table_compare( listItem *t1, listItem *t2 )
{
	for ( ; (t1); t1=t1->next, t2=t2->next ) {
		if ( !t2 ) return 1;
		Pair *r1 = t1->ptr;
		Pair *r2 = t2->ptr;
		Pair *range1 = r1->name;
		Pair *range2 = r2->name;
		if ( range1==NULL || range2==NULL ) {
			if ( range1 != range2 )
				return 1;
			if ( r1->value != r2->value )
				return 1;
		}
		else if ( (int) range1->name != (int) range2->name )
			return 1;
		else if ( (int) range1->value != (int) range2->value )
			return 1;
		else if ( compare( r1->value, r2->value ) )
			return 1;
	}
	return ( !!t2 );
}
static int
compare( listItem *state1, listItem *state2 )
/*
	assumption: state1 != NULL
*/
{
	if (state1==DONE) return (state2!=DONE);
	if (state2==DONE) return (state1!=DONE);
	do {
		if ( state2==NULL || state1->ptr!=state2->ptr )
			return 1;
		state1 = state1->next;
		state2 = state2->next;
	}
	while (( state1 ));
	return ( !!state2 );
}

//---------------------------------------------------------------------------
//	reduce
//---------------------------------------------------------------------------
static void mark( listItem *table[], char *trigger, char *position );
static void lookup( listItem **SM, listItem **state, listItem **active_next );
static listItem *post_g( char *p );
static char *beyond( char *s );

static void
reduce( listItem **SM, Pair *entry, listItem **trigger, char *flag, listItem **active_next )
/*
	generates the complete transition table for our state/entry, turning
	initial triggers into ranges of events, so that we end up with
		table:{ transition:[ trigger:[min,max], state:{p} ] } ]}
	where trigger:NULL means state is either PASS or FAIL
*/
{
	listItem *table[ 256 ];
	memset( table, 0, sizeof(table) );

	/* first inform transition table as an array indexed by input event
	   so that for each event we have table[event]=state:{p}
	*/
	char *p, *s, *next_p;
	listItem *ward = NULL;
	while (( p=popListItem( trigger ) )) {
		if ( lookupIfThere( ward, p ) )
			continue;
		addIfNotThere( &ward, p );
		s = post_p( p );
		next_p = beyond( s );
		if ( *s=='?' || *s=='*' ) {
			if ( *next_p ) addItem( trigger, next_p );
			else flag = PASS; // p as a trigger is optional
		}
		if ( *p=='(' ) {
			listItem *entries = post_g( p );
			while (( p=popListItem(&entries) ))
				addItem( trigger, p );
			continue;
		}
		if ( *next_p=='\0' ) {
			/* mark either ( table, p, p ) or ( table, p, DONE ) depending on
			   whether there is a repeat suffix standing between p and next_p
			*/
			for ( int done=0; !done; ) {
				switch ( *s ) {
				case '|':
					for ( s=post_p(s+1); *s!=')'; s=post_p(post_s(s)) ) {}
					s++; break;
				case ')':
				case '?':
					s++; break;
				case '*':
				case '+':
					mark( table, p, p );
					done = 1; break;
				default:
					mark( table, p, DONE );
					done = 1; break;
				}
			}
		}
		else mark( table, p, p );
	}
	freeListItem( &ward );

	/* then reduce table[] array into list:{[ trigger, state ]}
	   where trigger is either
		. a Pair representing a consecutive range of events
			. in which case state is either DONE or
			  a unique {p} combination
		. NULL - unless we already have full event coverage
			. in which case state is either FAIL or PASS
	*/
	listItem *list = NULL;
	listItem *state = NULL;
	int lower = 0, disjunct = 0;
	for ( int i=0; i<256; i++ ) {
		if ((state) && !compare( state, table[i] ) ) {
			if ( state!=DONE ) freeListItem( &table[i] );
		}
		else {
			if (( state )) {
				Pair *pair = new_pair( lower, i-1 );
				addItem( &list, newPair( pair, state ) );
			}
			if (( state = table[i] )) {
				lookup( SM, &state, active_next );
				lower = i;
			}
			else disjunct = 1;
		}
	}
	if (( state )) {
		Pair *pair = new_pair( lower, 255 );
		addItem( &list, newPair( pair, state ) );
	}
	if ( disjunct ) addItem( &list, newPair( NULL, flag ) );
	reorderListItem( &list );

	/* Further reduction: right now states are indexed by active (reached)
	   position(s). Now for each state, if we can find a previously defined
	   state with all the same transitions, then we replace all references
	   to the latter with reference to the former.
	*/
	listItem *match = NULL;
	for ( listItem *i=*SM; i!=NULL; i=i->next ) {
		Pair *r = i->ptr;
		if ( !table_compare( r->value, list ) ) {
			match = r->name;
			break;
		}
	}
	if (( match )) {
		removeItem( SM, entry );
		// replace all references to entry->name with match
		listItem *state = entry->name;
		for ( listItem *i=*SM; i!=NULL; i=i->next ) {
			Pair *r = i->ptr;
			for ( listItem *j=r->value; j!=NULL; j=j->next ) {
				Pair *e = j->ptr;
				if ( !compare( e->value, state ) )
					e->value = match;
			}
		}
		// free entry
		freeListItem( &state );
		for ( listItem *i=list; i!=NULL; i=i->next ) {
			Pair *transition = i->ptr;
			Pair *range = transition->name;
			if (( range )) freePair( range );
			freePair( transition );
		}
		freeListItem( &list );
		freePair( entry );
	}
	else {
		/* register final transition table as part of SM entry
		*/
		entry->value = list;
	}
}
static void
mark( listItem *table[], char *p, char *q )
/*
	foreach i, if !rxcmp(p,i) then add q to table[i]={p}
	note that {p} order is guaranteed by addIfNotThere()
	so as to facilitate ulterior state comparison
*/
{
	for ( int i=0; i<256; i++ ) {
		listItem *r = table[i];
		if ( r == DONE )
			continue;
		if ( rxcmp( p, i ) )
			continue;
		if ( q == DONE ) {
			freeListItem( &table[ i ] );
			table[ i ] = DONE;
		}
		else addIfNotThere( &table[i], q );
	}
}
static void
lookup( listItem **SM, listItem **state, listItem **active_next )
/*
	return unique {p} combination matching *state
*/
{
	if ( *state == DONE ) return;
	for ( listItem *i=*SM; i!=NULL; i=i->next ) {
		Pair *r = i->ptr;
		if ( !compare( r->name, *state ) ) {
			freeListItem( state );
			*state = r->name;
			return;
		}
	}
	Pair *r = newPair( *state, NULL );
	addItem( active_next, r );
	addItem( SM, r );
}

//---------------------------------------------------------------------------
//	post_p, post_s, pre_s, post_g, beyond
//---------------------------------------------------------------------------
static char *post_p( char *p )
/*
	if *p in )|?*+ returns p
	else if *p=='(' returns position after corresponding ')', suffix or not
	otherwise returns position after p, suffix or not
*/
{
	int level = 0;
	char *s = p;
	do {
		switch ( *s ) {
		case '|':
		case '?':
		case '*':
		case '+':
			if ( !level ) return s;
			s++; break;
		case ')':
			if ( !level ) return s;
			level--;
			s++; break;
		case '(':
			level++;
			s++; break;
		case '[':
			for ( s++; *s!=']'; )
				s += ( *s=='\\' ? ( s[1]=='x' ? 4 : 2 ) : 1 );
			s++; break;
		case '\\':
			s += ( s[1]=='x' ? 4 : 2 );
			break;
		default:
			s++;
		}
	} while ( level );
	return s;
}
static char *post_s( char *s )
{
	return ( strmatch( ")|?*+", *s ) ? s+1 : s );
}
static char *stepback( char *s, char *expr[], int nums );
static char *pre_s( char *s, char *expr[], int nums )
/*
	assumption: *s=='*' or '+'
*/
{
	int level = 0;
	for ( ; ; ) {
		s = stepback( s, expr, nums );
		switch ( *s ) {
		case ')':
			level++;
			break;
		case '|':
		case '?':
		case '*':
		case '+':
			break;
		case '(':
			level--;
			if ( !level ) return s;
			break;
		case ']':
			do s=stepback(s,expr,nums); while (*s!='[');
			// no break
		default:
			if ( !level ) return s;
		}
	}
}
static char *stepback( char *s, char *expr[], int nums )
/*
	assumption: expression(s) verified
*/
{
	s--;
	if ( s[-1]=='\\' ) {
		for ( int i=0; i<nums; i++ )
			if (expr[i]==s) return s;
		return s-1;
	}
	if ( s[-3]=='\\' && s[-2]=='x' ) {
		for ( int i=0; i<nums; i++ )
		for ( int j=1; j<3; j++ )
			if (expr[i]==s-j) return s;
		if ( s[-4]=='\\' ) {
			for ( int i=0; i<nums; i++ )
				if (expr[i]==s-3) return s-3;
			return s;
		}
		return s-3;
	}
	return s;
}
static listItem *post_g( char *p )
/*
	assumption: *p=='('
*/
{
	p++;
	listItem *entries = NULL;
	for ( int level=1; level; ) {
		switch ( *p ) {
		case '(': level++; p++; break;
		case ')': level--; p++; break;
		case '|': p++; break;
		default:
			addItem( &entries, p );
			for ( p=post_p(p); *p!=')' && *p!='|'; p=post_p(post_s(p)) ) {}
		}
	}
	return entries;
}
static char *beyond( char *s )
{
	for ( ; ; )
		switch ( *s ) {
		case '|':
			for ( s=post_p(s+1); *s!=')'; s=post_p(post_s(s)) ) {}
			// no break
		case ')':
		case '?':
		case '*':
		case '+':
			s++; break;
		default:
			return s;
		}
}

//---------------------------------------------------------------------------
//	smOutput
//---------------------------------------------------------------------------
static void beautify( listItem *SM, listItem *table, int ndx, int tab );
static int state_id( listItem *SM, listItem *state, Pair **entry );
static void output_c( int c, int in_quote );
static void output_r( int *range, int in_bracket );

#define TAB(n)	for ( int i=0; i<n; i++ ) printf( "\t" );
#define OUTPUT_S( id, ndx ) { \
	printf( "\t? " ); \
	if ( id == 0 ) printf( "%s\n", DONE ); \
	else if ( id == ndx ) printf( ".\n" ); \
	else printf( "%.4X\n", id ); }

void
smOutput( listItem *SM, int tab )
/*
	output
		StateMachine:{ entry:[ state:{p}, table:{ transition:[ trigger, state ] } ] }
	where
		. trigger is either a valid [min,max] range of events, or NULL
		. state is either
			. if trigger is NULL (representing "else"): PASS or FAIL
			. otherwise either DONE or a unique combination of {p}
*/
{
	TAB(tab); printf( "do :state:\n" );
	listItem *init;
	int ndx = 0;
	for ( listItem *i=SM; i!=NULL; i=i->next, ndx++ ) {
		Pair *entry = i->ptr;
		if ( ndx == 0 ) {
			init = entry->name;
			TAB( tab+1 );
			if (( init )) {
				int id = state_id( SM, init, &entry );
				printf( "{ ~., %.4X }", id );
			}
			else printf( "~." );
		}
		else if ( !compare( entry->name, init ) )
			continue;
		else {
			TAB(tab+1); printf( "%.4X", ndx );
		}
		listItem *table = entry->value;
		// handle here special case where range is [\x00-\xFF]
		if ( table->next == NULL ) {
			Pair *transition = table->ptr;
			listItem *state = transition->value;
			printf( " ? %.4X\n", state_id(SM,state,NULL) );
			continue;
		}
		printf( " ? :event:\n" );
#ifdef BEAUTIFY
		beautify( SM, table, ndx, tab );
#else
		char *other = NULL;
		for ( listItem *j=table; j!=NULL; j=j->next ) {
			Pair *transition = j->ptr;
			Pair *r = transition->name;
			listItem *state = transition->value;
			if ( r == NULL ) {
				other = (char *) state;
				break;
			}
			TAB( tab+2 );
			int range[ 2 ];
			range[ 0 ] = (int) r->name;
			range[ 1 ] = (int) r->value;
			if ( range[0] == range[1] ) {
				output_c( range[0], 1 );
				printf( "\t" );
			}
			else output_r( range, 1 );
			int id = ( state==DONE ? 0 : state_id(SM,state,NULL));
			OUTPUT_S( id, ndx );
		}
		if (( other )) { TAB(tab+2); printf( ".\t\t? %s\n", other ); }
#endif	// BEAUTIFY
	}
}
#ifdef BEAUTIFY
#define ANTIRANGE
#endif
#define SET_RANGE( pair, range ) \
	range[0] = (int) ((Pair*)pair)->name; \
	range[1] = (int) ((Pair*)pair)->value;
static void
beautify( listItem *SM, listItem *table, int ndx, int tab )
{
	/* index table by transition states
	*/
	Registry *registry = newRegistry( IndexedByNumber );
	Pair *entry; int range[ 2 ];

	char *other = NULL;
	for ( listItem *i=table; i!=NULL; i=i->next ) {
		Pair *transition = i->ptr;
		Pair *r = transition->name;
		listItem *state = transition->value;
		if ( r == NULL ) {
			other = (char *) state;
			break;
		}
		int id = ( state==DONE ? 0 : state_id(SM,state,NULL));
		if (( entry = registryLookup( registry, &id ) ))
			addItem((listItem**) &entry->value, r );
		else 
			registryRegister( registry, &id, newItem(r) );
	}
	/* 1. output single events all together
	*/
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		/* entry->value holds the list of input ranges (in decreasing order)
		   leading all to the entry->name state
		*/
		Pair *entry = i->ptr;
		int state = (int) entry->name;
		listItem *list = entry->value;

		listItem *stack = NULL;
		for ( listItem *j=list; j!=NULL; j=j->next ) {
			SET_RANGE( j->ptr, range );
			if ( range[0] != range[1] )
				continue;
			add_item( &stack, range[0] );
		}
		if (( stack )) {
			int c = (int) popListItem( &stack );
			int in_quote = !stack;
			TAB(tab+2);
			if ( !in_quote ) printf( "[" );
			do output_c( c, in_quote );
			while (( c = (int) popListItem( &stack ) ));
			if ( !in_quote ) printf( "]" );
			OUTPUT_S( state, ndx );
		}
	}
	/* 2. output ranges not ending with \xFF
	*/
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		/* entry->value holds the list of input ranges (in decreasing order)
		   leading all to the entry->name state
		*/
		Pair *entry = i->ptr;
		listItem *list = entry->value;
		Pair *r = list->ptr;
#ifdef ANTIRANGE
		if ( (int) r->value == 255 )
			continue;
#endif
		int state = (int) entry->name;
		for ( listItem *j=list; j!=NULL; j=j->next ) {
			SET_RANGE( j->ptr, range );
			if ( range[0] == range[1] )
				continue;
			TAB(tab+2); output_r( range, 1 );
			OUTPUT_S( state, ndx );
		}
	}
	/* 3. output ranges ending with \xFF
	*/
#ifdef ANTIRANGE
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		/* entry->value holds the list of input ranges (in decreasing order)
		   leading all to the same entry->name state
		*/
		Pair *entry = i->ptr;
		listItem *list = entry->value;
		Pair *r = list->ptr;
		if ( (int) r->value != 255 )
			continue;

		/* our working hypothesis is that the user did not likely specify
		   input \xFF. So we are going to output the inverted complementary
		   set of all ranges leading to that state - as one [^ ]
		*/
		int state = (int) entry->name;
		listItem *antirange = NULL;
		int upper = 255;
		for ( listItem *j=list; j!=NULL; j=j->next ) {
			SET_RANGE( j->ptr, range );
			if ( range[0] ) {
				if ( upper != 255 ) {
					addItem( &antirange, new_pair( range[1]+1, upper ) );
				}
				upper = range[0]-1;
			}
			else upper = 255;
		}
		if ( upper != 255 ) addItem( &antirange, new_pair( 0, upper ) );

		TAB(tab+2);
		printf( "[^" );
		for ( listItem *j=antirange; j!=NULL; j=j->next ) {
			SET_RANGE( j->ptr, range );
			if ( range[0] == range[1] )
				output_c( range[0], 0 );
			else output_r( range, 0 );
		}
		printf( "]" );
		OUTPUT_S( state, ndx );

		for ( listItem *j=antirange; j!=NULL; j=j->next )
			freePair(( Pair *) j->ptr );
		freeListItem( &antirange );
	}
#endif	// ANTIRANGE
	/* 4. output else
	*/
	if (( other )) { TAB(tab+2); printf( ".\t? %s\n", other ); }

	/* cleanup
	*/
	for ( listItem *i=registry->entries; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		freeListItem((listItem **) &entry->value );
	}
	freeRegistry( registry, NULL );
}
static int
state_id( listItem *SM, listItem *state, Pair **entry )
{
	int id = 1;
	for ( listItem *i=SM->next; i!=NULL; i=i->next, id++ ) {
		Pair *r = i->ptr;
		if ( !compare( r->name, state ) ) {
			if (( entry )) *entry = r;
			return id;
		}
	}
	fprintf( stderr, "Error: smgen: state unknown\n" );
	exit( -1 );
}
static void
output_c( int c, int in_quote )
{
	if ( in_quote ) printf( "'" );
	switch (c) {
	case '\0': printf( "\\0" ); break;
	case '\t': printf( "\\t" ); break;
	case '\n': printf( "\\n" ); break;
	case '\\': printf( "\\\\" ); break;
	case '\'':
		if ( !in_quote ) printf( "'" );
		else printf( "\\'" );
		break;
	case '-':
	case ']':
	case '[':
		if ( !in_quote ) {
			printf( "\\" );
		}
		// no break
	default:
		if ( is_printable(c) ) printf( "%c", c );
		else printf( "\\x%.2X", *(unsigned char *)&c );
	}
	if ( in_quote ) printf( "'" );
}
static void
output_r( int *range, int in_bracket )
{
#ifdef ANTIRANGE
#define PUTC(c) \
	if ( is_printable(c) ) printf( "%c", c ); \
	else printf( "\\x%.2X", *(unsigned char*)&c );
#else
#define PUTC(c) printf( "\\x%.2X", *(unsigned char*)&c );
#endif
	if ( in_bracket ) printf( "[" );
	PUTC( range[0] );
	if ( range[1] != range[0]+1 )
		printf( "-" );
	PUTC( range[1] );
	if ( in_bracket ) printf( "]" );
}

//---------------------------------------------------------------------------
//	smFree
//---------------------------------------------------------------------------
void
smFree( listItem **sm )
/*
   free StateMachine:{ entry:[ state:{p}, table:{ transition:[ range, state ] } ] }
*/
{
	for ( listItem *i=*sm; i!=NULL; i=i->next ) {
		Pair *entry = i->ptr;
		listItem *state = entry->name;
		if ( i != *sm ) freeListItem( &state );
		else if (( state )) continue;
		listItem *table = entry->value;
		for ( listItem *j=table; j!=NULL; j=j->next ) {
			Pair *transition = j->ptr;
			Pair *range = transition->name;
			if (( range )) freePair( range );
			freePair( transition );
		}
		freeListItem( &table );
		freePair( entry );
	}
	freeListItem( sm );
}

