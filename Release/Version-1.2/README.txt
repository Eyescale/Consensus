Name
	Consensus Programming Language (B%) version 1.2

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
	targeted major feature development - now happening in Version-WIP.

New Features

	The following features were added to the Version-1.1 feature list -
        modulo the changes mentioned in the Changes section below


	      Feature Name	      Syntax				Description

	assignment operator	in/on/do : variable : value	(( *, variable ), value ) instantiated
	  			in/on/do : variable : ~.	(( *, variable ), value ) released if it was
								instantiated, and ( *, variable ) manifested

	ternary operator    ( ... expr ? expr : expr ... )	the first sub-expression (which we call guard)
								is evaluated independently of its level in the
								overall expression - see more information in
								./design/specs/bm-TERNARY-implementation.txt

	contrary operator	in ~.: expression		passes if expression fails
				on ~.: expression		passes if no event matches expression
				do ~.: expression		nop

	list		      (( expr, ... ):_sequence_:)	instantiates ((((( expr, * ), a ), b ), ... ), z )
				       ^-------- ellipsis	            star --------^
								  where
									_sequence_ is as per B% literal

	signal			do identifier~			releases base entity named identifier upon creation

Changes & Extensions

	1. Base mark
	2. Star
	3. Sub-narrative definition
	4. Input handling integration
	5. Concurrent reassignment unauthorized

    1. Base mark

	The position of the query signum ? indicative of the Narrative query
	Register Variable %? is no longer restricted to leading the occurrence.

	For instance the following narrative - distributed as Test/10_basemark

		on init
			do : variable : value
		else on : variable : .
			do : variable : ~.
		else on ~((*,variable), ? )
			do >"%_\n": %?
			do exit

	outputs the previous variable's value - here "value" - provided that
		value itself has not been released.

    2. Star

	The CNDB star * base entity is now created at CNDB creation time.
	It can be released and thereby release all star-associated relationship
	instances - including variable-value associations - but it can no longer
	be removed.

    3. Sub-narrative definition

	Sub-narratives can now be defined as follow

		.identifier: ( expression )	// starting new line

	where
		identifier anywhere in the sub-narrative's body will reference
		the sub-narrative instance, instead of the default "this" as per
		Version-1.1 sub-narrative definition syntax - still supported:

		: ( expression )		// starting new line

    4. Input handling integration	

	.ini file, external input, and general instantiation are now sharing
	the same code base. As a consequence, all capabilities previously
	available only through external input (e.g. creation of B% literals)
	are now generically available.

	The contrary is of course far from being true, e.g. no reference is
	allowed via external input to existing CNDB contents, and assignment
	operations are only supported externally via ((*,variable),value)

    5. Concurrent reassignment unauthorized

	Assigning the same variable twice in the course of the same frame will
	fail, causing a Warning to be printed out to stderr.

Others
	The Consensus code base is now organized as follow:

		./Consensus/Base
			source code for libcn.a
			Examples/
				0_System/
				1_Yak/
				2_Converter/
				3_Segmentize/
				4_StateMachine/
				5_Ternarize/
				6_BTreefy/
		./Consensus/Release
			Version-1.0/
				source code for libbmod.a
				main.c for B% executable
				Examples/
					0_TuringMachine/
				Test/
			Version-1.1/
				source code for libbmod.a
				main.c for B% executable
				Examples/
					0_TuringMachine/
					1_Schematize/
				Test/
			Version-1.2/
				source code for libbmod.a
				main.c for B% executable
				Examples/
					0_TuringMachine/
					1_Schematize/
					2_TM-single/
				Test/

	Note that, as of Version-1.2, we generate and use the static
	object libraries libcn.a and libbmod.a as opposed to dynamic
	shared object libraries.

