:
	on init
#		do solo
#		do : base : 10
#		do : base : 16
		do : state : INIT
	else in : state : INIT
		do : p : 0
		do : q : 0
		in : base : 16
			do : dial : !! Dial((:0123456789ABCDEF:),OPT)
		else	do : dial : !! Dial((:0123456789:),OPT)
		do : state : MULT
	else in : state : MULT
		on : %% : ? < *dial  // read operation's results
			do : c : %<?:(?,.)>
			do : r : %<?:(.,?)>
			do : state : OUTPUT
		else on : state : .  // dial operation on next digit(s)
			do : *dial : ((*p,*q),0)
			in solo
				do > "%s x %s = ":< *p, *q >
	else in : state : OUTPUT
		in : base : 16
			on : q : ?
				in ~.: solo
					do > " %s%s ":< ((*c:~0)?:' '), *r >
				do : state : MULT
				in %?: 0
					do >:
					in : p : F
						do exit
					else do : p : (
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
				do : q : (
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
				do : state : MULT
				in %?: 0
					do >:
					in : p : 9
						do exit
					else do : p : (
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
				do : q : (
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

: Dial
	in : state : MADD	// add q to (r,p) i times, via n
		on : n : ? 
			in %?: 0
				do : i : %(?,(*i,.))	// decrement i
			else
				do : n : %(?,(%?,.))	// decrement n
				in (*p,(?,.))		// increment p
					do : p : %?
				else
					do : r : %(*r,(?,.))	// increment report
					do : p : 0		// reset p to 0
		else on : i : ?
			in %?: 0
				in : c : 0
					do : .. : ( *r, *p )	// notify result
					do : state : ~.
				else 
					do : n : *c	// set n to carry
					do : i : 1	// last round
					do : c : 0
			else
				do : n : *q		// reset n

		else on : state : .
			do : p : *q			// set p to q
			do : i : %(?,(*p,.))		// set i to p-1
			do : n : *q			// set n
			do : r : 0

	else in : state : MSUB	// sub q from (r,p) i times, via n
		on : n : ?
			in %?: 0
				in (?,(*i,.))  // decrement i
					in %?: 0
						do : state : MADD
						do : i : 0
					else
						do : i : %?
						do : n : *q
			else
				do : n : %(?,(*n,.))	// decrement n
				in (?,(*p,.))
					do : p : %?	// decrement p
				else
					do : r : %(?,(*r,.))	// decrement report
					do : p : %(?,'\0')	// reset p to MAX
		else on : i : .
			do : n : *q

	else in : state : MAXX	// compute i x (base-1)
		on : i : ?
			in (%?,'\0')
				in ( X ) // here from OPT
					do : r : %(?,(*q,.))
					do : p : *r
					do : state : MSUB
					do : i : *t
				else
					do : r : %(?,(*p,.))
					do : p : *r
					do : state : MADD
					do : i : 0
			else 
				do : r : %(*r,(?,.))
				do : i : %(%?,(?,.))
		else on : state : .
			do : i : ( X ? *q : *p ) // may be here from OPT
			do : r : 1

	else in : state : MCEN	// compute (r,p)=q*(n=base/2)
		on : i : ?
			in %?: *q
				in : p : *n
					do : state : MADD
					do : i : 0 	// override default state init
					on : r : .
						do : p : 0
				else
					do : state : ( X ? MADD : MSUB )
					do : i : *t
					on : r : .
						do : p : 0
					else
						do : p : *n
			else
				do : i : %(%?,(?,.))	// increment i
				on : r : .		// increment report every 2nd time
				else
					do : r : %(*r,(?,.))
		else on : state : .
			do : i : 1

	else in : state : OPT  // optimize using (base-1) or (base/2) depending on n=neg(p)
		on : r : ?
			in ( X ? (%?:*X) : (%?:*p) ) // r reaches X=neg(p) resp. p before i reaches center
				in ( X )
					do >"_"
					do : state : MAXX
					do : t : %(?,(%?,.))	// t is MAXX's argument to MSUB
				else
					do >"|"
					do : state : MADD	// as is
			else in : i : *n  // i reaches center
				do >"%s": ( X ? '-' : '!' )
				do : state : MCEN
				do : t : %(?,(%?,.))	// t is MCEN's argument to MADD/MSUB
				do : r : 0		// r is MCEN's alternator
			else
				do : r : %(%?,(?,.))	// increment r
				do : i : %(*i,(?,.))	// increment i
				do : n : %(?,(*n,.))	// decrement n
		else on : i : ?
			in %?: *n
				do >"."
				do : state : MCEN
			else in %?: *p
				do : r : 1
			else in : n : *p
				do : X : %?
				do : r : 1
			else
				do : i : %(%?,(?,.))	// increment i
				do : n : %(?,(*n,.))	// decrement n
		else on : state : .
			do : i : %(1,(?,.))
			do : n : %(?,(.,'\0'))

	else in : state : INIT
		on : i : ?  // no OPT - we want q > p
			in (%?,'\0')		// p is greater than q
				do : p : *q
				do : q : *p
				do : state : MADD
			else in %? : *q		// q is greater than p
				do : state : MADD
			else do : i : %(%?,(?,.))
		else on : state : .
			in ((*p:0)?:(*q:0))
				in ( OPT ? (*q:~0:~1:~%(?,'\0')) :)
					do >" "
				do : .. : ( 0, *c ) // notify result
				do : state : ~.
			else in *p: %(?,'\0')
				in ( OPT ? (*q:~1:~%(?,'\0')) :)
					do >" "
				do : p : *q
				do : state : MAXX
			else in *q: %(?,'\0')
				do : state : MAXX
			else in *p: 1
				in ( OPT ? (*q:~1) :)
					do >" "
				do : p : *q
				do : state : MADD
				do : i : 0
			else in *q: 1
				do : state : MADD
				do : i : 0
			else in OPT
				do : state : OPT
			else
				do : i : %(*p,(?,.))
	else on : %% : ? < ..
		do : p : %<?:((?,.),.)>
		do : q : %<?:((.,?),.)>
		do : c : %<?:(.,?)>
		do : r : 0
		do : .. : ~.
		do : state : INIT
		in OPT
			do : t : 0
			do ~( X )
	else on exit < ..
		do exit

