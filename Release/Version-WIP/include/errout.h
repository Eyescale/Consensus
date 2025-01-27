#ifndef ERROUT_H
#define ERROUT_H

typedef enum {
	ErrOutNone = 0,
	BMMemoryLeak,
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
	EnPostFrame,
	ExpressionTagUnknown,
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

int errout( ErrOutType, ... );


#endif // ERROUT_H
