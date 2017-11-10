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
	ClauseActive,
	ClausePassive
}
ClauseState;

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
	FileScheme = 1,
	SessionScheme,
	OperatorScheme,
	CGIScheme,
}
SchemeType;

typedef enum {
	UserInput,	// default: read from stdin
	HCNFileInput,
	HCNValueFileInput,
	PipeInput,
	FileInput,
	ClientInput,
	SessionOutputPipe,
	SessionOutputPipeInVariator,
	SessionOutputPipeOut,
	InstructionBlock,
	InstructionOne,
	InstructionInline
}
InputType;

typedef enum {
	ClientOutput = 1,
	SessionOutput,
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
	StringIdentifier,
	NullIdentifier,
	VariableIdentifier,
	QueryIdentifier
}
IdentifierType;

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
	EvaluateMode = 1,
	ReadMode,
	InstantiateMode,
	ReleaseMode,
	ActivateMode,
	DeactivateMode,
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
	AssignSet,
	AssignAdd,
	AssignClient
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
		struct {
			struct {
				IdentifierType type;
				void *value;
			} identifier;
			unsigned int active : 1;
			unsigned int inactive : 1;
			unsigned int not : 1;
			unsigned int any : 1;
			unsigned int none : 1;
			unsigned int resolve : 1;
			unsigned int lookup : 1;
			listItem *list;
		}
		result;
		void *super;
	}
	sub[ 4 ];
	struct {
		int as_sub, as_sup, mark;
		unsigned int marked : 1;
		unsigned int output_swap : 1;
		listItem *list;
	}
	result;
}
Expression;
typedef struct _ExpressionSub ExpressionSub;

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
typedef struct {
	listItem *list;
	char *ptr;
}
IdentifierVA;

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
	IdentifierVA buffer;
	char *position;
	struct {
		SessionVA *created;
	} session;
	struct {
		char *position;
		IdentifierVA *buffer;
		struct {
			listItem *instructions;
			int mode;
		} record;
		unsigned int prompt : 1;
		unsigned int flush : 1;
	} restore;
	unsigned int shed : 1;
}
InputVA;

typedef struct {
	int level;
	int mode;
	struct {
		unsigned int redirected : 1;
		unsigned int query : 1;
	} restore;
	struct {
		char *identifier;
	} variable;
	struct {
		int socket;
	} ptr;
	int remainder;
}
OutputVA;

// Variables
// --------------------------------------------------
typedef struct VariableVA_ {
	VariableType type;
	void *value;
	Registry sub;
}
VariableVA;

// Stack
// --------------------------------------------------
typedef struct {
	char *next_state;	// state to be restored upon pop
	Registry variables;
	struct {
		ClauseState state;
	} clause;
	struct {
		int base;
		listItem *candidates;	// { entity }
		listItem *begin;	// { instruction }
	} loop;
	struct {
		Expression *ptr;
		struct {
			unsigned int not : 1;
			unsigned int active : 1;
			unsigned int inactive : 1;
		} flag;
	} expression;
	struct {
		struct {
			unsigned int action : 1;
			unsigned int closure : 1;
			unsigned int exit : 1;
			unsigned int event : 1;
			unsigned int condition : 1;
			unsigned int then : 1;
			unsigned int whole : 1;
			unsigned int otherwise : 1;
			unsigned int contrary : 1;
		}
		state;
		EventVA event;
		Occurrence *thread;
	} narrative;
}
StackVA;

// EntityLog
// --------------------------------------------------
typedef struct {
	listItem *instantiated;	// { entity }
	listItem *released;	// { entity-expression }
	listItem *activated;	// { entity }
	listItem *deactivated;	// { entity }
} EntityLog;

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
		struct {
			int level;
		} freeze;
		unsigned int one : 1;
		unsigned int block : 1;
		unsigned int contrary : 1;
	} command;
	struct {
		int mode;
	} assignment;
	struct {
		int level;
		IdentifierVA string;
		listItem *instructions;
		RecordMode mode;
	} record;
	struct {
		IdentifierVA id[ 4 ];
		char *scheme;
		char *path;
		int current;
	} identifier;
	struct {
		char *state;
		char *position;
		IdentifierVA buffer;
	} hcn;
	struct {
		int level;
		ExpressionMode mode;
		listItem *results;
		listItem *filter;
		listItem *args;
		Expression *ptr;
		char *scheme;
		listItem *stack;
		int marked;
		int do_resolve;
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
			struct {
				char *ptr;
			} identifier;
		} backup;
		Narrative *current;
		Occurrence *occurrence;	// current
		listItem *registered;
	} narrative;
	struct {
		struct {
			EntityLog cgi;
			struct {
				Registry activate;
				Registry deactivate;
			} narratives;
			struct {
				EntityLog buffer[ 2 ];
				EntityLog *backbuffer, *frontbuffer;
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
			IdentifierVA base, *current;
			char *position;
		} buffer;
		int eof;
	} input;
	struct {
		listItem *stack;	// current OutputVA
		AssignmentMode mode;
		listItem *slist;
		IdentifierVA string;
		unsigned int anteprompt : 1;
		unsigned int prompt : 1;
		unsigned int redirected : 1;
		unsigned int html : 1;
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
