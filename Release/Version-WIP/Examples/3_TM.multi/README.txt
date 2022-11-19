Name
	Consensus/Release/Version-2.0/Examples/3_TM.multi

Usage
	../../B% TM-head_tape.story
	../../B% TM-head_cell.story

Description
	Implements a Turing Machine[1] using the Consensus B% programming language.

	Each example features a full B% program (aka. story in Consensus terms)
	allowing to read in and execute a Turing Machine, thereby demonstrating
	the Turing completeness of the language.

	The difference between these examples is that the latter's execution
	model does not rely on a global memory addressing scheme.

	Each individual cell is an actor of its own in the Turing Machine story,
	its execution relying on proper communication and synchronization with
	its peers -- this, after all, being the whole point motivating the
	Consensus development effort, overall.

	None of these examples uses an ini file, so that all the information
	pertaining to their executions are visible from one source - namely
	the example story.

	The example features the machine from Lin and Rado[1].

Reference
	1. http://www.logique.jussieu.fr/~michel/tmi.html
