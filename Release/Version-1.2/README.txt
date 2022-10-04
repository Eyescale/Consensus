Name
	Consensus Programming Language (B%) version 1.2

Usage
	./B% program.story
	./B% -p program.story
	./B% -f file.ini program.story

Description
	The objective of this version was to support the examples provided
	in the ./design/story sub-directory of this site, eventually allowing
	to specify and execute multiple B% ( narrative, CNDB ) instances
	in parallel.

	The first example, named single-thread.story, is a rewriting of the
	original Turing Machine example, which we still support and is provided
	in the Examples/0_TuringMachine sub-directory of this release.

	The new version of this same example can be found under the
	Examples/2_TM.single sub-directory of this release.

	This example relies on a few extensions originally intended only to
	improve the readbility of the code - for instance, the assignment
	operation 
			do : variable : value

	was supposed to be a direct tranlation of its equivalent

			do ((*,variable),value)

	operation.

	Our intention was to quickly implement these extensions - see the
	Contents section below for a full list and description - and then
	move on to the key feature really motivating this development.

	However, after we completed our task, we realized that the amount of
	changes affecting the existing code base justified the creation of an
	intermediate minor release, as a basis for the targeted major feature
	development.

Contents
    File Format extensions

	      Feature Name	      Syntax				Comments

	  assignment		in/on/do : variable : value	(( *, variable ), value ) instantiated
				in/on/do : variable : ~.	(( *, variable ), value ) released if it was
								instantiated, and ( *, variable ) manifested
								in any case

	  negation		in ~.: expression		passes if expression fails
				on ~.: expression		passes if no event such as per expression
				do ~.: expression		nop

	  signals		do identifier~			releases identifier upon creation

	  ternary-operated   ( ... expr ? expr : expr ... )	the first sub-expression (called guard)
	  expression						is evaluated independently of its level
								in the overall expression

	  list		      (( expr, ... ):_sequence_:)	(where _sequence_ as per literal)
		      ellipsis --------^			instantiates ((((( expr, * ), a ), b ), ... ), z )
									    star --------^
    Other
			[ documentation in progress ]

	. The position of the register variable indicator "?" is no longer
	  restricted. For instance the following narrative

			on init
				do : variable : value
			else on : variable : .
				do : variable : ~.
			else on ~((*,variable), ? )
				do >"%_": %?
				do exit

	  will output the variable's previous value - here "value" -
	  provided the value itself has not been released.

