:
	on init 
		do (( init, ... ):0 0 0 A0 0 0 0 0 0 0:)
//		do (( init, ... ):A0:)
		do : head : !! Head (
			((*,HALT), H )
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
	else
		on ( init )
			do : record : ( init, * )
		else in ( *record, ? )
			in %?: /[01]/
				// manifest symbol for tape
				do : symbol : %?
			else in %?: /[A-Z]/
				// manifest state for head
				do : state : %?
				do mark
			else do ( mark ? ~( mark ) :)
			do : record : %( *record, . )
		else
			do ( mark ? ~( mark ) : exit )

: Head
	on init
		on : tape : ? < ..
			do : tape : %<?> @<
		do : setup
	else in : setup
		on : state : ? < ..
			do : state : %<?>
		else on exit < ..
			do : ~.
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
		do : setup
	else in : setup
		on : symbol : ? < ..
			do : new : !! Cell( ((*,symbol),%<?>) )
		else on ~( mark ) < ..
			do : start : *new
		else on exit < ..
			in : start : ?
				do : cell : %?
				do : ~.
			else in : new : ?
				do : cell : %?
				do : ~.
			else do exit
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
	//	initialization
	//--------------------------------------------------
	on : new : ? < ..
		in %<?>: %%
			on ~((*,new), ? ) < ..
				do : left : %<?> @<
		else on ~((*,new), %% ) < ..
			do : right : %<?> @<
	//--------------------------------------------------
	//	tape input
	//--------------------------------------------------
	else on : cell : ? < ..
		in %<?>: %%
			on ~.: ~((*,cell),.) < ..
				do start
				// manifest right for tape
				in : right : ?
					do : right : %?
				// manifest left for tape
				in : left : ?
					do : left : %?
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
	//	rollback
	//--------------------------------------------------
	else on ~( signal ) < *right
		do signal~
	else on ( *left ? ~.: ~( signal ) )
		do > "%s": (start?'|':)
		in : current
			do ( .., *symbol )
		else do callout~
	//--------------------------------------------------
	//	rollcall
	//--------------------------------------------------
	else on ~( callout )
		do > " %s%s":< *symbol, (*right?' ':'\n') >
		do ( *right ? rollcall~ : signal~ )
	else on ~( rollcall ) < *left
		do > "%s": (start?'|':)
		in : current
			on ~.::. // not newly made current
				do ( .., *symbol )
			else do callout~
		else do callout~
	//--------------------------------------------------
	//	flush
	//--------------------------------------------------
	else on ~( flush ) < *left
		do > "%s %s%s":< (start?'|':), *symbol, (*right?' ':'\n') >
		do ( *right ? flush~ : exit )
	else on exit < *right
		do exit
