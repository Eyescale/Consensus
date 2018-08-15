#ifndef PARSER_H
#define PARSER_H

#include "pair.h"
#include "string_util.h"
#include "scheme.h"
#include "stream.h"

#define	P_DATA		1
#define	P_RESULTS	2
#define	P_BASE		4
#define P_SPACE		8
#define	P_TOK		16
#define	P_ALL		31

typedef enum {
	rvParserOutOfMemory = 1,
	rvParserNoMatch,
	rvParserEncore,
	rvParserNoMore
} ParserRetval;

typedef struct {
	Scheme *scheme;
	listItem *frames;
	listItem *results;
	listItem *tokens;
	int options;
} Parser;

int	ParserValidate( Scheme * );

Parser * newParser( Scheme *, int );
void	freeParser( Parser * );
ParserRetval ParserStatus( Parser * );
ParserRetval ParserOption( Parser *, int );
ParserRetval ParserRebase( Parser *, int p_flags );
ParserRetval ParserFrame( Parser *, int );

typedef listItem Sequence;


#endif // PARSER_H
