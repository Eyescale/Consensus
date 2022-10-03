#ifndef TERNARIZE_H
#define TERNARIZE_H

listItem *ternarize( char * );

typedef int (*BMTernaryCB)( char *, void * );
char *deternarize( char *, BMTernaryCB pass_CB, void *user_data );

void free_ternarized( listItem *sequence );
void output_ternarized( listItem *sequence );


#endif	// TERNARIZE_H

