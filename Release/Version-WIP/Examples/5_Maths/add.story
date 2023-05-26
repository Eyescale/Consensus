:
/*
	Uses pre-instantiated literal - aka. dial - e.g.
		(:0123456789:)	==	0 --+---v
					    1 --+---v
						2 --+---v
						    3 --+---v
							... +---v
							    9 --+--\0
	to compute the sum Z of the two numbers X and Y
*/
	on init
		do : dial : !! Dial((:0123456789:))
#		do : X : ((X,...):123:)	// no carry
		do : X : ((X,...):189:)
		do : Y : ((Y,...):13234:)
		do : Z : ((Z,...)::)
		do : INIT
	else in : INIT
		do : p : *X
		do : q : *Y
		do : r : (r,*)
		do : carry : 0
		do : ADD
	else in : ADD
		on : %% : ? < *dial  // read operation's results
			in %<?:'\0'>
				do >"[1] "
				do : carry : 1
			else
				do >"%_\n": %<?>
				do : r : ( *r, %<?> )
				in : p : (?,.)
					do : p : ( %?:(.,*) ? ~. : %? )
				in : q : (?,.)
					do : q : ( %?:(.,*) ? ~. : %? )
				do : ADD
		else on : .  // dial operation on next digit(s)
			in : p : (.,?)
				do : *dial : ((%?,(*q?%(*q:(.,?)):0)),*carry)
			else in : q : (.,?)
				do : *dial : ((%?,0),*carry)
			else
				do : r : ( *carry:1 ? (*r,1) : *r )
				do : FORMAT
			do : carry : 0
	else in : FORMAT
		on : r : (.,?)	// inform Z from r
			in %?: ~*
				do : Z : ( *Z, %? )
				do : r : %(*r:(?,.))
			else
				do ~( *r )
				do : OUTPUT
	else in : OUTPUT
		on : p : (.,?)
			do > "%s" : %?
			in ?: ( *p, . )
				do : p : %?
			else in : q : X
				do > " + "
				do : p : %((Y,*), . )
				do : q : Y
			else in : q : Y
				do > " = "
				do : p : %((Z,*), . )
				do : q : Z
			else
				do > "\n~~~~~~~~~~~~~~~~\n"
				do exit
		else on : .
			do > "~~~~~~~~~~~~~~~~\n"
			do : p : %((X,*), . )
			do : q : X

: Dial
/*
	compute p + q + carry (digits) on demand
*/
	on : q : ?
		in %?: 0
			in : carry : 0
				do : .. : *p	// notify result
			else
				do : carry : 0
				do : q : 1
		else in (*p,(?,.))
			do : p : %?		// increment p
			do : q : %(?,(*q,.))	// decrement q
		else
			do : .. : '\0'		// notify carry
	else on : .. : '\0'
		do : p : 0			// reset p
		do : q : %(?,(*q,.))		// decrement q
		do : .. : ~.
	else on : %% : ? < ..
		in %<?:(.,?)>: 1
			do >"[1]\t%_ + %_ -> ":< %<?:((?,.),.)>, %<?:((.,?),.)> >
		else
			do >"\t%_ + %_ -> ":< %<?:((?,.),.)>, %<?:((.,?),.)> >
		do : p : %<?:((?,.),.)>		// base
		do : q : %<?:((.,?),.)>		// offset
		do : carry : %<?:(.,?)>		// carry
		do : .. : ~.
	else on exit < ..
		do exit


