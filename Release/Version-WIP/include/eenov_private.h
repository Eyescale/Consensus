#ifndef EENOV_PRIVATE_H
#define EENOV_PRIVATE_H

#include "context.h"

typedef struct {
	CNInstance *src; // proxy
	CNDB *db; // corresponding to src
	CNInstance *instance;
	union {
		struct { OutputData *od; } output;
		struct { BMContext *ctx; listItem *result; } inform;
		struct { BMContext *ctx; CNDB *db; } lookup;
		struct { CNInstance *x; CNDB *db; } match;
	} param;
	int success;
	CNInstance *result;
	struct { listItem *instance, *flags; } stack;
} EEnovData;
typedef enum {
	EEnovNone,
	EEvaType,
	EEnovSrcType,
	EEnovInstanceType,
	EEnovExprType
} EEnovType;
typedef enum {
	EEnovOutputOp,
	EEnovInformOp,
	EEnovLookupOp,
	EEnovMatchOp
} EEnovQueryOp;

//---------------------------------------------------------------------------
//	PUSH and POP
//---------------------------------------------------------------------------
// Assumption: eenov exponent is AS_SUB-only

#define PUSH( stack, exponent, POP ) \
	while (( exponent )) { \
		int exp = (int) exponent->ptr; \
		for ( j=e->as_sub[ exp & 1 ]; j!=NULL; j=j->next ) { \
			if ( !db_private( privy, j->ptr, db ) ) \
				break; } \
		if (( j )) { \
			addItem( &stack, i ); \
			addItem( &stack, exponent ); \
			exponent = exponent->next; \
			i = j; e = j->ptr; } \
		else goto POP; }

#define POP( stack, exponent, PUSH ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto PUSH; } } \
		else if (( stack )) { \
			POP_XPi( stack, exponent ) } \
		else break; } \
	break;

#define POP_XPi( stack, exponent ) { \
	exponent = popListItem( &stack ); \
	i = popListItem( &stack ); }

#define POP_ALL( stack, exponent ) \
	while (( stack )) POP_XPi( stack, exponent )

//---------------------------------------------------------------------------
//	LUSH and LOP	- PUSH and POP list
//---------------------------------------------------------------------------
#define LUSH( stack, LOP ) \
	addItem( &stack, i ); \
	for ( i=e->as_sub[0]; i!=NULL; i=i->next ) \
		if ( !db_private( privy, i->ptr, db ) ) \
			break; \
	if (( i )) { \
		addItem( &stack, i ); \
		e = ((CNInstance *) i->ptr )->sub[ 1 ]; } \
	else { \
		i = popListItem( &stack ); \
		goto LOP; }

#define LOP( stack, LUSH ) \
	for ( ; ; ) { \
		if (( i->next )) { \
			i = i->next; \
			if ( !db_private( privy, i->ptr, db ) ) { \
				e = i->ptr; \
				goto LUSH; } } \
		else if (( stack )) \
			i = popListItem( &stack ); \
		else break; }


#endif // EENOV_PRIVATE_H
