/*===========================================================================
|
|			yak-drive story main
|
+==========================================================================*/
#include "yak.bm"
:
	on init
		in ( Rule, . ) // Scheme from file.ini
			do : yak : !! Yak( %((Rule,.),(Schema,.)) )
		else
			// default Scheme: dot-terminated number
			do : yak : !! Yak(
				(( Rule, base ), ( Schema, (:%term:)))
				(( Rule, term ), ( Schema, (:%test.:)))
				(( Rule, test ), ( Schema, {
					(:\d:)
					(:%test\d:)	} ))
				)
		do : INPUT

	else in : INPUT
		in .READY
			on : input : ?
				do ( *yak, %? )
				do ~( .READY )
				in %?: ~'\n'
					do check
			else on : input : ~.
				do > "  \n" // wipe out ^D
				do exit
		else on ~( %%, READY ) < *yak
			// prompt based on last input, none included
			in ~.: : input : ~'\n'
				do >&"yak-drive > "
			// input next character
			do input: "%c" <
			do .READY
		else on ~( %%, IN ) < *yak
			// yak recognized input and traverses results
			do : TRAVERSE
			do ( *yak, CONTINUE )
		else on ~( %%, OUT ) < *yak
			// yak failed to recognize and flushes input
			do : OUT
			do ( *yak, CONTINUE )
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
			   %<(!,?)> current rule id, if there is
			   yak pending on response, which can be either
				CONTINUE - push and enter new rule %<?>
				PRUNE - pass and continue current rule
				DONE - end traversal */
			do > "%%%s:{": %<?>
			do ( %<, CONTINUE )
		else on ~(( %%, OUT ), ? ) < *yak
			/* %<?> current rule id
			   %<(!,?)> target rule id or, in case %<?:base>, carry
			   yak pending on response, which can be either
				CONTINUE - pop current and continue previous rule
				PRUNE - pop and skip to the end of previous rule
				DONE - end traversal */
			in %<?:( base )>
				in %<(!,?)> // carry
					do > "}%s": %<(!,?)>
				else	do > "}"
				do : INPUT
				do ( %<, DONE )
			else
				do > "}"
				do ( %<, CONTINUE )
		else on ~( %%, ERR ) < *yak
			do exit

	else // in : OUT
		on ~(( %%, TAKE ), ? ) < *yak
			do > "%s": %<?>
			do ( %<, CONTINUE )
		else on ~( %%, OUT ) < ?:*yak
			/* anything other than DONE here and yak will replay
			   input sequence
			*/
			do ( %<, DONE )
			do : INPUT

