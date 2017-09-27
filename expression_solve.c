#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "database.h"
#include "registry.h"
#include "kernel.h"
#include "output.h"

#include "api.h"
#include "expression.h"
#include "expression_util.h"
#include "filter_util.h"
#include "variables.h"

// #define DEBUG
#define MEMOPT

#ifdef DEBUG
#define RETURN( value ) \
	{ output( Debug, "expression_solve: exiting - success=%d", value ); return value; }
#define DEBUG_1	\
	output( Debug, "solve: no sub-expression[ %d ], starting from '%s', any:%d, active:%d, inactive:%d, mark:%d", \
	count, expression->sub[count].result.identifier.value, expression->sub[count].result.any, expression->sub[count].result.active, \
	expression->sub[count].result.inactive, ( expression->result.mark & ( 1 << count ) ) >> count );
#define DEBUG_2 \
	output( Debug, "\tsource: %0x, any:%d, active: %d, inactive:%d: mark:%d", (int) r_sub[0], \
		expression->sub[0].result.any, expression->sub[0].result.active, expression->sub[0].result.inactive, \
		expression->result.mark & 1 ); \
	output( Debug, "\tmedium: %0x, any:%d, active: %d, inactive:%d: mark:%d", (int) r_sub[1], \
		expression->sub[1].result.any, expression->sub[1].result.active, expression->sub[1].result.inactive, \
		( expression->result.mark & 2 ) >> 1 ); \
	output( Debug, "\ttarget: %0x, any:%d, active: %d, inactive:%d: mark:%d", (int) r_sub[2], \
		expression->sub[2].result.any, expression->sub[2].result.active, expression->sub[2].result.inactive, \
		( expression->result.mark & 4 ) >> 2 ); \
	output( Debug, "\tinstance: %0x, any:%d, active: %d, inactive:%d: mark:%d", (int) r_sub[3], \
		expression->sub[3].result.any, expression->sub[3].result.active, expression->sub[3].result.inactive, \
		( expression->result.mark & 8 ) >> 3 );
#else
#define RETURN( value ) return value;
#define DEBUG_1
#define DEBUG_2
#endif

typedef struct {
	unsigned int any : 1;
	unsigned int active : 1;
	unsigned int inactive : 1;
	unsigned int not : 1;
} FlagSub;

typedef struct {
	void *ptr;
	int active;
} CandidateSub;

/*---------------------------------------------------------------------------
	filter_results utilities
---------------------------------------------------------------------------*/
#define s_loop( count, results, ptr ) \
	for ( listItem *i = results; i!=NULL; i=i->next ) if ( set_s( ptr, i, count ) )

static int
set_s( ExpressionSub **result, listItem *i, int count )
{
	ExpressionSub *s = (ExpressionSub *) i->ptr;
	if ( count < 3 ) {
		if ( s->e == NULL )
			return 0;
		s = &s->e->sub[ count ];
	}
	*result = s;
	return 1;
}

#define e_loop( count, results, ptr ) \
	for ( listItem *i = results; i!=NULL; i=i->next ) if ( set_e( ptr, i, count ) )

static int
set_e( Entity **result, listItem *i, int count )
{
	Entity *e = (Entity *) i->ptr;
	if ( count < 3 ) {
		e = e->sub[ count ];
		if ( e == NULL )
			return 0;
	}
	*result = e;
	return 1;
}

