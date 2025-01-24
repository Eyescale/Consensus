#ifndef FPRINT_EXPR_H
#define FPRINT_EXPR_H

#define TAB( level ) \
	for ( int k=0; k<level; k++ ) putc( '\t', stream );

void fprint_expr( FILE *, char *, int, int );
void fprint_output( FILE *, char *, int );


#endif	// FPRINT_EXPR_H
