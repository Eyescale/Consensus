#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "database.h"
#include "errout.h"

#define YELLOW "\x1B[1;33m"
#define RESET "\x1B[0m"

#define HEAD ">>>>> B%%: error: "
#define BODY RESET "\t\t"
#define FOOT YELLOW "\t<<<<< "

#define _( msg ) \
	fprintf( stderr, msg ); break;
#define _arg( msg ) \
	fprintf( stderr, msg, va_arg(ap,char*) ); break;

void *
errout( ErrOutType type, ... ) {
	CNDB *db;
	CNInstance *e, *f, *g;
	char *p, *expression;
	void *address;
	fprintf( stderr, YELLOW );
	va_list ap;
	va_start( ap, type );
	switch ( type ) {
	//----------------------------------------------------------------------
	//	Formatted
	//----------------------------------------------------------------------
	case NarrativeOutputNone: _(
		"B%%: error: narrative_output: No narrative\n" )
	case OperationNotSupported: _(
		"B%%: error: operation not supported\n" )
	case ProgramStory: _(
		"B%%: error: story has no main\n" )
	case StoryUnexpectedEOF: _(
		"B%%: error: readStory(): unexpected EOF\n" )
	case CellLoad: _arg(
		"B%%: error: load init file: '%s' failed\n" )
	case ContextLoad: _arg(
		"B%%: error: no such file or directory: '%s'\n" )
	case ProgramLoad: _arg(
		"B%%: error: no such file or directory: '%s'\n" )
	case StoryLoad: _arg(
		"B%%: error: no such file or directory: '%s'\n" )
	case ContextMarkType: _(
		HEAD "extract_mark(): unknown mark type\n" )
	case QueryScopeMemoryLeak: _(
		HEAD "xp_verify: memory leak on scope\n" )
	case QueryExponentMemoryLeak: _(
		HEAD "xp_verify: memory leak on exponent\n" )
	case ContextEENOSelfAssignment: _arg(
		HEAD "Self-assignment\n"
		BODY "(%s\n"
		FOOT "not supported in EENO\n" )
	case ContextTagUnknown: _arg(
		HEAD "tag unknown in expression\n"
		BODY "_|^%s\n"
		FOOT "tag operation failed\n" )
	case ContextLocaleMultiple: _arg(
		HEAD "Locale declaration\n"
		BODY ".%s\n"
		FOOT "already declared\n" )
	case ContextLocaleConflict: _arg(
		HEAD "Locale declaration\n"
		BODY ".%s\n"
		FOOT "conflicts with sub-narrative .arg\n" )
	case EENOQueryFiltered: _arg(
		HEAD "EENO filtered in expression\n"
		BODY "on/per _%s\n"
		FOOT "filter ignored\n" )
	case ExpressionTagUnknown: _arg(
		HEAD "list identifier unknown in expression\n"
		BODY ".%%%s\n"
		FOOT "instruction ignored\n" )
	case InstantiateFiltered: _arg(
		HEAD "instantiation filtered in expression\n"
		BODY "do _%s\n"
		FOOT "filter ignored\n" )
	case InstantiatePartial: _arg(
		HEAD "unable to complete instantiation\n"
		BODY "do %s\n"
		FOOT "failed at least partially\n" )
	case InstantiateClassNotFound: _arg(
		HEAD "class not found in expression\n"
		BODY "do !! %s\n"
		FOOT "instantiation failed\n" )
	case InstantiateClassNotFoundv: _arg(
		HEAD "class not found in expression\n"
		BODY "do :_: !! %s\n"
		FOOT "assignment failed\n" );
	case InstantiateUBENotConnected: _arg(
		HEAD "UBE not connected in\n"
		BODY "do :_: !! | %s\n"
		FOOT "assignment failed\n" )
	case OperationProtoPassThrough: _arg(
		HEAD "do_enable():\n"
		BODY "proto %s resolves to %%(.,...)"
		FOOT "passes through!!\n" )
	//----------------------------------------------------------------------
	//	Unformatted
	//----------------------------------------------------------------------
	case CellInform:
		db = va_arg( ap, CNDB * );
		e = va_arg( ap, CNInstance * );
		db_outputf( stderr, db,
			HEAD "bm_inform() instance\n"
			BODY "%_\n"
			FOOT "transposition failed\n", e );
		break;
	case DBConcurrentAssignment:
		db = va_arg( ap, CNDB * );
		e = va_arg( ap, CNInstance * );
		f = va_arg( ap, CNInstance * );
		g = va_arg( ap, CNInstance * );
		db_outputf( stderr, db,
			HEAD "concurrent reassignment\n"
			BODY ":%_:%_->%_\n"
			FOOT "not authorized\n", e, f, g );
		break;
	case DeternarizeMemoryLeak:
		address = va_arg( ap, void * );
		expression = va_arg( ap, char * );
		p = va_arg( ap, char * );
		fprintf( stderr,
			HEAD "deternarize: Memory Leak: "
			"0x%x %s, at %s\n", (int)address, expression, p );
		break;
	case InstantiateInput:
		expression = va_arg( ap, char * );
		p = va_arg( ap, char * );
		fprintf( stderr,
			HEAD "input instantiation failed\n"
			FOOT "on arg=%s, input=%s\n", expression, p );
		break;
	default:
		break; }
	va_end( ap );
	fprintf( stderr, RESET );
	return NULL; }


