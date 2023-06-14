/*===========================================================================
|
|			calculator story
|
+==========================================================================*/
#include "../4_Yak/yak.bm"
#include "dpu.bm"
:
	on init
		do : dpu : !! DPU
		do : yak : !! Yak(
			(( Rule, base ), ( Schema, {
				(: %sum :)
			} ))
			(( Rule, sum ), ( Schema, {
				(:%part:)
				(:%sum + %part:)
			} ))
			(( Rule, part ), ( Schema, {
				(:%mult:)
				(:%term:)
			} ))
			(( Rule, mult ), ( Schema, {
				(:%term * %term:)
				(:%mult * %term:)
			} ))
			(( Rule, term ), ( Schema, {
				(:%number:)
				(:\( %sum \):)
			} ))
			(( Rule, number ), ( Schema, {
				(:\i:)
			} )) )
		do : INPUT

	else in : INPUT
		...
			do : TRAVERSE
		...


	else in : TRAVERSE
		in .SYNC
			on ~( %%, . ) < *dpu
				do ~( .SYNC )
				do ( *yak, CONTINUE )
		else on ~(( %%, TAKE ), ? ) < *yak
			in .A // accumulator assignment
				in : A : ?  // inform ((*,*A),(((*A,*),...),digit(s)))
					do : %? : ((%?,*) ? (*%?,%<?>) : ((%?,*),%<?>))
			do ( %<, CONTINUE )
		else on ~(( %%, IN ), ? ) < *yak
			in %<?:( number )>
				do .A // turn on accumulator assignment
			else in %<?:( sum )>
				in : A : ?
					do :< A, op >:< ((%?,A)|((%|,%?),*op)), ADD >
				else	do :< A, op >:< A, ADD >
				do .SET
			else in %<?:( mult )>
				do :< A, op >:< ((*A,A)|((%|,*A),*op)), MULT >
				do .SET
			do ( %<, CONTINUE )
		else on ~(( %%, OUT ), ? ) < *yak
			in %<?:( number )>
				do ~( .A ) // turn off accumulator assignment
				in ?: ( *A, * )
					in .SET
						do ((( *dpu, SET ), ... ), %(%?,?:...))
						do ~( .SET ) // sync not required here
					else 
						do ((( *dpu, *op ), ... ), %(%?,?:...))
						do .SYNC
					do ~( %? )
			else in ~.: %<?:( ~sum: ~mult )>
				in : A : ( ?, A ) // previous A
					do ~( %?, A )
					do :< A, op >:< %?, %((*A,%?),?) >
					in ( %?, * )
						do ((( *dpu, %((*A,%?),?)), ... ), %((%?,*),?:...))
						do .SYNC
					else do ( %<, CONTINUE )
			else in %<?:( base )>
				=> en sortie, il n'y a plus qu'a recuperer la valeur courante du dpu
			else
				do ( %<, CONTINUE )
		else on ~( %%, ERR ) < *yak
			do exit