/*---------------------------------------------------------------------------
	filter_results
---------------------------------------------------------------------------*/
#define IDENTIFIER 1
#define ENTITY 2
static void
filter_results( listItem **list, Expression *expression, int count, void *instance, int type, listItem *results )
{
	// special case...
	if ( expression->sub[ 1 ].result.none ) {
		count = 3;
	}

	if ( CN.context->expression.mode == ReadMode )
	{
		ExpressionSub *s;
		if ( type == IDENTIFIER )
		{
			s_loop( count, results, &s ) {
				char *identifier = s->result.identifier.value;
				if ( identifier == NULL ) continue;
				if ( !strcmp( identifier, (char *)instance ) ) {
					addItem( list, s );
				}
			}
		}
		else if ( type == ENTITY )
		{
			Entity *entity = (Entity *) instance;
			if ( entity == CN.nil ) {
				s_loop( count, results, &s ) {
					if ( s->result.identifier.type == NullIdentifier ) {
						addItem( list, s );
					}
				}
			} else if ( entity->sub[ 1 ] == NULL ) {
				char *identifier = cn_name( entity );
				if ( identifier != NULL ) s_loop( count, results, &s ) {
					if ( !strcmp( s->result.identifier.value, identifier ) )
						addItem( list, s );
				}
			} else {
				Expression *expr = cn_expression( entity );
				if ( expr != NULL ) s_loop( count, results, &s ) {
					listItem *restore_filter = CN.context->expression.filter;
					listItem *filter = newItem( s );
					CN.context->expression.filter = filter;
					if ( expression_solve( expr, count, CN.context ) > 0 ) {
						addItem( list, s );
					}
					CN.context->expression.filter = restore_filter;
					freeItem( filter );
				}
				freeExpression( expr );
			}
		}
#ifdef MEMOPT
		else if ( count != 3 ) {
			s_loop( count, results, &s ) {
				addItem( list, s );
			}
		}
		else {
			*list = results;
		}
#else
		else {
			s_loop( count, results, &s ) {
				addItem( list, s );
			}
		}
#endif
	}
	else	// not in ReadMode
	{
		Entity *e;
		if ( results == NULL ) {
			if ( instance != NULL ) {
				addItem( list, instance );
			}
			return;
		}
		else if ( instance != NULL )
		{
			e_loop( count, results, &e ) {
				if ( e == (Entity *) instance ) {
					addIfNotThere( list, e );
					return;
				}
			}
		}
#ifdef MEMOPT
		else if ( count != 3 ) {
			e_loop( count, results, &e ) {
				addIfNotThere( list, e );
			}
		}
		else {
			*list = results;
		}
#else
		else {
			e_loop( count, results, &e ) {
				addIfNotThere( list, e );
			}
		}
#endif
	}
}

/*---------------------------------------------------------------------------
	sub_solve
---------------------------------------------------------------------------*/
static int solve( Expression *expression, int as_sub, listItem *results );

