/*===========================================================================
|
|			dpu-mult story main
|
+==========================================================================*/
#include "dpu.bm"
:
	on init
		do : dpu : !! DPU
		do : X : ((X,...):1230456:)
		do : Y : ((Y,...):9870654:)
		do : MULT
	else in : MULT
		on : .
			do ((( *dpu, SET ), ... ), %((X,*),?:...) )
		else on ~( %%, SET ) < *dpu
			do ((( *dpu, MULT ), ... ), %((Y,*),?:...) )
		else on ~( %%, MULT ) < *dpu
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
				do > " x "
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
