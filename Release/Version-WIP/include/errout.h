#ifndef ERROUT_H
#define ERROUT_H

typedef enum {
	ErrOutNone = 0,
	CellLoad,
	CellInform,
	ContextLoad,
	ContextMarkType,
	ContextEENOSelfAssignment,
	ContextTagUnknown,
	ContextLocaleMultiple,
	ContextLocaleConflict,
	DBConcurrentAssignment,
	DeternarizeMemoryLeak,
	EENOQueryFiltered,
	ExpressionTagUnknown,
	InstantiateFiltered,
	InstantiatePartial,
	InstantiateClassNotFound,
	InstantiateClassNotFoundv,
	InstantiateUBENotConnected,
	InstantiateInput,
	NarrativeOutputNone,
	OperationNotSupported,
	OperationProtoPassThrough,
	ProgramPrintout,
	ProgramStory,
	QueryScopeMemoryLeak,
	QueryExponentMemoryLeak,
	StoryLoad,
	StoryUnexpectedEOF
	} ErrOutType;

void * errout( ErrOutType, ... );


#endif // ERROUT_H