static int
sub_solve( Expression *expression, int count, listItem *results, _context *context )
{
	int success = 1;
	ExpressionSub *sub = expression->sub;

#ifdef DEBUG
	output( Debug, "sub_solve: entering count=%d", count );
#endif
	listItem **sub_results = &sub[ count ].result.list;

	int type = sub[ count ].result.identifier.type;
	switch ( type ) {
	case NullIdentifier:
		filter_results( sub_results, expression, count, CN.nil, ENTITY, results );
		if ( *sub_results == NULL ) return 0;
		break;
	case VariableIdentifier:
		if ( sub[ count ].result.identifier.value != NULL )
		{
			char *identifier = sub[ count ].result.identifier.value;
			registryEntry *entry;
			if ( sub[ count ].result.lookup ) {
				listItem *stack = context->control.stack;
				context->control.stack = stack->next;
				entry = lookupVariable( context, identifier );
				context->control.stack = stack;
			} else {
				entry = lookupVariable( context, identifier );
			}
			if ( entry == NULL ) {
				return output( Error, "variable '%%%s' not found", identifier );
			}
			VariableVA *variable = (VariableVA *) entry->value;
			switch ( variable->type ) {
			case EntityVariable:
				if (( count == 3 ) && (( results == NULL ) || ( context->expression.mode == ReadMode ))) {
					context->expression.mode = EvaluateMode;
					for ( listItem *i = (listItem *) variable->data.value; i!=NULL; i=i->next ) {
						addItem( sub_results, i->ptr );
					}
					break;
				}
				for ( listItem *i = (listItem *) variable->data.value; i!=NULL; i=i->next ) {
					filter_results( sub_results, expression, count, i->ptr, ENTITY, results );
				}
				if ( *sub_results == NULL ) return 0;
				break;
			case ExpressionVariable:
				filter_results( sub_results, expression, count, NULL, 0, results );
				if (( results != NULL ) && ( *sub_results == NULL )) return 0;

				listItem *restore_filter = context->expression.filter;
				context->expression.filter = *sub_results;
				int sub_count = sub[ 1 ].result.none ? 3 : count;
				success = expression_solve( ((listItem *) variable->data.value )->ptr, sub_count, context );
				context->expression.filter = restore_filter;
#ifdef MEMOPT
				if ( sub_count != 3 )
#endif
					freeListItem( sub_results );
				if ( success <= 0 ) return success;
				*sub_results = context->expression.results;
				context->expression.results = NULL;
				break;
			case LiteralVariable:
				if (( count == 3 ) && (( results == NULL ) || ( context->expression.mode != ReadMode ))) {
					context->expression.mode = ReadMode;
					for ( listItem *i = (listItem *) variable->data.value; i!=NULL; i=i->next ) {
						addItem( sub_results, i->ptr );
					}
					break;
				} else {
					filter_results( sub_results, expression, count, NULL, 0, results );
					if (( results != NULL ) && ( *sub_results == NULL )) return 0;
				}
				listItem *last_results = NULL;
				sub_count = sub[ 1 ].result.none ? 3 : count;
				if ( context->expression.mode == ReadMode )
				{
					for ( listItem *i = (listItem *) variable->data.value; i!=NULL; i=i->next ) {
						ExpressionSub *s = (ExpressionSub *) i->ptr;
						success = solve( s->e, sub_count, *sub_results );
						if ( success > 0 ) {
							last_results = catListItem( s->e->result.list, last_results );
							s->e->result.list = NULL;
						}
					}
				}
				else {
					restore_filter = context->expression.filter;
					context->expression.filter = *sub_results;
					for ( listItem *i = (listItem *) variable->data.value; i!=NULL; i=i->next ) {
						ExpressionSub *s = (ExpressionSub *) i->ptr;
						success = expression_solve( s->e, sub_count, context );
						if ( success > 0 ) {
							last_results = catListItem( context->expression.results, last_results );
							context->expression.results = NULL;
						}
					}
					context->expression.filter = restore_filter;
				}
#ifdef MEMOPT
				if ( sub_count != 3 )
#endif
					freeListItem( sub_results );
				if ( last_results == NULL ) return 0;
				*sub_results = last_results;
				break;
			case NarrativeVariable:
				return output( Error, "'%%%s' is a narrative variable - "
					"not allowed in expressions", identifier );
			}
		}
		else if ( sub[ count ].e != NULL )	// residue
		{
			filter_results( sub_results, expression, count, NULL, 0, results );
			if (( results != NULL ) && ( sub_results == NULL )) return 0;

			context->expression.filter = *sub_results;
			int sub_count = sub[ 1 ].result.none ? 3 : count;
			listItem *restore_filter = context->expression.filter;
			success = expression_solve( sub[ count ].e, sub_count, context );
			context->expression.filter = restore_filter;
#ifdef MEMOPT
			if ( sub_count != 3 )
#endif
				freeListItem( sub_results );
			if ( success <= 0 ) return success;

			*sub_results = context->expression.results;
			context->expression.results = NULL;
			break;
		}
		else {
			return output( Error, "expression terms are incomplete" );
		}
		break;
	case QueryIdentifier:
		; listItem *restore_filter = context->expression.filter;
		int restore_mode = context->expression.mode;

		context->expression.mode = EvaluateMode;
		success = expression_solve( sub[ count ].e, 3, context );
		context->expression.mode = restore_mode;
		context->expression.filter = restore_filter;
		if ( success <= 0 ) return success;

		for ( listItem *i = context->expression.results; i!=NULL; i=i->next ) {
			filter_results( sub_results, expression, count, i->ptr, ENTITY, results );
		}
		if ( *sub_results == NULL ) return 0;
		break;
	case DefaultIdentifier:
		if ( sub[ count ].result.identifier.value == NULL ) {
			return output( Error, "expression terms are incomplete" );
		}
		DEBUG_1;
		char *identifier = sub[ count ].result.identifier.value;
		if ( context->expression.mode == ReadMode ) {
			filter_results( sub_results, expression, count, identifier, IDENTIFIER, results );
			if ( *sub_results == NULL ) return 0;
			break;
		}
		Entity *e = cn_entity( identifier );
	       	if ( e != NULL )  {
			filter_results( sub_results, expression, count, e, ENTITY, results );
			if ( *sub_results == NULL ) return 0;
		}
		else if ( context->expression.mode == InstantiateMode )
		{
			e = cn_new( strdup( identifier ) );
			addItem( sub_results, e );
#ifdef DEBUG
			output( Debug, "set_sub_identifier[ %d ]: created new: '%s', addr: %0x",
				count, identifier, (int) e );
#endif
		} else if ( !sub[ count ].result.not ) {
			if ( context->expression.mode == EvaluateMode ) {
				return 0;
			} else {
				return output( Error, "'%s' is not instantiated", identifier );
			}
		}
		break;
	default:
		if ( sub[ count ].e == NULL ) {
			return output( Error, "expression terms are incomplete" );
		}
#ifdef DEBUG_FULL
		output( Debug, "recursing in SOLVE: count=%d, not=%d", count, sub[ count ].result.not );
#endif
		filter_results( sub_results, expression, count, NULL, 0, results );
		if (( results != NULL ) && ( *sub_results == NULL )) return 0;

		Expression *sub_e = sub[ count ].e;
		int sub_count = sub[ 1 ].result.none ? 3 : count;
		success = solve( sub_e, sub_count, *sub_results );
#ifdef MEMOPT
		if ( sub_count != 3 )
#endif
			freeListItem( sub_results );
		if ( success <= 0 ) return success;
#ifdef DEBUG
		output( Debug, "solve: returning - %0x [ %d ]", (int) sub[ count ].e, count );
#endif
		*sub_results = sub_e->result.list;
		sub_e->result.list = NULL;
		break;
	}
	return success;
}

