#ifndef TERNARIZE_H
#define TERNARIZE_H

listItem *ternarize( char * );

typedef int (*BMTernaryCB)( char *, void * );
char *deternarize( char *, BMTernaryCB pass_CB, void *user_data );

void free_ternarized( listItem *sequence );
void output_ternarized( listItem *sequence );


#define	FIRST		1
#define	LEVEL		2
#define	SUB_EXPR	4
#define	INFORMED	8
#define	TERNARY		16
#define	FILTERED	32


#endif	// TERNARIZE_H

