/*===========================================================================
|
|			yak-drive story main
|
+==========================================================================*/
/*
	Assumption - cf yak.bm: Yak always completes, if it does, on next
	event unconsumed (presumably '\n')
*/
#define YAK_DRIVE
#include "yak.story"
:
	on init
		in ( Rule, . ) // Scheme from file.ini
			do : yak : !! Yak( %((Rule,.),(Schema,.)) )
		else
			// default Scheme: dot-terminated number
			do : yak : !! Yak(
				(( Rule, base ), ( Schema, (:%term:)))
				(( Rule, term ), ( Schema, (:%int.:)))
				(( Rule, int ), ( Schema, {
					(:\d:), (:%int\d:) } )) )
		do : INPUT

	else in : INPUT
		in .READY
			on : input : ?
				in %?: ~'\n'
					do check
				do ( *yak, %? )
				do ~( .READY )
			else on : input : ~.
				do > "  \n" // wipe out ^D
				do exit
		else on ~( %%, READY ) < *yak
			// prompt based on last input, none included
			in : input : ~'\n'
			else do >&"yak-drive > "
			do input: "%c" <
			do .READY
		else on ~( %%, IN ) < *yak
			// yak recognized input and traverses results
			in : input : '\n'
				do ~( check )
				do : TRAVERSE
				do ( *yak, CONTINUE )
			else
				do ( *yak, FLUSH )
		else on ~( %%, OUT ) < *yak
			// yak failed to recognize input
			in check
				do : FLUSH
			else in : input : '\n'
				do : %(:?) // reenter
				do ( *yak, DONE )
			do ~( check )
		else on ~( %%, ERR ) < *yak
			do exit

	else in : TRAVERSE
		on ~(( %%, TAKE ), ? ) < *yak
			/* %<?> event
			   yak pending on response, which can be either
				CONTINUE - continue current rule
				PRUNE - skip to the end of current rule
				DONE - end traversal */
			do > "%s": %<?>
			do ( %<, CONTINUE )
		else on ~(( %%, IN ), ? ) < *yak
			/* %<?> target rule id
			   yak pending on response, which can be either
				CONTINUE - push and enter new rule %<?>
				PRUNE - pass and continue current rule
				DONE - end traversal */
			do > "%%%s:{": %<?>
			do ( %<, CONTINUE )
		else on ~(( %%, OUT ), ? ) < *yak
			/* %<?> target rule id
			   yak pending on response, which can be either
				CONTINUE - pop current and continue previous rule
				PRUNE - pop and skip to the end of previous rule
				DONE - end traversal */
			do > "}"
			do ( %<, CONTINUE )
		else on ~( %%, OUT ) < *yak
			/* yak pending on response to return to input mode */
			do > "}"
			do : INPUT
			do ( *yak, DONE )
		else on ~(( %%, CARRY ), ? ) < *yak
			/* last event unconsumed - e.g. \i completing on fail
			   yak pending on response to return to input mode */
			do > "}%s": %<?>
			do : INPUT
			do ( *yak, DONE )
		else on ~( %%, ERR ) < *yak
			do exit

	else in : FLUSH
		on : . // output error
			in check
//				do >"yak-drive >"
				do >"           "
			else
				do >"\x1B[1;33m"
				    "~~~~~~~~~~~"
			do ( *yak, CONTINUE )
		else on ~( %%, TAKE ) < ?:*yak
			do >"%s": ( check ? ' ' : '~' )
			do ( %<, CONTINUE )
		else on ~( %%, OUT ) < *yak
			in check
				in : input : '\n'
					do >"  incomplete"
					    "\x1B[0m\n"
				else	do >"  ?"
					    "\x1B[0m\n"
				do : input : *input
				do ~( check )
			else
				do >"^\n"
				do check
				do : %(:?) // reenter
		else on : input : ?
			in : input : ~'\n'
				do input : "%c" <
			else
				do ( *yak, DONE )
				do : INPUT