/*---------------------------------------------------------------------------
	restore
---------------------------------------------------------------------------*/
static int
restore( Expression *expression, int restore_mode, listItem *results, _context *context )
{
	listItem *list = NULL;
	if ( context->expression.mode == ReadMode )
	{
#ifdef DEBUG
		output( Debug, "restoring mode & results - from ReadMode, to mode=%d...", restore_mode );
#endif
		// Here we need to translate the resulting literals into entities
		context->expression.mode = restore_mode;
		listItem *restore_filter = context->expression.filter;
		context->expression.filter = results;
		for ( listItem *i = expression->result.list; i!=NULL; i=i->next ) {
			ExpressionSub *s = (ExpressionSub *) i->ptr;
			int success = expression_solve( s->e, 3, context );
			if ( success > 0 ) {
				list = catListItem( context->expression.results, list );
				context->expression.results = NULL;
			}
		}
		context->expression.filter = restore_filter;
	}
	else
	{
#ifdef DEBUG
		output( Debug, "restoring mode & results - from mode=%d to ReadMode...", restore_mode );
#endif

		// Here we need to translate the resulting entities into literals
		context->expression.mode = restore_mode;
		for ( listItem *i = expression->result.list; i!=NULL; i=i->next ) {
			filter_results( &list, expression, 3, i->ptr, ENTITY, results );
		}
	}
	freeListItem( &expression->result.list );
	expression->result.list = list;
	return ( list == NULL ) ? 0 : 1;
}

/*---------------------------------------------------------------------------
	solve utilities
---------------------------------------------------------------------------*/
static void *
sub_init( listItem **sub, int i, Expression *expression )
{
	sub[ i ] = expression->sub[ i ].result.list;
	return sub[ i ] ? sub[i]->ptr : NULL;
}

static void *
sub_next( listItem **sub, int i )
{
	if ( sub[ i ] ) sub[ i ] = sub[i]->next;
	return sub[ i ] ? sub[i]->ptr : NULL;
}

static listItem *
first_candidate( listItem **sub, FlagSub *flag_sub, Expression *expression, listItem *results )
{
#ifdef DEBUG
	output( Debug, "first_candidate..." );
#endif
	if ( results != NULL ) return results;

	Entity *source = ( sub[0] && !flag_sub[0].not ) ? sub[0]->ptr : NULL;
	Entity *medium = ( sub[1] && !flag_sub[1].not ) ? sub[1]->ptr : NULL;
	Entity *target = ( sub[2] && !flag_sub[2].not ) ? sub[2]->ptr : NULL;
	Entity *instance = ( sub[3] && !flag_sub[3].not ) ? sub[3]->ptr : NULL;

	if ( instance != NULL )
		return sub[3];
	if ( target != NULL )
		return target->as_sub[2];
	if ( source != NULL )
		return source->as_sub[0];
	if ( medium != NULL )
		return medium->as_sub[1];

	return NULL;
}

static int
set_candidate_sub( CandidateSub *sub, int as_sub, listItem *candidate )
{
	if ( CN.context->expression.mode == ReadMode )
	{
		ExpressionSub *entity = (ExpressionSub *) candidate->ptr;
		if ( !test_as_sub( entity, 1, as_sub ) ) {
#ifdef DEBUG
			output( Debug, "failed test_as_sub [ %d ]", as_sub );
#endif
			return 0;
		}
		sub[ 3 ].ptr = entity;
		sub[ 3 ].active = entity->result.active;
		for ( int i=0; i<3; i++ ) {
			if ( entity->e == NULL ) {
				sub[ i ].ptr = NULL;
				sub[ i ].active = 1;
			} else {
				ExpressionSub *s = &entity->e->sub[ i ];
				sub[ i ].ptr = s;
				sub[ i ].active = s->result.active;
			}
		}
	} else {
		Entity *entity = (Entity *) candidate->ptr;
		if ( !test_as_sub( entity, 0, as_sub ) )
		{
#ifdef DEBUG
			output( Debug, "failed test_as_sub [ %d ]", as_sub );
#endif
			return 0;
		}
#ifdef DEBUG
		output( Debug, "passed test_as_sub [ %d ]", as_sub );
#endif
		sub[ 3 ].ptr = entity;
		sub[ 3 ].active = cn_is_active( entity );
		for ( int i=0; i<3; i++ ) {
			Entity *e = entity->sub[ i ];
			if ( e == NULL ) {
				sub[ i ].ptr = NULL;
				sub[ i ].active = 1;
			} else {
				sub[ i ].ptr = e;
				sub[ i ].active = cn_is_active( e );
			}
		}
	}
	return 1;
}

