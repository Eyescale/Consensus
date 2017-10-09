#ifndef KERNEL_H
#define KERNEL_H

#define IO_BUFFER_MAX_SIZE 4

/*---------------------------------------------------------------------------
	types
---------------------------------------------------------------------------*/

typedef enum {
	ExecutionMode = 1,
	InstructionMode,
	FreezeMode
}
ControlMode;

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
	DeactivateEvent
}
EventType;

typedef enum {
	UserInput,	// default: read from stdin
	HCNFileInput,
	PipeInput,
	StreamInput,
	ClientInput,
	InstructionBlock,
	InstructionOne,
	LastInstruction
}
InputType;

typedef enum {
	ClientOutput = 1,
	IdentifierOutput
}
OutputType;

typedef enum {
	Text = 1,
	Error,
	Warning,
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
	registryEntry *variables;
	unsigned int registered;
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
	struct {
		IdentifierType type;
		char *name;
	}
	identifier;
	struct {
		unsigned int stream: 1;
		unsigned int instantiate : 1;
		unsigned int release : 1;
		unsigned int activate : 1;
		unsigned int deactivate : 1;
		unsigned int init : 1;
		unsigned int notification : 1;
		unsigned int request : 1;
	} type;
	void *format;
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
	registryEntry *variables;
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
	unsigned int initialized : 1;
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
	int corrupted;
	union {
		FILE *file;
		char *string;
		int fd;
	} ptr;
	int client;
	char *position;
	int remainder;
	struct {
		unsigned int prompt : 1;
	} restore;
}
InputVA;

typedef struct {
	int level;
	int mode;
	union {
		int socket;
		IdentifierVA *identifier;
	} ptr;
	int remainder;
	struct {
		unsigned int redirected : 1;
	} restore;
}
OutputVA;

// Variables
// --------------------------------------------------
typedef struct {
	VariableType type;
	struct {
		int ref;
		void *value;
	}
	data;
}
VariableVA;

// Stack
// --------------------------------------------------
typedef struct {
	char *next_state;	// state to be restored upon pop
	registryEntry *variables;
	struct {
		ClauseState state;
	} clause;
	struct {
		int base;
		listItem *index;	// { entity }
		listItem *begin;	// { instruction }
	} loop;
	struct {
		Expression *ptr;
		struct {
			unsigned int not : 1;
			unsigned int active : 1;
			unsigned int inactive : 1;
		} flags;
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
		}
		state;
		EventVA event;
		Occurrence *thread;
	} narrative;
}
StackVA;

// FrameLog
// --------------------------------------------------
typedef struct {
	struct {
		listItem *instantiated;	// { entity }
		listItem *released;	// { entity-expression }
		listItem *activated;	// { entity }
		listItem *deactivated;	// { entity }
	} entities;
	struct {
		listItem *instantiated;	// { entity }
	} streams;
	struct {
		Registry activate;	// { ( narrative, { entity } ) }
		Registry deactivate;	// { ( narrative, { entity } ) }
	} narratives;
}
FrameLog;

// Context
// --------------------------------------------------
typedef struct {
	struct {
		ControlMode mode;	// ExecutionMode or InstructionMode or FreezeMode
		listItem *stack;	// current StackVA
		int level;
		unsigned int terminal : 1;
		unsigned int cgi : 1;
		unsigned int prompt : 1;
		unsigned int contrary : 1;
		unsigned int stop : 1;
	} control;
	struct {
		int mode;
	} assignment;
	struct {
		int level;
	} freeze;
	struct {
		int level;
		IdentifierVA string;
		listItem *instructions;
		RecordMode mode;
	} record;
	struct {
		IdentifierVA id[ 3 ];
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
		listItem *stack;
		int marked;
		int do_resolve;
	} expression;
	struct {
		int level;
		struct {
			unsigned int condition : 1;
			unsigned int event : 1;
			struct {
				unsigned int one : 1;
				unsigned int block : 1;
			} action;
			unsigned int output : 1;
			unsigned int script : 1;
		} mode;
		struct {
			struct {
				listItem *results;
			} expression;
			struct {
				char *ptr;
			} identifier;
		} backup;
		Occurrence *action;
		Narrative *current;
		listItem *registered;
	} narrative;
	struct {
		FrameLog log;
		int backlog;
	} frame;
	struct {
		listItem *stack;	// current InputVA
		listItem *instruction;
		int event, buffer;
	} input;
	struct {
		listItem *stack;	// current OutputVA
		AssignmentMode mode;
		int redirected;
		struct {
			char *session;
			char *entity;
		} target;
	} output;
	struct {
		unsigned int flush_input;
		unsigned int flush_output;
	} error;
	struct {
		int operator, service, client;	// sockets
		struct {
			FrameLog log;
			struct {
				char ptr[ IO_BUFFER_MAX_SIZE ];
			} buffer;
		} input;
		struct {
			FrameLog log;
			struct {
				char ptr[ IO_BUFFER_MAX_SIZE ];
			} buffer;
		} output;
		struct {
			struct {
				char ptr[ IO_BUFFER_MAX_SIZE ];
			} buffer;
		} pipe;
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
		Registry VB;		// registry of value accounts
		Registry registry;	// registry of named entities
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
_action	error;
_action	warning;

/*---------------------------------------------------------------------------
	kernel utilities	- public
---------------------------------------------------------------------------*/

void set_control_mode( ControlMode mode, int event, _context *context );
int context_check( int freeze, int instruct, int execute );


#endif	// KERNEL_H
