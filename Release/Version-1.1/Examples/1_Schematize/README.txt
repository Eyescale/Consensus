Name
	Consensus/Release/Version-1.1/Examples/1_Schematize

Usage
	../../B% -f Schemes/yak.example yak.story
	../../B% -f Schemes/file schematize
	../../B% hello_world.story

Description
	This directory holds the examples targeted by the Version-1.1 release
	development. 

	The yak.story and schematize examples allow the user to specify a full
	formal grammar consisting of a collection of named Rules, each Rule made
	of Schemas which in turn — using the ( %, identifier ) relationship
	instance — may reference Rules.

	The input read from stdin is then output to stdout, with the pattern-matching
	sequences encapsulated within the structure - process generally known as
	segmentation, but which we refer to as schematization.

	The schematize story differs from the yak.story example in that
	1. the former uses the construct (( identifier, % ), pattern ) instead of
	   the construct (( Rule, identifier ), ( Schema, pattern )) which is used
	   in the latter.
	2. the schematize story uses a do ( | pipe command instead of two separate
	   commands to launch a rule and associated schema threads.

	The Release/Version-1.1/design directory of this site provide more insight
	into the architecture of these examples.

	. The ./design/yak.story file is a prototype, in B% pseudo-code, of the
	  first targeted implementation - the final implementation of which is
	  located in the present directory.

	. The ./design/schema-fork-on-completion .jpg and .txt files describe the
	  entity behavior & relationship model (EBRM) underlying that design.

	The hello_world.story example only allows the use of Schemas, without any
	cross-referencing possibility. This example is presented in more details
	in the Consensus B% Programming Guide located in the Release/Documentation
	directory of this site, and is intended to facilitate the reader’s
	understanding of the other two, more advanced programs.

Data Model
			rule: %(?,base)
		   .----------.---------- (etc.)
		   v	      v
		schema      schema

			 rule @ schema position - aka. feeder rule
	           .----------.---------- (etc.)
                   v          v
		schema      schema

Execution Model
	Since schemas may subscribe to a rule already launched, including their own - CYCLIC
	case - they must manage their own status directly from the feeder rule's schemas

	schema COMPLETE event must not happen at the same time as schema TAKE = (]/[,frame )
	otherwise rule would complete without taking new (forked) schemas into account.

	COMPLETE'd schemas must detect when successor resp. subscriber fails => defunct

Example 1. rule with NULL schema

	./B% -f Schemes/A_null schematize

	0: on init
		rule: pass = base
			schema: %really' 'good \0	started at (CONSUMED,0): NULL in really => FORKS
				^
			schema: %really' 'good \0	(forked) started at (CONSUMED,0)
				       ^

		rule: really
			schema: very \0		started at (CONSUMED,0)
				^
	1: on 'g'
		rule: pass = base
			>>> schema: %really' 'good \0	started at (CONSUMED,0): really FAIL => FAIL
				    ^
			schema: %really' 'good \0	(forked) started at (CONSUMED,0): PASS
				          ^
		rule: really
			>>> schema: very \0	started at (CONSUMED,0): FAIL => really FAIL
				    ^
	2: on 'o'
		rule: pass = base
			schema: %really' 'good \0	(forked) started at (CONSUMED,0): PASS
				           ^
	3: on 'o'
		rule: pass = base
			schema: %really' 'good \0	(forked) started at (CONSUMED,0): PASS
				            ^
	4: on 'd'
		rule: pass = base
			schema: %really' 'good \0	(forked) started at (CONSUMED,0): PASS => COMPLETE at (CONSUMED,4)
				             ^
	Final status
		pass, started at (CONSUMED,0)/finished at (CONSUMED,4): %really' 'good \0

	Output
		%pass:{good}


Example 2. LEFT recursive rule: e | %rule e

	./B% -f Schemes/C_eee.left schematize

	0: on init
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0)
				^
		rule: eee(0)
			schema: e \0		started at (CONSUMED,0)
				^
			schema: %eee e \0	started at (CONSUMED,0): CYCLIC detected
				^
	1. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			schema: %eee . \0	(forked): started at (CONSUMED,1)
				     ^
		rule: eee(0)
			schema: e \0		started at (CONSUMED,0): PASS => TAKE / finished at (CONSUMED,1)
				  ^
			schema: %eee e \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			schema: %eee e \0	(forked): started at (CONSUMED,1)
				     ^
	2. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			>>> schema: %eee . \0	started at (CONSUMED,1): FAIL
				         ^
			schema: %eee . \0	(forked): started at (CONSUMED,2)
				     ^
		rule: eee(0)
			>>> schema: e \0	started at (CONSUMED,0)/finished at (CONSUMED,1): g FAIL => FAIL
				      ^
			schema: %eee e \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			schema: %eee e \0	started at (CONSUMED,1): PASS => TAKE / finished at (CONSUMED,2)
				     ^
			schema: %eee e \0	(forked): started at (CONSUMED,2)
				     ^
	3. on '.'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): eee(0) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee . \0	started at (CONSUMED,2): PASS => COMPLETE / finished at (CONSUMED,3)
				     ^
		rule: eee(0)
			schema: %eee e \0	started at (CONSUMED,0): eee(0) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee e \0	started at (CONSUMED,1) / finished at (CONSUMED,2)
				     ^
			>>> schema: %eee e \0	started at (CONSUMED,2): FAIL
				         ^
	Final status
		g: base, started at (CONSUMED,0): %eee . \0
			=> eee(0) [CYCLIC]: subscribed to by g AND by eee(0), started at (CONSUMED,0): %eee e \0
				=> eee(0): started at (CONSUMED,1): %eee e \0
				<= eee(0): finished at (CONSUMED,2)
		g: started at (CONSUMED,2): %eee . \0
		g: finished at (CONSUMED,3)

	Output
		%g:{%eee:{ee}.}
		    ^----------- eee(0) pushed / starting at (CONSUMED,0)

