/*===========================================================================
|
|			sub story main
|
+==========================================================================*/
/*
	Uses pre-instantiated literal - aka. dial - e.g.
		(:0123456789:)	==	0 --+---v
					    1 --+---v
						2 --+---v
						    3 --+---v
							... +---v
							    9 --+--\0
	to compute the sub Z of the two numbers X and Y
*/
:
	on init
		do : dial : !! Dial((:0123456789:))
		do : X : ((X,...):123:)
//		do : X : ((X,...):189:)
		do : Y : ((Y,...):13234:)
		do : Z : ((Z,...)::)
		do : INIT
	else in : INIT
		do : p : *X
		do : q : *Y
		do : r : (r,*)
		do : carry : 0
		do : SUB
	else in : SUB
		on : %% : ? < *dial  // read operation's results
			in %<?:~'\0'>
				do : r : ( *r, %<?> )
				in : p : (?,.)
					do : p : ( %?:(.,*) ? ~. : %? )
				in : q : (?,.)
					do : q : ( %?:(.,*) ? ~. : %? )
				do : SUB
			else do : carry : 1
		else on : .  // dial operation on next digit(s)
			in : p : (.,?)
				do : *dial : ((%?,(*q?%(*q:(.,?)):0)),*carry)
			else in : q : (.,?)
				do : *dial : ((0,%?),*carry)
			else // no more digits
				in : carry : 0
					do : FORMAT
					do : r : *r
				else
					do : r : %((r,*),.)
					do : q : ( q, * )
					do : NEG
			do : carry : 0
	else in : NEG
		on : %% : ? < *dial  // read operation's results
			in %<?:~'\0'>
				do : q : ( *q, %<?> )
				in ?: (*r,.)
					do : r : %?
				else do ~( r )
				do : NEG
			else do : carry : 1
		else on : .
			in : r : (.,?)
				do : *dial : ((0,%?),*carry)
				do : carry : 0
			else
				do : FORMAT
				do : r : *q
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
			in ?: (*p,.)
				do : p : %?
			else in : q : X
				do > " - "
				do : p : %((Y,*), . )
				do : q : Y
			else in : q : Y
				do > " = %s": ((*carry:1)?'-':)
				do : p : %((Z,*), . )
				do : q : Z
			else
				do > "\n"
				do exit
		else on : .
			do > "\t"
			do : p : %((X,*), . )
			do : q : X

: Dial
/*
	compute p - q - carry (digits) on demand
*/
	on : %% : ? < ..
		do : p : %<?:((?,.),.)>		// base
		do : q : %<?:((.,?),.)>		// offset
		do : carry : %<?:(.,?)>		// carry
	else on : q : ?
		in %?: 0
			in : carry : 0
				do : .. : *p	// notify result
			else
				do : carry : 0
				do : q : 1
		else in (?,(*p,.))
			do : p : %?		// decrement p
			do : q : %(?,(*q,.))	// decrement q
		else
			do : .. : '\0'		// notify carry
	else on : .. : '\0'
		do : p : %(?:~(.,.),'\0')	// reset base
		do : q : %(?,(*q,.))		// decrement q
	else on exit < ..
		do exit

