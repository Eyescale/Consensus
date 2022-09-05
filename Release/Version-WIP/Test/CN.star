/*
	on init
		do *
	else in *
		do >"all is well: %_\n": *
		do exit
*/
//
	on init
		do ((*,toto),*)
	else
		in *toto: *
			do >"all is well: %_\n": *toto
		do exit
//
