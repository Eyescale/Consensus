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

System Rules
	. Each DO or CL action occurrence substantiates its corresponding
	  event and condition occurrences, i.e.
		DO "occurrence" => IN/ON "occurrence" substantiated
	. DO action occurrences may be specified as occurrence originators,
	  via
		DO "action occurrence"
		> "generated occurrence"
		...
	. occurrence-generating action occurrences MUST be associated with
	  a B% narrative generating the event, via
		: "action occurrence"
			...
			in ?: "generated occurrence"
				.%action <(%?,ON|OFF)>
	  This includes
		DO "action occurrence"
		> &
	. When an action narrative is specified, then the corresponding
	  CL action can not be specified outside that narrative.
	  The corresponding OFF event occurrence, if used as IN/ON/OFF,
	  must be generated from inside that same narrative, via
		: "action occurrence"
			...
			.%action <(this,OFF)>
	. Conversely neither CL action nor generated occurrences are permitted
	  a B% narrative, whereas DO action occurrences which are neither
	  also specified as CL action occurrences nor generating occurrences
	  may or may not have associated B% narrative - in the negative, they
	  then substantiate System events/conditions.
	. TBD: whether multiple actions can generate the same occurrence(s)
	  and whether a CL occurrence can also be generated remains to be
	  decided, which has bearing on whether CL and generated occurrences
	  require a System cosystem instance, and/or whether generated occurrences
	  pertain to the originating cosystem, in which case
	  . a System cosystem instance is only required in case of CL occurrences
	  . one additional level of indirection is required to translate
	    generated occurrences into system occurrences

Rule Summary
		"Generated cannot be generative, & vice-versa"
	where
		CL occurrences are considered generated occurrences
		DO occurrences are considered generative if they are
		   explicitely specified as generating occurrences

System Completion
	The system is complete when every IN/ON/OFF occurrence has
	a corresponding action or generated occurrence, that is:
	Each event or condition occurrence is either
	  . corresponding to an action specified and in use
	  . specified as originating from an action, via
		: "action occurrence" // B% narrative
			...
			in ?: "event or condition occurrence"
				.%action <(%?,ON|OFF)>

Notes
	. If the system is not complete, then the user must be informed as to
	  which IN/ON/OFF occurrence does not have an originator
	. Some action or generated occurrences may not be used as IN/ON/OFF
	  occurrences, in which case the user must be warned

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

