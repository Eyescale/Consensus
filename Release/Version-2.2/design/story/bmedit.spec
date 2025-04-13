System Description
	We start from a pseudo-narrative = System description,
	from which we derive cosystems and the System occurrences,
	which we categorize into conditions, events and actions

					O
				       -:-
				       / \
	 .---> condition .
	 |	  ...      } guard ----.
	 .---> condition .	        |
	 |			        |
	 |			        |
	 |  .---> event .	        v
	  --|	    ...   } trigger ----+---> action ----.
	    .---> event .			          |
	    ^ 					          |
	    | 					          |
	     ------------- [ B% Narrative ] --------------


				     Machine

	Notes
	. Conceptually each action is itself a System, where the cosystems
	  are B% entity [aka. state variable], and the occurrences are B%
	  instructions (performative)

Task List
	. Extract occurrences from System description
	. Navigate list of occurrences

	=> create my own vi-type terminal editor, using ANSI code

	bmedit filename - basic
		1. save screen
		2. cls + cursor(0,0)
		3. fill screen
		4. let user navigate
		   => must read user arrow key or similar information
		      and scroll if necessary
		5. on ":" enter command (last line)
		6. on q\cr
			restore screen
			exit

	Undo operation reverts the system from current to previous state.
	The way to support undo is, for each normal operation, to stack on
	the backward stack the reverse operation to the one leading from
	previous to current state, so that undo
	. pops the backward stack and performs the saved undo operation, while
	  informing the forward stack of the reverse operation to the undo
	  operation being done, so that redo
	  . pops the forward stack and performs the saved redo operation,
	    while re-informing the backward stack of the reverse operation
	    to the redo operation being done, so as to reallow undo.
	The forward stack is erased as soon as the user resumes normal operation.

	Example:
	operation:
		from position p
		insert "text"
		ending at position q
	=> undo operation:
		from position q
		remove 4 characters backward,
		ending at position p
	=> redo operation:
		from position p
		insert the 4 characters "text" [just removed]
		ending at position q

Task List
	./B% -i
	> support do expression // including output
	> allow to create System: ON|OFF relationship instances
		. need support for unnamed entities
		. need support for string entities
		. need support for do { ON|^system, OFF|^system }
		  => for %system to represent ON|OFF
	> allow	to output System definition [to stdout and/or file] as follow

		: cosystemA : Cosystem
		:"occurrence"
			ON|OFF
				: "occurrence" : ON|OFF < cosystem
				etc.
			passing
				: "occurrence" : ON|OFF < cosystem
				etc.
			passing
				: "occurrence" : ON|OFF < cosystem
				etc.
			etc.

			ON|OFF
				: "occurrence" : ON|OFF < cosystem
				etc.
			passing
				: "occurrence" : ON|OFF < cosystem
			etc.

		:"occurrence"
			ON|OFF
				: "occurrence" : ON|OFF < cosystem
			passing
				: "occurrence" : ON|OFF < cosystem
			etc.
		etc.

		: cosystemB : Cosystem
		etc.

	  Note that the above represents the overall System definition,
	  aka. data structure, and not the cosystem sub-class definitions,
	  aka. narratives and sub-narratives. These are kept in separate,
	  individual cosystem.bm files using the same template

		: cosystem : Cosystem
			...
		:"occurrence" // invoked when occurrence is ON
			...
		:"occurrence" // invoked when occurrence is ON
			...
		:"occurrence" // invoked when occurrence is ON
			...

	  but where
		... represents B% user code - including do : this : OFF

	> allow to launch system
			>>>>> launch from files <<<<<

		=> must be able to
			load system definition from file
			load Cosystem class definition from file
			load each cosystem.bm sub-class definition from file

		need support for sub-class execution
			need support for shared memory access
			need support for EENOC
			need support for %list

