/*===========================================================================
|
|			dpu-drive-ui story main
|
+==========================================================================*/
#include "../4_Yak/yak.bm"
/*
   Usage
	../../B% dpu-drive-ui.story
   Purpose
	Test dpu-drive User Interface using ../4_Yak/calculator scheme
   Note
	This implementation relies on completion occurring, when it does, on
	the next event unconsumed (presumably '\n').
*/
:
	on init
		do : yak : !! Yak(
#include "../4_Yak/Schemes/calculator"
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
				do >&"dpu-drive > "
			// input next character
			do input: "%c" <
			do .READY
		else on ~( %%, IN ) < *yak
			// yak recognized input and traverses results
			in : input : ~'\n'
				do ( *yak, FLUSH )
			else
				do ~( check )
				do : TRAVERSE
				do ( *yak, CONTINUE )
		else on ~( %%, OUT ) < *yak
			// yak failed to recognize input
			in check
				do : FLUSH
			else do ( *yak, DONE )
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
			   %<(!,?)> target rule id - unused here
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

	else in : FLUSH
		on : . // output error msg
			in check
				do >"\x1B[1;33m"
				    "~~~~~~~~~~~"
			else	do >"           "
//				do >"dpu-drive >"
			do ( *yak, CONTINUE )
		else on ~( %%, TAKE ) < ?:*yak
			do >"%s": ( check ? '~' : ' ' )
			do ( %<, CONTINUE )
		else on ~( %%, OUT ) < *yak
			in check
				do >"^\n"
				do ~( check )
				do : %(:?) // reenter
			else in : input : ?
				in %?: '\n'
					do >"  incomplete"
				else	do >"  ?"
				do : input : %?
		else on : input : ?
			in : input : ~'\n'
				do input : "%c" <
			else
				do >"\x1B[0m\n"
				do ( *yak, DONE )
				do : INPUT

