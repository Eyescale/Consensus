:
/*
	Uses pre-instantiated literal - aka. dial - e.g.
		(:0123456789:)	==	0 --+---v
					    1 --+---v
						2 --+---v
						    3 --+---v
							... +---v
							    9 --+--\0
	to compare the two numbers X and Y
*/
	on init
		do : dial : !! Dial((:0123456789:))
		do : X : ((X,...):000189:)
		do : Y : ((Y,...):13234:)
		do : INIT
	else in : INIT
		do : p : *X
		do : q : *Y
		do : r : '='
		do : CMP
	else in : CMP
		on : %% : ? < *dial  // read operation's result
			in %<?:~'='>
				do : r : %<?>
			in : p : (?,.)
				do : p : ( %?:(.,*) ? ~. : %? )
			in : q : (?,.)
				do : q : ( %?:(.,*) ? ~. : %? )
			do : CMP
		else on : .  // dial operation on next digit(s)
			in : p : (.,?)
				do : *dial : (%?,(*q?%(*q:(.,?)):0))
			else in : q : (.,?)
				do : *dial : ( 0, %? )
			else
				do : OUTPUT
	else in : OUTPUT
		on : p : (.,?)
			do > "%s" : %?
			in ?: ( *p, . )
				do : p : %?
			else in : q : X
				do > " %s ": *r
				do : p : %((Y,*), . )
				do : q : Y
			else
				do > "\n"
				do exit
		else on : .
			do > "\t"
			do : p : %((X,*), . )
			do : q : X

: Dial
/*
	compare p and q (digits) on demand
*/
	on : p : ?
		in %?: *q
			do : .. : '='		// notify EQUAL
		else in (%?,(*q,.))
			do : .. : '<'		// notifier LESSER
		else in (%?,'\0')
			do : .. : '>'		// notify GREATER
		else
			do : p : %(%?,(?,.))	// increment p
	else on : %% : ? < ..
		do : p : %<?:(?,.)>
		do : q : %<?:(.,?)>
		do : .. : ~.
	else on exit < ..
		do exit


