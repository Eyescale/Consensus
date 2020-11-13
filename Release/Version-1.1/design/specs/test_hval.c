#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
   cf.	narrative.c: readNarrative()
	database.c: cn_out() 
	context.c: charscan(), bm_lookup(), bm_register()
	string_util.c: tokcmp(), strmake()

   Note that readNarrative() limits hexadecimal representations
   to upper cases, hence in charscan() we use a simplified HVAL
*/

int
is_separator( int event )
{
        return  ((( event > 96 ) && ( event < 123 )) ||         /* a-z */
                 (( event > 64 ) && ( event < 91 )) ||          /* A-Z */
                 (( event > 47 ) && ( event < 58 )) ||          /* 0-9 */
                  ( event == 95 )) ? 0 : 1;                     /* _ */
}

#define HVAL(c) (c-((c<58)?48:(c<97)?55:87))
int
main( int argc, char **argv )
{
	char lower[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	char upper[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
	for ( int i=0; i<16; i++ ) {
		for ( int j=0; j<16; j++ ) {
//			char c = HVAL(upper[i])*16 + HVAL(lower[j]);
			char c = HVAL(upper[i])<<4 | HVAL(lower[j]);
			if (( c=='*' ) || ( c=='%' ) || !is_separator(c) )
				printf( "  %c   ", c );
			else switch (c) {
			case '\0': printf( " '\\0' " ); break;
			case '\t': printf( " '\\t' " ); break;
			case '\n': printf( " '\\n' " ); break;
			case '\'': printf( " '\\'' " ); break;
			case '\\': printf( " '\\\\' " ); break;
			default:
				if (( c < 32 ) || ( c > 126 ))
				printf( "'\\x%.2X'", *(unsigned char *)&c );
				else printf( " '%c'  ", c );
			}
		}
		printf( "\n" );
	}
}

