#ifndef KERNEL_H
#define KERNEL_H

#include "config.h"

/*---------------------------------------------------------------------------
	types
---------------------------------------------------------------------------*/

typedef enum {
	ExecutionMode = 1,
	InstructionMode,
	FreezeMode
}
CommandMode;

typedef enum {
	ClauseNone,
	PassiveClause,
	ActiveClause,
	PostActiveClause,
	ClauseElse
}
ClauseMode;

typedef enum {
	InstantiateEvent = 1,
	ReleaseEvent,
	ActivateEvent,
	DeactivateEvent,
	InitEvent,
	StreamEvent
}
EventType;

typedef enum {
	DefaultScheme = 0,
	FileScheme,
	SessionScheme,
	OperatorScheme,
	CGIScheme,
}
SchemeType;

typedef enum {
	UserInput,	// default: read from stdin
	HCNFileInput,
	HCNValueFileInput,
	UNIXPipeInput,
	FileInput,
	ClientInput,
	SessionOutputPipe,
	SessionOutputPipeToVariable,
	SessionOutputPipeOut,
	InstructionBlock,
	InstructionOne,
	InstructionInline
}
InputType;

typedef enum {
	ClientOutput = 1,
	SessionOutput,
	FileOutput,
	StringOutput
}
OutputType;

typedef enum {
	Text = 1,
	Error,
	Warning,
	Enquiry,
	Info,
	Debug
}
OutputContentsType;

typedef enum {
	OffRecordMode,	// default: do not record input
	OnRecordMode,
	RecordInstructionMode
}
RecordMode;

typedef enum {
	DefaultIdentifier = 1,
	NullIdentifier,
	VariableIdentifier,
	ExpressionValue,
	NarrativeIdentifiers,
	ExpressionCollect,
	LiteralResults,
	EntityResults,
	StringResults
}
ValueType;

typedef enum {
	EntityVariable = 1,
	ExpressionVariable,
	LiteralVariable,
	StringVariable,
	NarrativeVariable,
}
VariableType;

typedef enum {
	VariableFilter = 1,
	ExpressionFilter,
	ListFilter
}
FilterType;

typedef enum {
	InstantiateOperation = 1,
	ReleaseOperation,
	ActivateOperation,
	DeactivateOperation,
}
OperationType;

typedef enum {
	BuildMode = 1,
	ReadMode,
	EvaluateMode,
	InstantiateMode,
	ErrorMode
}
ExpressionMode;

typedef enum {
	OtherwiseOccurrence = 1,
	ConditionOccurrence,
	EventOccurrence,
	ActionOccurrence,
	ThenOccurrence
}
OccurrenceType;

typedef enum {
	AssignSet = 1,
	AssignAdd
}
AssignmentMode;

/*---------------------------------------------------------------------------
	structures
---------------------------------------------------------------------------*/

// Session
// --------------------------------------------------
typedef struct {
	char *path;
	pid_t pid;
	int genitor; // socket connection (transient)
	pid_t operator;
}
SessionVA;

// Expression
// --------------------------------------------------
typedef struct _Expression {
	struct _ExpressionSub {
		struct _Expression *e;
		struct _ExpressionSub *super;
		struct {
			struct {
				ValueType type;
				void *value;
			} identifier;
			listItem *list;
			unsigned int active : 1;
			unsigned int passive : 1;
			unsigned int not : 1;
			unsigned int any : 1;
			unsigned int none : 1;
			unsigned int resolve : 1;
		}
		result;
	}
	sub[ 4 ];
	struct {
		int twist;
		int as_sub, as_sup, mark;
		listItem *list;
		unsigned int marked : 1;
	}
	result;
}
Expression;
typedef struct _ExpressionSub ExpressionSub;
typedef ExpressionSub Literal;

// Occurrence
// --------------------------------------------------
typedef struct _Occurrence {
	struct _Occurrence *thread;
	OccurrenceType type;
	Registry variables;
	unsigned int registered;
#ifdef DO_LATER
	char *format;
#else
	unsigned int contrary : 1;
#endif
	listItem *va;
	int va_num;
	struct {
		listItem *n;
		int num;
	}
	sub;
}
Occurrence;

