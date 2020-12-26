

	WORK IN PROGRESS...


Yak Example Execution Model

			rule: %(?,base)
		   .----------.---------- (etc.)
		   v	      v
		schema      schema*

			*rule @ schema position
	           .----------.----------(etc.)
                   v          v
		schema      schema

	Since schemas may subscribe to a rule already launched, including their own (CYCLIC)
	they must manage their own status directly from the feeder rule's schemas

	schema COMPLETE event must not happen at the same time as schema TAKE = (]/[,frame )
	otherwise rule would complete without taking new (forked) schemas into account.

	COMPLETE'd schemas must detect when successor resp. subscriber fails => defunct


Example 1. LEFT recursive rule: e | %rule e

	0: on init
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0)
				^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,0)
				^
			schema: %eee e \0	started at (CONSUMED,0): CYCLIC detected
				^
	1. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			schema: %eee . \0	(forked): started at (CONSUMED,1)
				     ^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,0): PASS => TAKE / finished at (CONSUMED,1)
				  ^
			schema: %eee e \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			schema: %eee e \0	(forked): started at (CONSUMED,1)
				     ^
	2. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			>>> schema: %eee . \0	started at (CONSUMED,1): FAIL
				         ^
			schema: %eee . \0	(forked): started at (CONSUMED,2)
				     ^
		rule: eee(1)
			>>> schema: e \0	started at (CONSUMED,0)/finished at (CONSUMED,1): g FAIL => FAIL
				      ^
			schema: %eee e \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			schema: %eee e \0	started at (CONSUMED,1): PASS => TAKE / finished at (CONSUMED,2)
				     ^
			schema: %eee e \0	(forked): started at (CONSUMED,2)
				     ^
	3. on '.'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): eee(1) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee . \0	started at (CONSUMED,2): PASS => COMPLETE / finished at (CONSUMED,3)
				     ^
		rule: eee(1)
			schema: %eee e \0	started at (CONSUMED,0): eee(1) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee e \0	started at (CONSUMED,1) / finished at (CONSUMED,2)
				     ^
			>>> schema: %eee e \0	started at (CONSUMED,2): FAIL
				         ^
	Final status
		g: base, started at (CONSUMED,0): %eee . \0
			=> eee(1) [CYCLIC]: subscribed to by g AND by eee(1), started at (CONSUMED,0): %eee e \0
				=> eee(1): started at (CONSUMED,1): %eee e \0
				<= eee(1): finished at (CONSUMED,2)
		g: started at (CONSUMED,2): %eee . \0
		g: finished at (CONSUMED,3)

	Output
		%g:{%eee:{ee}.}
		    ^----------- eee(1) pushed / starting at (CONSUMED,0)

Example 2. RIGHT recursive rule: e | e %rule

	0. on init
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0)
				^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,0)
				^
			schema: e %eee \0	started at (CONSUMED,0)
				^

	1. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			schema: %eee . \0	(forked): started at (CONSUMED,1)
				     ^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,0): PASS => TAKE / finished at (CONSUMED,1)
				  ^
			schema: e %eee \0	started at (CONSUMED,0): PASS => launch eee(2)
				  ^
		rule: eee(2)
			schema: e \0		(launched): started at (CONSUMED,1)
				^
			schema: e %eee \0	(launched): started at (CONSUMED,1)
				^
	2. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				^
			>>> schema: %eee . \0	started at (CONSUMED,1): FAIL
				         ^
			schema: %eee . \0	(forked): started at (CONSUMED,2)
				     ^
		rule: eee(1)
			>>> schema: e \0	started at (CONSUMED,0)/finished at (CONSUMED,1): g FAIL => FAIL
				      ^
			schema: e %eee \0	started at (CONSUMED,0): TAKE from eee(2) => FORKS
				  ^
			schema: e %eee \0	(forked): starts at (CONSUMED,2): PASS => TAKE / finished at (CONSUMED,2)
					^
		rule: eee(2)
			schema: e \0		started at (CONSUMED,1): PASS => TAKE / finished at (CONSUMED,2)
				  ^
			schema: e %eee \0	started at (CONSUMED,2): PASS => launch eee(3)
				  ^
		rule: eee(3)
			schema: e \0		(launched): started at (CONSUMED,2)
				^
			schema: e %eee \0	(launched): started at (CONSUMED,2)
				^
	3. on '.'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): eee(1) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee . \0	started at (CONSUMED,2): PASS => COMPLETE / finished at (CONSUMED,3)
				     ^
		rule: eee(1)
			schema: e %eee \0	started at (CONSUMED,0): eee(2) COMPLETE => COMPLETE / no finish frame
				  ^
			schema: e %eee \0	COMPLETE/started at (CONSUMED,2) / finished at (CONSUMED,frame2)
					^
		rule: eee(2)
			schema: e \0		started at (CONSUMED,1): COMPLETE / finished at (CONSUMED,2)
				^
			>>> schema: e %eee \0	started at (CONSUMED,2): FAIL from eee(3) => FAIL
				      ^
		rule: eee(3)
			>>> schema: e \0	started at (CONSUMED,2): FAIL <<<
				    ^
			>>> schema: e %eee \0	started at (CONSUMED,2): FAIL <<<
				    ^
	Final status
		g: base, started at (CONSUMED,0): %eee . \0
			=> eee(1): subscribed to by g, started at (CONSUMED,0): e %eee \0
				=> eee(2): subscribed to by eee(1), started at (CONSUMED,1): e \0
				<= eee(2): finished at (CONSUMED,2)
			=> eee(1): started at (CONSUMED,2): e %eee \0
			<= eee(1): finished at (CONSUMED,2)
		g: started at (CONSUMED,2): %eee . \0
		g: finished at (CONSUMED,3)

	Output
		%g:{%eee:{e%eee:{e}}.}
		    ^	  ^^
		    |	   ----- eee(2) pushed / starting at (CONSUMED,1)
		     ----------- eee(1) pushed / starting at (CONSUMED,0)
