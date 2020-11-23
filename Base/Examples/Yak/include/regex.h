#ifndef REGEX_H
#define REGEX_H

#include "pair.h"
#include "string_util.h"
#include "stream.h"

#define R_RESULTS	1
#define R_BASE		2
#define R_ALL		3

typedef enum {
	rvRegexOutOfMemory = 1,
	rvRegexNoMatch,
	rvRegexEncore,
	rvRegexNoMore
} RegexRetval;

typedef listItem Sequence;
typedef struct {
	listItem *groups;
	listItem *frames;
	Sequence *results;
} Regex;


Regex *newRegex( char *format, int *passthrough );
void freeRegex( Regex * );
RegexRetval RegexStatus( Regex * );
RegexRetval RegexRebase( Regex *, int r_flags );
RegexRetval RegexFrame( Regex *, int event, int capture );
void RegexSeqFree( Sequence **results );
listItem *RegexExpand( Regex *, listItem *strings, listItem * );


#endif // REGEX_H