typedef struct {
	Occurrence *occurrence;
}
OtherwiseVA;

typedef struct _ConditionVA {
	char *identifier;
	void *format;
}
ConditionVA;

typedef struct _EventVA {
	char *identifier;
	struct {
		SchemeType scheme;
		char *path;
	} source;
	void *format;	// expression or path
	EventType type;
}
EventVA;

typedef struct {
	unsigned int exit : 1;
	listItem *instructions;
}
ActionVA;

// Narrative
// --------------------------------------------------
typedef struct {
	char *name;
	Occurrence root;
	Registry variables;
	struct {
		listItem *events;
		listItem *actions;
		listItem *then;
	}
	frame;
	Registry entities;	// { ( entity, narrative-pseudo ) }
	Registry instances;	// { ( entity, narrative-instance ) }
	unsigned int deactivate : 1;
	unsigned int assigned : 1;
}
Narrative;

// Input & Output
// --------------------------------------------------
typedef registryEntry StringVA;

typedef struct {
	char *identifier;
	int level;
	InputType mode;
	union {
		FILE *file;
		char *string;
		int fd;
	} ptr;
	int client;
	int remainder;
	StringVA buffer;
	char *position;
	struct {
		SessionVA *created;
	} session;
	struct {
		char *identifier;
	} variable;
	struct {
		struct {
			listItem *instructions;
			int mode;
		} record;
		char *position;
		listItem *instruction;
		StringVA *buffer;
		unsigned int eof : 1;
		unsigned int prompt : 1;
		unsigned int flush : 1;
		unsigned int command_one : 1;
	} restore;
	unsigned int discard : 1;
}
InputVA;

typedef struct {
	int level;
	int mode;
	struct {
		char *identifier;
	} variable;
	union {
		int socket;
		FILE *file;
	} ptr;
	int remainder;
	struct {
		unsigned int redirected : 1;
		unsigned int query : 1;
		unsigned int hcn : 1;
		unsigned int cgi : 1;
	} restore;
}
OutputVA;

typedef enum {
	TXT_LINE = 1,
	TXT_SEGMENT,
	TXT_OPTION
}
OutputOperation;

// Variables
// --------------------------------------------------
typedef struct VariableVA_ {
	VariableType type;
	void *value;
}
VariableVA;

// Stack
// --------------------------------------------------
typedef struct {
	char *next_state;	// state to be restored upon pop
	Registry variables;
	struct {
		ClauseMode mode;
	} clause;
	struct {
		int base;
		listItem *candidates;	// { entity }
		listItem *begin;	// { instruction }
	} loop;
	struct {
		Expression *ptr;
		listItem *collect;
		struct {
			unsigned int not : 1;
			unsigned int active : 1;
			unsigned int passive : 1;
		} flag;
	} expression;
	struct {
		struct {
			unsigned int condition : 1;
			unsigned int event : 1;
			unsigned int action : 1;
			unsigned int otherwise : 1;
			unsigned int then : 1;
			unsigned int whole : 1;
			unsigned int closure : 1;
			unsigned int exit : 1;
			unsigned int contrary : 1;
		}
		state;
		EventVA event;
		Occurrence *thread;
	} narrative;
}
StackVA;

// CNLog
// --------------------------------------------------
typedef struct {
	listItem *instantiated;	// { entity }
	listItem *released;	// { literal }
	listItem *activated;	// { entity }
	listItem *deactivated;	// { entity }
} CNLog;

