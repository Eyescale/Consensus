System Description
	We start from a pseudo-narrative = System description
	from which we derive the System occurrences, which are
	categorized into actions, events and conditions

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
	. each action is itself a System, where the cosystems are
	  B% entity [aka. state variable], and the occurrences are
	  B% instructions (performative)

System Completion
	The system is complete when

	. Every condition has a corresponding action or event
	  occurrence specified and in use

	. Every event occurrence is either
	  . corresponding to an action specified and in use
	  . specified as originating from an action, whose
	    B% Narrative generates the event via

		in ?: "occurrence"
			.%action <(%?,ON|OFF)>

  	Notes
	. Every action occurrence also represents the event and condition
	  occurrences corresponding to the action
		do "occurrence" => in/on "occurrence" allowed
	. An action which is not specified as event originator does
	  not require a B% narrative, as it may just serve to set
	  system conditions
	. The "action" done event if not specified also as an action
	  does requires a B% narrative to be specified for the action,
	  where this event is generated via .%action <(this,OFF)>

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

