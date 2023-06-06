/*===========================================================================
|
|			yak-drive story main
|
+==========================================================================*/
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

	else on exit < *yak
		in : OUT
			do > "  \n" // wipe out ^D
		do exit
			
	else in : INPUT
		on ~( %%, READY ) < *yak
			// yak is pending on input
			// prompt based on last input, none included
			in : input : ~'\n'
			else do >&"yak-drive > " // prompt
			do input: "%c" <
		else on ~( %%, IN ) < *yak
			// yak recognized input and traverses results
			do : TRAVERSE
		else on ~( %%, OUT ) < *yak
			// yak failed to recognize and flushes input
			do : OUT

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
			/* yak either returning to input mode or exiting */
			do > "}"
			do : INPUT
		else on ~(( %%, CARRY ), ? ) < *yak
			/* last event unconsumed - e.g. \i completing on fail
			   yak then either returns to input mode or exits
			   without further notification */
			do > "}%s": %<?>
			do : INPUT
		else on ~( %%, ERR ) < *yak
			do : ERR

	else // in : OUT or : ERR
		on ~(( %%, TAKE ), ? ) < *yak
			do > "%s": %<?>
			do ( %<, CONTINUE )
		else on ~( %%, OUT ) < ?:*yak
			do : INPUT