Example Usage - targeted
	./B% -i
	> do : occurrence : "whatever"
	> do (( *occurrence, {ON,OFF} ), cosystem )
	> do : action : ((*occurrence,ON),.)
	// assuming we have informed (Event,.) and (Condition,.)
	> do : trigger : !! | ( %(Event,?), %| )
	> do : guard : !! | {
		( %|, ( *trigger, *action ) )
		( %(Condition,?), %| ) }

	// to access trigger matching both action and list of events:
	// %( %(?,*action):~(~%(%(Event,?),?)) )

	// to create a whole ( action, ( trigger(s), guard(s) )) record in one go

	> do (( "occurrence", ON|OFF ), cosystem ) |{
		(!!,%|) |{	// new trigger
			((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
			((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
			...
			(!!,%|) |{	// new guard
				((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
				((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
				... }
			(!!,%|) |{	// new guard
				((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
				((( "occurrence", ON|OFF ), cosystem ), %(%|:(?,.)))
				... }
			... }
		(!!,%|) |{
			... }
		... }
	> do exit
	
	Eventually we also want to support:
	./B% -launch path/system.bm

Usage
	./B% -i

Design principles
	select cosystem	// make current
	select occurrence => assumes from cosystem
		if ((occurrence,ON|OFF),cosystem) doesn't exist
			=> propose to make it
		as event
		as condition
		as action

	do : occurrence : "whatever"
	do (( *occurrence, {ON,OFF} ), cosystem )
	do : action : ((*occurrence,ON),.)
	do : trigger : !! | ( %(?,Event), %| )
	do : guard : !! | { ( %|, ( *trigger, *action ) ), ( %(Condition,?), %| ) }

	// to get trigger matching both action and list of events:

	do : trigger : ( %(?,*action):~(~%(%(Event,?),?)) )

Question is
1. can I put my System in inifile format?

	(( "occurrence", ON|OFF ), cosystem ) |{
		(!!,%|) |{	// new trigger
			((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
			((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
			...
			(!!,%|) |{	// new guard
				((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
				((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
				... }
			(!!,%|) |{	// new guard
				((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
				((( "occurrence", ON|OFF ), cosystem ), %|^(?,.))
				... }
			... }
		(!!,%|) |{
			... }
		... }
	(( "occurrence", ON|OFF ), cosystem ) |{
		... }
	...

Simplified:

: cosystem
: "occurrence"
/*
	Specify occurrence's ON|OFF triggers/guards within System
*/
	ON 
		: "occurrence" : ON|OFF	< cosystem
		: "occurrence" : ON|OFF	< cosystem
		...
	/
		"occurrence" : ON|OFF < cosystem
		"occurrence" : ON|OFF < cosystem
		...
	/
		"occurrence" : ON|OFF < cosystem
		"occurrence" : ON|OFF < cosystem
		...
	OFF
		...
/*
	Specify cosystem's internal actions while occurrence is ON
	- could also [automatically] #include "cosystem.bm"
*/
{
	in : state	// cosystem state
	...
		do : this : OFF		// occurrence internal trigger
}

: "occurrence"
	...
...

: cosystem
	...

2. can I produce such System file?
	Sure...
	per .cosystem:%((.,ON|OFF),?)

User Interface
	create/select/rename	occurrence
	create/select/rename	cosystem
	bind		occurrence cosystem
	create/select	(( occurrence, ON|OFF ), cosystem )
		as Action
			=> create/select trigger(s)
				=> add/remove event
				=> create/select guard(s)
					=> add/remove condition
		as Event
			=> see/select trigger(s)
		as Condition
			=> see/select guard(s)

	I may want to name trigger / guard => to use by reference

Description
	Essentially ./B% -i produces
		System definition	- system.bm file => system named?
		Cosystem definitions	- cosystem.bm files

	To launch System, B%
	. create and inform System Shared Arena from System Definition
	. create proxy by launching each cosystem as Cosystem sub-classed
	  with sub-narrative

	My problem is that I want to write the whole thing in B%, whereas
	maybe in C is the best option?
		1. System Edition
			includes sub-narratives (?)
		2. System Execution
			uses Cosystem.bm and each cosystem-name.bm class to
			. create special CNProgram
			. operate that program

	What I am missing from a B% standpoint is
		./B% -launch system
	    where
		system is a directory holding
		    . System description [tbd]
		    . cosystem.bm definition
		    . cosystem-name.bm files, holding

			: cosystem-name : Cosystem
			.occurrence:&[ "whatever" ]
				...
				do : occurrence : OFF
			.occurrence:&[ "whatever" ]
				...
				do : occurrence : OFF
			etc.
	    or
		system is the path to System description file,
		including cosystem -> path/file mapping table

			// System Description
			...
			%%
			// Cosystem Class Definition
			#include "cosystem.bm"
			%%
			// Cosystem Sub-Narratives Definitions
			#include "toto.bm"
			#include "titi.bm"
			#include "tutu.bm"

		Actually the two last sections would be optional,
		as by default B% would look in the same path
		
System Definition
	Objective is to create and navigate the System:

		"occurrence"
			cosystem
				ON|OFF
					Trigger
						"occurrence" ON|OFF
						"occurrence" ON|OFF
						...
					Guard
						"occurrence" ON|OFF
						"occurrence" ON|OFF
						...
					Guard
						...
					...
				ON|OFF
					...
				...
			cosystem
				...
			...
		...

	User Actions
		select "-----"	=> select occurrence
			=> if does not exist: propose to create

		set current occurrence => created if does not exist
			do : occurrence : "asdlakjsdlaksjd"
		set current cosystem => created if does not exist
			do : cosystem : toto
		add trigger for that occurrence
			do : trigger : !! | ( %|, ((*occurrence,ON),*cosystem) )


Now for an occurrence which has already been created
	=> I may want to fetch it
	. change description
	. change referent
	. change or add trigger
		=> selected event
	. change or add guard
		=> selected trigger
		=> selected condition

Cosystem Definition - occurrence sub-narratives
	: cosystem
	.occurrence:&[ "whatever" ]	// invoked in/on :occurrence:ON
		...			// user code
		do : occurrence : OFF
	.occurrence:&[ "whatever" ]
		...
		do : occurrence : OFF
	...