/*---------------------------------------------------------------------------
	solve
---------------------------------------------------------------------------*/
static int
solve( Expression *expression, int as_sub, listItem *results )
{
	_context *context = CN.context;
	ExpressionSub *sub = expression->sub;

#ifdef DEBUG
	output( Debug, "solve: entering - %0x", (int) expression );
#endif

	int restore_mode = context->expression.mode;
	listItem *restore_results = results;

	// fill in sub results
	// -------------------

	if ( sub[ 3 ].result.none || sub[ 3 ].result.any ) {
#ifdef DEBUG
		output( Debug, "solve: no or any sub[ %d ]", 3 );
#endif
	} else {
		int success = sub_solve( expression, 3, results, context );
		if ( success <= 0 ) return success;

		if (( sub[ 3 ].result.identifier.type == VariableIdentifier ) &&
		    ( sub[ 3 ].result.identifier.value != NULL ))
		{
			results = sub[ 3 ].result.list;
		}
	}

	for ( int count = 0; count < 3; count ++ ) {
		if ( sub[ count ].result.none || sub[ count ].result.any ) {
#ifdef DEBUG
			output( Debug, "solve: no or any sub[ %d ]", count );
#endif
			continue;
		}
		int success = sub_solve( expression, count, results, context );
		if ( success <= 0 ) return success;
	}

	if ( context->expression.mode == InstantiateMode )
	{
		if ( sub[ 1 ].result.none ) {
			expression->result.list = sub[ 0 ].result.list;
			sub[ 0 ].result.list = NULL;
			return 1;
		}
		for ( listItem *i = sub[ 0 ].result.list; i!=NULL; i=i->next )
		for ( listItem *j = sub[ 1 ].result.list; j!=NULL; j=j->next )
		for ( listItem *k = sub[ 2 ].result.list; k!=NULL; k=k->next )
		{
			Entity *source = (Entity *) i->ptr;
			Entity *medium = (Entity *) j->ptr;
			Entity *target = (Entity *) k->ptr;
			Entity *e = cn_instance( source, medium, target );
			if ( e == NULL ) {
				e = cn_instantiate( source, medium, target );
#ifdef DEBUG
				output( Debug, "solver: created new relationship instance: ( %0x, %0x, %0x ): %0x",
					(int) source, (int) medium, (int) target, (int) e );
#endif
			}
			addItem( &expression->result.list, e );
		}
		return 1;
	}

	as_sub = expression->result.as_sub | ( 1 << as_sub );

	// 1. process special cases
	// ------------------------
#ifdef DEBUG
	output( Debug, "solver: 1. checking special cases..." );
#endif
	// special cases: "?: a : b" or "?:b" or "?: *:b" or "*:b" or "b"
	if ( sub[ 1 ].result.none ) {
		if ( sub[ 3 ].result.any ) {
			if ( sub[ 0 ].result.any ) {
				return 1;
			}
			else if ( sub[ 0 ].result.not ) {
				invert_results( &sub[ 0 ], as_sub, results );
			} else {
				take_sub_results( &sub[ 0 ], as_sub );
			}
			expression->result.list = sub[ 0 ].result.list;
			sub[ 0 ].result.list = NULL;
		}
		else if ( sub[ 0 ].result.none || sub[ 0 ].result.any )
		{
			if ( sub[ 3 ].result.not ) {
				invert_results( &sub[ 3 ], as_sub, results );
			} else {
				take_sub_results( &sub[ 3 ], as_sub );
			}
			expression->result.list = sub[ 3 ].result.list;
			sub[ 3 ].result.list = NULL;
		} else {
			extract_sub_results( &sub[ 0 ], &sub[ 3 ], as_sub, results );
			expression->result.list = sub[ 0 ].result.list;
			sub[ 0 ].result.list = NULL;
		}
		goto RETURN_RESULTS;
	}

	// 2. check that we have all the information we need
	// -------------------------------------------------
#ifdef DEBUG
	output( Debug, "solver: 2. checking terms completion.." );
#endif
	FlagSub flag_sub[ 4 ];
	for ( int i=0; i<4; i++ ) {
		flag_sub[ i ].active = sub[ i ].result.active;
		flag_sub[ i ].inactive = sub[ i ].result.inactive;
		flag_sub[ i ].not = sub[ i ].result.not;
		flag_sub[ i ].any = sub[ i ].result.any;

		if ( sub[ i ].result.list != NULL )
			continue;

		// here we don't have any result for sub i
		if ( flag_sub[ i ].not ) {
			// not nothing is anything
			flag_sub[ i ].not = 0;
			flag_sub[ i ].any = 1;
		}
		else if ( !flag_sub[ i ].any ) {
#ifdef DEBUG
			output( Debug, "solver: returning on count %d", i );
#endif
			return 0;
		}
	}

	// 3. other special case with respect to the '~' and '.' flags
	// -----------------------------------------------------------
	if (( results == NULL ) &&
	    ( flag_sub[ 0 ].any || flag_sub[ 0 ].not ) &&
	    ( flag_sub[ 1 ].any || flag_sub[ 1 ].not ) &&
	    ( flag_sub[ 2 ].any || flag_sub[ 2 ].not ) &&
	    ( flag_sub[ 3 ].any || flag_sub[ 3 ].not ))
	{
		// in this case we would not be able to get a first candidate, so we must perform
		// the negation or extraction over the whole database.
		int count = flag_sub[ 0 ].not ? 0 :
			    flag_sub[ 1 ].not ? 1 :
			    flag_sub[ 2 ].not ? 2 :
			    flag_sub[ 3 ].not ? 3 : 4;
		if ( count < 4 )
		{
#ifdef DEBUG
			output( Debug, "solver: 3.1. special case - inverting all %d", count );
#endif
			int sub_count = ( count < 3 ) ? ( 1 << count ) : as_sub;
			invert_results( &sub[ count ], sub_count, NULL );
			flag_sub[ count ].not = 0;
		}
		else	// special cases: [ .-.->. : . ], [ ... ], .-.->[ ..? ] etc.
		{
#ifdef DEBUG
			output( Debug, "solver: 3.2. special cases - taking all" );
#endif
			count = ( expression->result.mark == 1 ) ? 0 :
				( expression->result.mark == 2 ) ? 1 :
				( expression->result.mark == 4 ) ? 2 : 3;

			int sub_count = ( count < 3 ) ? ( 1 << count ) : as_sub;
			listItem *list = take_all( expression, as_sub, NULL );
			sub[ count ].result.list = list;
		}
	}

	// 4. generate own list of results
	// -------------------------------
	listItem *sub_list[4], *candidate;

#ifdef DEBUG
	output( Debug, "solver: 4. searching..." );
#endif
	// initiate loop
	void *r_sub[ 4 ];
	for ( int i=0; i<4; i++ ) {
		r_sub[ i ] = sub_init( sub_list, i, expression );
	}
	DEBUG_2;
	candidate = first_candidate( sub_list, flag_sub, expression, results );
	if ( candidate == NULL ) {
#ifdef DEBUG
		output( Debug, "NULL candidate" );
#endif
		return 0;
	}
#ifdef DEBUG
	output( Debug, " - %0x", (int) candidate->ptr );
#endif
	// execute loop
	for ( ; ; ) {

		CandidateSub c_sub[ 4 ];

		int take = set_candidate_sub( c_sub, as_sub, candidate );
		if ( flag_sub[ 0 ].any && flag_sub[ 1 ].any && flag_sub[ 2 ].any &&
		     ( c_sub[ 0 ].ptr == NULL )) // e.g. expression [ ...: a ]
		{
			take = 0;
		}
		for ( int i=0; take && ( i < 4 ); i++ )
		{
			if ( ( flag_sub[ i ].active && !c_sub[ i ].active ) ||
			     ( flag_sub[ i ].inactive && c_sub[ i ].active ) ||
			     ( flag_sub[ i ].any ? 0 : flag_sub[ i ].not ?
				( c_sub[ i ].ptr == r_sub[ i ] ) : ( c_sub[ i ].ptr != r_sub[i] ) )
			     )
			{
				take = 0;
			}
		}
		if ( !take ) {
#ifdef DEBUG
			output( Debug, "solver: wrong candidate" );
#endif
			; // wrong candidate
		}
		else {
#ifdef DEBUG
			output( Debug, "solver: good one - calling addIfNotThere" );
#endif
			addIfNotThere( &expression->result.list, candidate->ptr ); // eliminates doublons
		}

		if ( candidate->next != NULL ) {
			candidate = candidate->next;
		}
		else do {
			if (( r_sub[3] = sub_next( sub_list, 3 ) )) {
				;
			} else if (( r_sub[2] = sub_next( sub_list, 2 ) )) {
				r_sub[3] = sub_init( sub_list, 3, expression );
			} else if (( r_sub[1] = sub_next( sub_list, 1 ) )) {
				r_sub[2] = sub_init( sub_list, 2, expression );
				r_sub[3] = sub_init( sub_list, 3, expression );
			} else if (( r_sub[0] = sub_next( sub_list, 0 ) )) {
				r_sub[1] = sub_init( sub_list, 1, expression );
				r_sub[2] = sub_init( sub_list, 2, expression );
				r_sub[3] = sub_init( sub_list, 3, expression );
			}
			else {
#ifdef DEBUG
				output( Debug, "solver: returning" );
#endif
				goto RETURN_RESULTS;
			}
			candidate = first_candidate( sub_list, flag_sub, expression, results );
		}
		while( candidate == NULL );
	}

RETURN_RESULTS:
	take_sup( expression );
	if ( restore_mode != context->expression.mode ) {
		restore( expression, restore_mode, restore_results, context );
	}
	return ( expression->result.list == NULL ) ? 0 : 1;
}