// Context
// --------------------------------------------------
typedef struct {
	struct {
		unsigned int terminal : 1;
		unsigned int cgi : 1;
		unsigned int cgim : 1;	// cgi emulator mode
		unsigned int operator : 1;
		unsigned int operator_absent : 1;
		unsigned int session : 1;
		unsigned int stop : 1;
	} control;
	struct {
		Registry *sessions, *pid;
	} operator;
	struct {
		char *path;
	} session;
	struct {
		CommandMode mode;	// ExecutionMode or InstructionMode or FreezeMode
		listItem *stack;	// current StackVA
		StackVA stackbase;
		int level;
		OperationType op;
		struct {
			int level;
		} freeze;
		unsigned int one : 1;
		unsigned int block : 1;
	} command;
	struct {
		ClauseMode mode;
		unsigned int contrary : 1;
	} clause;
	struct {
		int mode;
	} assignment;
	struct {
		int level;
		StringVA string;
		listItem *instructions;
		RecordMode mode;
	} record;
	struct {
		StringVA id[ 4 ];
		char *scheme;
		char *path;
		int current;
	} identifier;
	struct {
		char *state;
		char *position;
		StringVA buffer;
	} hcn;
	struct {
		int level;
		ExpressionMode mode;
		listItem *results;
		listItem *args;
		Expression *ptr;
		char *scheme;
		listItem *stack;
		listItem *backup;
		listItem *filter;
		unsigned int marked : 1;
	} expression;
	struct {
		int level;
		struct {
			unsigned int editing : 1;
			unsigned int output : 1;
		} mode;
		struct {
			struct {
				listItem *results;
			} expression;
			char *identifier;
		} backup;
		Narrative *current;
		Occurrence *occurrence;	// current
		listItem *registered;
	} narrative;
	struct {
		struct {
			CNLog cgi;
			struct {
				Registry activate;
				Registry deactivate;
			} narratives;
			struct {
				CNLog buffer[ 2 ];
				CNLog *backbuffer, *frontbuffer;
			} entities;
		} log;
		int backlog;
		unsigned int updating : 1;
	} frame;
	struct {
		int level;
		listItem *stack;	// current InputVA
		listItem *instruction;
		struct {
			StringVA base, *ptr;
			char *position;
		} buffer;
		unsigned int eof : 1;
		unsigned int hcn : 1;
	} input;
	struct {
		listItem *stack;	// current OutputVA
		AssignmentMode mode;
		listItem *slist;
		StringVA string;	// buffer for slist
		unsigned int anteprompt : 1;
		unsigned int prompt : 1;
		unsigned int redirected : 1;
		unsigned int cgi : 1;
		unsigned int as_is : 1;
		unsigned int marked : 1;
		unsigned int query : 1;
	} output;
	struct {
		unsigned int tell : 2;
		unsigned int flush_input : 1;
		unsigned int flush_output : 1;
	} error;
	struct {
		int bulletin, cgiport, service, client;	// sockets
		struct {
			struct {
				char ptr[ IO_BUFFER_MAX_SIZE ];
			} buffer;
		} input, output;
		struct {
			listItem *results;
			unsigned int sync : 1;
			unsigned int leave_open : 1;
		} query;
		struct {
			unsigned int sync : 1;
		} restore;
	} io;
}
_context;

// CN
// --------------------------------------------------

#ifndef KERNEL_C
extern
#endif
	struct {
		_context *context;
		Entity *this, *nil;
		listItem *DB;
		Registry *VB;		// registry of value accounts
		Registry *names;	// base entity names
	} CN;

/*---------------------------------------------------------------------------
	state_machine C code utilities
---------------------------------------------------------------------------*/

#define bgn_		if ( 0 ) {
#define in_( s )	} else if ( !strcmp( state, s ) ) {
#define in_any		} else {
#define in_other	} else {
#define on_( e )	} else if ( event == e ) {
#define	on_separator	} else if ( is_separator( event ) ) {
#define on_any		} else {
#define on_other	} else {
#define end		}

/*---------------------------------------------------------------------------
	state machine specific actions, states and functions
---------------------------------------------------------------------------*/

#ifdef KERNEL_C
char *base = "base";
char *same = "same";
char *out = "out";
char *pop_state = "pop";
char *variator_symbol = "?";
char *this_symbol = "!";
#else
extern char *base;
extern char *same;
extern char *out;
extern char *pop_state;
extern char *this_symbol;
extern char *variator_symbol;
#endif

/*---------------------------------------------------------------------------
	kernel actions	- public
---------------------------------------------------------------------------*/

typedef int _action( char *state, int event, char **next_state, _context *context );

_action	nothing;
_action	nop;
_action	push;
_action	pop;

/*---------------------------------------------------------------------------
	kernel utilities	- public
---------------------------------------------------------------------------*/

int ttyu( _context *context );

void set_command_mode( CommandMode mode, _context *context );
int command_mode( int freeze, int instruct, int execute );


#endif	// KERNEL_H
