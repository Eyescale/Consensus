/*===========================================================================
|
|			dpu-add story main
|
+==========================================================================*/
#include "dpu.bm"
:
	on init
		do : dpu : !! DPU
#		do : X : ((X,...):123:)	// no carry
		do : X : ((X,...):189:)
		do : Y : ((Y,...):13234:)
		do : ADD
	else in : ADD
		on : .
			do ((( *dpu, SET ), ... ), %((X,*),?:...) )
		else on ~( %%, SET ) < *dpu
			do ((( *dpu, ADD ), ... ), %((Y,*),?:...) )
		else on ~( %%, ADD ) < *dpu
			do ( *dpu, GET )
		else on ~( %%, GET ) < ?:*dpu
			do : Z : (((Z,*), ... ), %<(!,?:...)> )
			do : OUTPUT
	else in : OUTPUT
		on : p : (.,?)
			do > "%s" : %?
			in ?: ( *p, . )
				do : p : %?
			else in : q : X
				do > " + "
				do : p : %((Y,*), . )
				do : q : Y
			else in : q : Y
				do > " = "
				do : p : %((Z,*), . )
				do : q : Z
			else
				do > "\n"
				do exit
		else on : .
			do >"\t"
			do : p : %((X,*), . )
			do : q : X