/*---------------------------------------------------------------------------
	resolve
---------------------------------------------------------------------------*/
/*
	First, solve solves expression, creating intermediate list of results
	at each node of the expression graph.
	Then resolves finds and returns the results from the node marked '?'
*/
static void
extract_final_results( listItem **list, Expression *expression, int count, listItem *results )
{
	if ( CN.context->expression.mode == ReadMode )
	{
		ExpressionSub *s;
		s_loop( count, results, &s ) {
			addItem( list, s );
		}
	}
	else	// not in ReadMode
	{
		Entity *e;
		e_loop( count, results, &e ) {
			addIfNotThere( list, e );
		}
	}
}

static int
resolve( Expression *expression )
{
	_context *context = CN.context;
	if ( expression->result.list == NULL )
		return 0;

#ifdef DEBUG
	output( Debug, "resolve: entering" );
#endif

	int mark = expression->result.mark;
	if ( mark )
	{
#ifdef DEBUG
		output( Debug, "resolve: found mark=%d", mark );
#endif
		listItem *results = NULL;
		if ( mark != 8 ) {
			for ( int count=0; count<3; count++ )
			{
				if ( !( mark & ( 1 << count )) )
					continue;

				extract_final_results( &results, expression, count, expression->result.list );
			}
			if ( context->expression.mode == ReadMode ) {
				rebuild_filtered_results( &results, context );
			}
		}
		if ( mark & 8 ) {
			if ( results == NULL ) {
				return 1;
			} else {
				catListItem( results, expression->result.list );
			}
		} else {
			if ( context->expression.mode == ReadMode ) {
				free_filter_results( expression, context );
			}
			freeListItem( &expression->result.list );
		}
		expression->result.list = results;
		return 1;
	}

	// here we haven't found the mark yet
	ExpressionSub *sub = expression->sub;
	for ( int count = 0; count<4; count++ )
	{
		if ( !sub[ count ].result.resolve )
			continue;

		Expression *e = sub[ count ].e;
		if (( count == 3 ) || sub[ 1 ].result.none )
		{
			e->result.list = expression->result.list;
			expression->result.list = NULL;
		}
		else
		{
			listItem *list = NULL;
			// extract subs and transport results up to the next level
			extract_final_results( &list, expression, count, expression->result.list );
			if ( context->expression.mode == ReadMode ) {
				rebuild_filtered_results( &list, context );
			}
			freeListItem( &e->result.list );
			e->result.list = list;
		}

		if ( resolve( e ) ) {
			// transport the results all the way back to base
			if ( context->expression.mode == ReadMode ) {
				free_filter_results( expression, context );
			}
			freeListItem( &expression->result.list );
			expression->result.list = e->result.list;
			e->result.list = NULL;
			return 1;
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------
	expression_solve
---------------------------------------------------------------------------*/
static void
setup_results( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		freeListItem( &sub[ i ].result.list );
		if ( sub[ i ].result.identifier.type == VariableIdentifier )
			continue;
		if ( !sub[ i ].result.any )
			setup_results( sub[ i ].e );
	}
}

static void
cleanup_results( Expression *expression )
{
	if ( expression == NULL ) return;
	ExpressionSub *sub = expression->sub;
	for ( int i=0; i<4; i++ )
	{
		if ( sub[ i ].result.identifier.type == VariableIdentifier )
			continue;
		if ( !sub[ i ].result.any )
			cleanup_results( sub[ i ].e );
	}
	freeListItem( &expression->result.list );
}

int
expression_solve( Expression *expression, int as_sub, _context *context )
{
#ifdef DEBUG
	output( Debug, "entering expression_solve: ReadMode=%d, expression=%0x...",
		(context->expression.mode == ReadMode), (int) expression );
#endif
	freeListItem( &context->expression.results );
	if ( expression == NULL ) RETURN( 0 );

	// set traversal to ReadMode if filter is Literal Variable
	// -------------------------------------------------------

	int restore_mode = context->expression.mode;
	if ( context->expression.filter == NULL ) {
		if (( restore_mode == ReadMode ) && ( context->expression.stack == NULL )) RETURN( 0 );
		restore_mode = set_filter( expression, context );
#ifdef DEBUG
		if ( restore_mode == ReadMode ) {
			output( Debug, "expression_solve: set_filter() - just switched to ReadMode" );
		}
#endif
	}
	if (( context->expression.mode == ErrorMode ) ||
	    (( context->expression.mode == InstantiateMode ) && !instantiable( expression, context ) ))
		RETURN( -1 );

	// in case of embedded expressions: ward against infinite recursion
	// ----------------------------------------------------------------

	for ( listItem *i = context->expression.stack; i!=NULL; i=i->next ) {
		if ( expression == i->ptr ) {
			return output( Error, "recursion in expression" );
		}
	}
	addItem( &context->expression.stack, expression );

	// extract general expression results - i.e. as if no question mark
	// ----------------------------------------------------------------

	int success, do_resolve;
	setup_results( expression );
	if (( expression->sub[ 0 ].result.any || expression->sub[ 0 ].result.none ) &&
	    ( expression->sub[ 1 ].result.any || expression->sub[ 1 ].result.none ) &&
	    ( expression->sub[ 2 ].result.any || expression->sub[ 2 ].result.none ) &&
	    ( expression->sub[ 3 ].result.any || expression->sub[ 3 ].result.none ) )
	{
#ifdef DEBUG
		output( Debug, "going to take_all - as_sub: %d", as_sub );
#endif
		as_sub = expression->result.as_sub | ( 1  <<  as_sub );
		expression->result.list = take_all( expression, as_sub, context->expression.filter );
		int count =
			( expression->result.mark == 1 ) ? 0 :
			( expression->result.mark == 2 ) ? 1 :
			( expression->result.mark == 4 ) ? 2 : 3;

		success = ( expression->result.list != NULL );
		do_resolve = (( count == 3 ) && expression->result.marked );
	}
	else
	{
		success = solve( expression, as_sub, context->expression.filter );
		do_resolve = expression->result.marked;
	}

	// invoke resolve() to extract marked results
	// ------------------------------------------

	// Notat that resolve must be called even in TestMode due to embedded expressions

	int filter_on = ( context->expression.mode == ReadMode );
#ifdef DEBUG
	if ( filter_on && ( success > 0 )) output( Debug, "expression_solve: filter on" );
#endif
	if ( filter_on && ( success > 0 )) {
		success = rebuild_filtered_results( &expression->result.list, context );
	}

	if ( do_resolve  && ( success > 0 )) {
		success = resolve( expression );
		if ( filter_on && ( success <= 0 )) {
			free_filter_results( expression, context );
		}
	}
	if ( filter_on && ( success > 0 )) {
		success = remove_filter( expression, restore_mode, as_sub, context );
	}

	// pop expression from expression stack
	// ------------------------------------

	popListItem( &context->expression.stack );

	context->expression.results = expression->result.list;
	expression->result.list = NULL;
	cleanup_results( expression );

#ifdef DEBUG
	output( Debug, "exiting expression_solve - success=%d, mode=%d",
		success, (context->expression.mode == ReadMode) );
#endif
	return success;
}

