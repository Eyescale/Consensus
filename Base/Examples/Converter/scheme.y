#

	: /\n
	| in% %{ START }
	| in( %{ START }
	| in: %{ START }
	| in~ %{ START }
	| %{ in_ } in %cond \n %{ _in }

 cond	: %state
	| %op

 state	: %{ name } : %% %{ take }
	| %{ not name } :~ %% %{ take }
	| %{ name } : %% %{ value } : %% %{ take }

 op	: %{ and_ } ( %expr ) %{ _and }
 	| %{ or_ } { %expr } %{ _or }
 	| %{ not and_ } ~( %expr ) %{ _and }
 	| %{ not or_ } ~{ %expr } %{ _or }

 expr	: %state
	| %op
	| %expr, %{ comma } %state
	| %expr, %{ comma } %op

