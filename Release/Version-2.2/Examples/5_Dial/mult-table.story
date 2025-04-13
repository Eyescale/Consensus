/*===========================================================================
|
|			mult-table story main
|
+==========================================================================*/
#define DEBUG
#include "dial.bm"
:
	on init
#		do solo
#		do : base : 10
		do : base : 16
		do : INIT
	else in : INIT
		do : p : 0
		do : q : 0
		in : base : 16
			do : dial : !! Dial((:0123456789ABCDEF:),OPT)
		else	do : dial : !! Dial((:0123456789:),OPT)
		do : MULT
	else in : MULT
		on : . // dial operation on next digit(s)
			do (( *dial, ... ),< MULT, *p, *q, 0 >)
			in solo
				do > "%s x %s = ":< *p, *q >
		else on ~( %%, MULT ) < ?:*dial
			do :< c, r >: %<(!,?:...)>
			do : OUTPUT
	else in : OUTPUT
		in : base : 16
			on : q : ?
				in ~.: solo
					do > " %s%s ":< ((*c:~0)?:' '), *r >
				do : MULT
				in %?: 0
					do >:
					in : p : F
						do exit
					else do : p : ( \
						(*p:0) ? 1 :
						(*p:1) ? 2 :
						(*p:2) ? 3 :
						(*p:3) ? 4 :
						(*p:4) ? 5 :
						(*p:5) ? 6 :
						(*p:6) ? 7 :
						(*p:7) ? 8 :
						(*p:8) ? 9 :
						(*p:9) ? A :
						(*p:A) ? B :
						(*p:B) ? C :
						(*p:C) ? D :
						(*p:D) ? E :
						(*p:E) ? F :)
			else
				in solo
					do > "%s%s\nPress enter_ ":< *c:~0, *r >
					do :<
				do : q : ( \
					(*q:0) ? 1 :
					(*q:1) ? 2 :
					(*q:2) ? 3 :
					(*q:3) ? 4 :
					(*q:4) ? 5 :
					(*q:5) ? 6 :
					(*q:6) ? 7 :
					(*q:7) ? 8 :
					(*q:8) ? 9 :
					(*q:9) ? A :
					(*q:A) ? B :
					(*q:B) ? C :
					(*q:C) ? D :
					(*q:D) ? E :
					(*q:E) ? F :
					(*q:F) ? 0 :)
		else // : base : ~16
			on : q : ?
				in ~.: solo
					do > " %s%s ":< ((*c:~0)?:' '), *r >
				do : MULT
				in %?: 0
					do >:
					in : p : 9
						do exit
					else do : p : ( \
						(*p:0) ? 1 :
						(*p:1) ? 2 :
						(*p:2) ? 3 :
						(*p:3) ? 4 :
						(*p:4) ? 5 :
						(*p:5) ? 6 :
						(*p:6) ? 7 :
						(*p:7) ? 8 :
						(*p:8) ? 9 :)
			else
				in solo
					do > "%s%s\nPress enter_ ":< *c:~0, *r >
					do :<
				do : q : ( \
					(*q:0) ? 1 :
					(*q:1) ? 2 :
					(*q:2) ? 3 :
					(*q:3) ? 4 :
					(*q:4) ? 5 :
					(*q:5) ? 6 :
					(*q:6) ? 7 :
					(*q:7) ? 8 :
					(*q:8) ? 9 :
					(*q:9) ? 0 :)