Example 3. RIGHT recursive rule: e | e %rule

	./B% -f Schemes/C_eee.left schematize

	0. on init
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0)
				^
		rule: eee(0)
			schema: e \0		started at (CONSUMED,0)
				^
			schema: e %eee \0	started at (CONSUMED,0)
				^

	1. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			schema: %eee . \0	(forked): started at (CONSUMED,1)
				     ^
		rule: eee(0)
			schema: e \0		started at (CONSUMED,0): PASS => TAKE / finished at (CONSUMED,1)
				  ^
			schema: e %eee \0	started at (CONSUMED,0): PASS => launch eee(1)
				  ^
		rule: eee(1)
			schema: e \0		(launched): started at (CONSUMED,1)
				^
			schema: e %eee \0	(launched): started at (CONSUMED,1)
				^
	2. on 'e'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): TAKE from eee(0) => FORKS
				^
			>>> schema: %eee . \0	started at (CONSUMED,1): FAIL
				         ^
			schema: %eee . \0	(forked): started at (CONSUMED,2)
				     ^
		rule: eee(0)
			>>> schema: e \0	started at (CONSUMED,0)/finished at (CONSUMED,1): g FAIL => FAIL
				      ^
			schema: e %eee \0	started at (CONSUMED,0): TAKE from eee(1) => FORKS
				  ^
			schema: e %eee \0	(forked): starts at (CONSUMED,2): PASS => TAKE / finished at (CONSUMED,2)
					^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,1): PASS => TAKE / finished at (CONSUMED,2)
				  ^
			schema: e %eee \0	started at (CONSUMED,2): PASS => launch eee(2)
				  ^
		rule: eee(2)
			schema: e \0		(launched): started at (CONSUMED,2)
				^
			schema: e %eee \0	(launched): started at (CONSUMED,2)
				^
	3. on '.'
		rule: g = base
			schema: %eee . \0	started at (CONSUMED,0): eee(0) COMPLETE => COMPLETE / no finish frame
				^
			schema: %eee . \0	started at (CONSUMED,2): PASS => COMPLETE / finished at (CONSUMED,3)
				     ^
		rule: eee(0)
			schema: e %eee \0	started at (CONSUMED,0): eee(1) COMPLETE => COMPLETE / no finish frame
				  ^
			schema: e %eee \0	COMPLETE/started at (CONSUMED,2) / finished at (CONSUMED,frame2)
					^
		rule: eee(1)
			schema: e \0		started at (CONSUMED,1): COMPLETE / finished at (CONSUMED,2)
				^
			>>> schema: e %eee \0	started at (CONSUMED,2): FAIL from eee(2) => FAIL
				      ^
		rule: eee(2)
			>>> schema: e \0	started at (CONSUMED,2): FAIL <<<
				    ^
			>>> schema: e %eee \0	started at (CONSUMED,2): FAIL <<<
				    ^
	Final status
		g: base, started at (CONSUMED,0): %eee . \0
			=> eee(0): subscribed to by g, started at (CONSUMED,0): e %eee \0
				=> eee(1): subscribed to by eee(0), started at (CONSUMED,1): e \0
				<= eee(1): finished at (CONSUMED,2)
			=> eee(0): started at (CONSUMED,2): e %eee \0
			<= eee(0): finished at (CONSUMED,2)
		g: started at (CONSUMED,2): %eee . \0
		g: finished at (CONSUMED,3)

	Output
		%g:{%eee:{e%eee:{e}}.}
		    ^	  ^^
		    |	  | ---- eee(1) pushed / starting at (CONSUMED,1)
		    |	   ----- output on eee(1) found to-be-pushed
		     ----------- eee(0) pushed / starting at (CONSUMED,0)
