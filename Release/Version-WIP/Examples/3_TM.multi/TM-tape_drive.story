:
	on init
		do : head : !! Head (
			((*,HALT), H )
			((*,INIT), A )
			( TUPLE, {
				(A,(0,(1,(RIGHT,B))))
				(A,(1,(1,(RIGHT,H))))
				(B,(0,(0,(RIGHT,C))))
				(B,(1,(1,(RIGHT,B))))
				(C,(0,(1,(LEFT,C))))
				(C,(1,(1,(LEFT,A))))
				} ) )
		do : tape : !! Tape (
			((*,BLANK), 0 ) )
		do exit

: Head
	on init
		on : tape : ? < ..
			do : tape : %<?> @<
		do : state : *INIT
	else in : state : ?
		on ~( %%, ? ) < *tape
			do > "%s": %?	// output state
			in %? : *HALT
				do exit
			else in ( TUPLE, (%?,(%<?>, ? )))
				do : symbol : %(%?:(?,.))
				do : shift : %(%?:(.,(?,.)))
				do : state : %(%?:(.,(.,?)))
			else
				do >"Error: head: unable to dereference ( state, symbol )\n"
				do exit
	else
		do >"Error: head: no state\n"
		do exit

: Tape
	on init
		on : head : ? < ..
			do : head : %<?> @<
		do : cell : !! Cell( first, ((*,symbol),*BLANK) )
	else on : symbol : ? < *head
		do : symbol : %<?>  // manifest symbol for cell
		on : shift : RIGHT < %<
			in : right : ?
				do : cell : %? @<
			else do : cell : !! Cell( ((*,symbol),*BLANK) )
			do : left : *cell ~<
			do : right : ~.
		else on : shift : LEFT < %<
			in : left : ?
				do : cell : %? @<
			else do : cell : !! Cell( ((*,symbol),*BLANK) )
			do : right : *cell ~<
			do : left : ~.
	else on exit < *head
		do exit
	else in : cell : ?
		in ~.: *left
			on : left : ? < %?
				do : left : %<?>
		in ~.: *right
			on : right : ? < %?
				do : right : %<?>
		on ~( %%, ? ) < %?
			do ( *head, %<?> )

: Cell
	//--------------------------------------------------
	//	rollback
	//--------------------------------------------------
	on ~( signal ) < *right
		do signal~
	else on ( *left ? ~.: ~( signal ) )
		do > "%s": (first?'|':)
		in : current
			do ( .., *symbol )
		else do callout~
	//--------------------------------------------------
	//	tape input
	//--------------------------------------------------
	else on : cell : ? < ..
		in %<?>: %%
			on first
				do signal~
			else on : left : ? < ..  // shift RIGHT
				// register left if didn't have
				in ~.: *left
					do : left : %<?> @<
				// manifest right for tape
				in : right : ?
					do : right : %?
			else on : right : ? < ..  // shift LEFT
				// register right if didn't have
				in ~.: *right
					do : right : %<?> @<
				// manifest left for tape
				in : left : ?
					do : left : %?
			do : current
		else in : current
			on : left : %% < .. // shift RIGHT
				in ~.: *right
					do : right : %<?> @<
			else on : right : %% < .. // shift LEFT
				in ~.: *left
					do : left : %<?> @<
			on : symbol : ? < ..
				do > "%s%s":< *symbol, (*right?' ':'\n') >
				do ( *right ? rollcall~ : signal~ )
				do : symbol : %<?>
			do : ~.
	else on exit < ..
		in : current
			do > "%s%s":< *symbol, (*right?' ':'\n') >
			do ( *right ? flush~ : exit )
	//--------------------------------------------------
	//	rollcall
	//--------------------------------------------------
	else on ~( callout )
		do > " %s%s":< *symbol, (*right?' ':'\n') >
		do ( *right ? rollcall~ : signal~ )
	else on ~( rollcall ) < *left
		do > "%s": (first?'|':)
		in : current
			on ~.::. // not newly made current
				do ( .., *symbol )
			else do callout~
		else do callout~
	//--------------------------------------------------
	//	flush
	//--------------------------------------------------
	else on ~( flush ) < *left
		do > "%s %s%s":< (first?'|':), *symbol, (*right?' ':'\n') >
		do ( *right ? flush~ : exit )
	else on exit < *right
		do exit
