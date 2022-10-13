Name
	Consensus Programming Language (B%) version 1.2	[ BETA ]

Usage
	./B% program.story
	./B% -p program.story
	./B% -f file.ini program.story

Description
	This release was originally intended to support all the prototype
	stories located in its ./design/story sub-directory, and
	ultimately to support user specification and concurrent execution
	of multiple ( narrative, CNDB ) instances, from a single story.

	The first of these prototypes, named single-thread.story, features
	a simplified Turing Machine example implementation - note that the
	original version is still supported, and can be found under the
	Examples/0_TuringMachine sub-directory of this release.

	The new version is located in the Examples/2_TM.single sub-directory
	of this release.

	This example does not use an ini file, so that all the information
	pertaining to its execution are visible from one source - namely
	Examples/2_TM.single/TM-single.story

	This, together with the introduction of other minor features - aimed
	at enhancing narrative readability [see list below] - required changes
	to be made to our existing Version-1.1 code base which proved more
	substantial than anticipated.

	Hence our decision to create this intermediate minor release, limited
	to supporting the TM.single story example, prior to moving on to our
	targeted major feature development - now in Version-WIP.

Contents
    Version-1.2 File Format extensions

	      Feature Name	      Syntax				Description

	  assignment operator	in/on/do : variable : value	(( *, variable ), value ) instantiated
	  			in/on/do : variable : ~.	(( *, variable ), value ) released if it was
								instantiated, and ( *, variable ) manifested

	  list		      (( expr, ... ):_sequence_:)	instantiates ((((( expr, * ), a ), b ), ... ), z )
				       ^-------- ellipsis	            star --------^
								where
									_sequence_ is as per B% literal

	  ternary operator    ( ... expr ? expr : expr ... )	the first sub-expression (which we call guard)
								is evaluated independently of its level in the
								overall expression

	  negative		in ~.: expression		passes if expression fails
				on ~.: expression		passes if no event matches expression
				do ~.: expression		nop

	  signal		do identifier~			releases identifier base entity upon creation


    Other Changes and Additions [ SECTION TO BE COMPLETED ]

	      Feature Name				Description

	  Base mark		The position of the register variable indicator "?" is no longer restricted.
				For instance the following narrative - distributed as Test/10_basemark

					on init
						do : variable : value
					else on : variable : .
						do : variable : ~.
					else on ~((*,variable), ? )
						do >"%_\n": %?
						do exit

				outputs the variable's previous value - here "value" - provided that value
				itself has not been released.

