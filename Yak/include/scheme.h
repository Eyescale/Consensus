#ifndef SCHEME_H
#define SCHEME_H

typedef struct {
	char *identifier;
	listItem *schemata;
} Rule;

typedef struct {
	listItem *rules;
	void *internals;
} Scheme;

typedef struct {
	char *string;
	void *data;
} Schema;

typedef struct {
	char *position;
	listItem *values;
} Token;

Scheme *readScheme( char *path );
void outputScheme( Scheme *, int tok );

Scheme *newScheme( void );
void freeScheme( Scheme * );
Rule *SchemeBase( Scheme * );
Rule *findRule( Scheme *, char * );
Rule *fetchRule( Scheme *, char * );
void addRule( Scheme *, Rule * );
void removeRule( Scheme *, Rule * );
listItem *findToken( Scheme *, char * );
listItem *addToken( Scheme *, char * );
void removeToken( Scheme *, listItem * );

Rule *newRule( char * );
void freeRule( Rule * );
Schema *findSchema( Rule *, char * );
void addSchema( Rule *, Schema * );
void removeSchema( Rule *, Schema * );

Schema *newSchema( char *, listItem * );
void freeSchema( Schema * );

#define	isRuleBase( r )	!r->identifier[ 0 ]


#endif	// SCHEME_H
