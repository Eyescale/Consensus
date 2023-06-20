/*===========================================================================
|
|			DPU story main
|
+==========================================================================*/
#include "../4_Yak/yak.bm"
#include "dpu.bm"
:
	on init
		do : dpu : !! DPU
		do : yak : !! Yak(
#include "../4_Yak/Schemes/calculator"
			(( Rule, base ), ( Schema, (:%op:) ))
			(( Rule, op ), ( Schema, {
				(:+ %sum :)
				(:* %sf :)
				(:* %sf + %sum :) } ))
			(( Rule, sf ), ( Schema, {
				(:%term:)
				(:%mult:) } ))
			)
		do : A : ~.
		do : INPUT

	else in : INPUT
		in .READY // pending on user input
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
			in ~.: : input : ~'\n'
				do >&"dpu-drive > "
			// input next character
			do input: "%c" <
			do .READY
		else on ~( %%, IN ) < *yak
			// yak recognized input and traverses results
			in : input : '\n'
				do : PROCESS
				do ( *yak, CONTINUE )
				do ~( check )
			else do ( *yak, FLUSH )
		else on ~( %%, OUT ) < *yak
			// yak failed to recognize input
			in check
				do : FLUSH
			else do ( *yak, DONE )

	else in : PROCESS
		in .SYNC // pending on dpu's return value
			on ~( %%, . ) < ?:*dpu
				do (((*A,*),~.)|((%|,...),%<(!,?:...)>))
				do ( *yak, CONTINUE )
				do ~( .SYNC )
		else on ~(( %%, TAKE ), ? ) < *yak
			in .N
				do : *A : ((*A,*) ? (**A,%<?>) : ((*A,*),%<?>))
			else in : A : ~.
				in %<?:'*'>
					do :< A, op >:< (A,A), MULT >
				else in %<?:'+'>
					do :< A, op >:< (A,A), ADD >
			do ( %<, CONTINUE )
		else on ~(( %%, IN ), ? ) < *yak // pushing rule
			in %<?:number>
				do ( .SET ? ~. : ~(*A,*) )
				do .N
			else in %<?:mult>
				do .SET
				do :< A, op >:< ((A,*A)|((%|,*A),*op)), MULT >
			else in %<?:sum>
				do .SET
				in : A : ?
					do :< A, op >:< ((A,%?)|((%|,%?),*op)), ADD >
				else	do :< A, op >:< ((A,A),~.), ADD >
			do ( %<, CONTINUE )
		else on ~(( %%, OUT ), ? ) < *yak // popping rule
			in %<?:number>
				in .SET
					do ((( *dpu, SET ), ... ), %((*A,*),?:...))
					do ( %<, CONTINUE )
					do ~( .SET )
				else
					do ((( *dpu, *op ), ... ), %((*A,*),?:...))
					do .SYNC
				do ~( .N )
			else in ( %<?:sum> ?: %<?:mult> )
				in : A : ( A, ?:~A ) // pop < A, op >
					do ~( A, %? )
					do :< A, op >:< %?, %((*A,%?),?) >
					in ( %?, * ) // operate if informed
						do ((( *dpu, %((*A,%?),?)), ... ), %((%?,*),?:...))
						do .SYNC
					else // pass current value down
						do (((%?,*),...),%((*A,*),?:...))
						do ( %<, CONTINUE )
				else do ( %<, CONTINUE )
			else in %<?:sf>
				do : op : ADD
				do ( %<, CONTINUE )
			else in %<?:base>
				do : A : ~.
				do : OUTPUT
			else
				do ( %<, CONTINUE )

	else in : OUTPUT
		do >"%$\n": %(((A,A),*),?:...)
		do ( *yak, DONE )
		do : INPUT

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

