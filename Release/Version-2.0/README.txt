Name
	Consensus Programming Language (B%) version 2.0	[ BETA ]

Usage
	./B% program.story
	./B% -p program.story
	./B% -f file.ini program.story

Description
	The objective of this release was to substantiate all the prototypes
	located in its ./design/story sub-directory, and ultimately to support
	concurrent execution of multiple ( narrative, CNDB ) instances, from
	a single story.

	The first of these prototypes - named single-thread.story - featuring
	a simplified Turing Machine example implementation, was delivered as
	part of the Consensus/Release/Version-1.2 - the original version is
	still supported, and can be found under the Examples/0_TuringMachine
	sub-directory of this release, whereas the newer version can be found
	under the Examples/2_TM.single sub-directory of this release.

	The functional examples derived from the two other prototypes can now
	be found under the Examples/3_TM.multi sub-directory of this release,
	as
		TM-head_tape.story
		TM-head_cell.story

	The difference between these examples is that the latter's execution
	model does not rely on a global memory addressing scheme.

	Each individual cell is an actor of its own in the Turing Machine story,
	its execution relying on proper communication and synchronization with
	its peers -- this, after all, being the whole point motivating the
	Consensus development effort, overall.

	None of these examples uses an ini file, so that all the information
	pertaining to their executions are visible from one source - namely
	the example story.

Contents		[ SECTION TO BE COMPLETED ]
    New Features
	The following features were added to the Version-1.2 additions - modulo
	the changes mentioned in the Changes section below

	    	Feature Name			Expression			Comment

	New operator !!			do !! Narrative ( ... )		!! stands for Bang-bang
					do !! Narrative ( ... ) @<
					do !! Narrative ( ... ) ~<

					do : handle : !! Narrative ( ... )
					do : handle : !! Narrative ( ... ) @<
					do : handle : !! Narrative ( ... ) ~<

	Subscribe / Unsubscribe		do expression @<
					do expression ~<
					do : handle : expression @<
					do : handle : expression ~<

	Pause				do :<				useful for debugging

	External Event Narrative	on event < src
	Occurrence (EENO)

	EENO Register Variables		%<?:sub>		sub is a B% valid expression combining
					%<!:sub>		  . B% valid identifiers
					%<?>			  . the wildcard signus 
					%<!>			  . a single optional query signus ?
					%<			  . the negation signus ~

	parent Register Variable	..

	self Register Variable		%%

Description
    1. New operator !!

	CNCell instances are created upon either one of the following parent
	cell's narrative operations:

		do !! Narrative ( ... )
		do !! Narrative ( ... ) @<
		do !! Narrative ( ... ) ~<

		do : handle : !! Narrative ( ... )
		do : handle : !! Narrative ( ... ) @<
		do : handle : !! Narrative ( ... ) ~<

	The instantiated cell's init condition are specified within the
	parentheses - in lieu of the ellipsis symbol - as a list of comma-
	and/or newline- separated expressions. Referencing terms, such as

		register variable
		expression:filter
		%( expression )
		*expression

	are allowed in these expressions. These will be dereferenced within
	the parent cell's CNDB prior to the resulting expression's being
	replicated into the new cell's CNDB.

	The second occurrence within each of these subsets is equivalent to
	the first (default) occurrence, whereby both parent and instantiated
	cells subscribe to each other upon instantiation. This enables them
	to track each other's events (a.k.a. changes) via Narrative EENO.

	In addition to creating a CNCell instance, the second subset of
	above-listed occurrences allows a proxy of the instantiated connection
	to be assigned to a local CNDB handle - expression - if the connection
	was instantiated.

	The last occurrence within both subsets denotes an occurrence where
	parent unsubscribes upon instantiation from its instantiated cell,
	after which neither parent nor child are able to track each other's
	changes - until potentially future explicit connection.

	The very last occurrence in the above list releases and unassigns the
	connection upon instantiation, which leaves the (*,handle) relationship
	instance created but with nothing to show for it - not even via
	on ~((*,handle),?) ... It is, however, manifested.

    2. Subscribe

	CNCell instantiation involves instantiating connections, which can be
	handled and stored as proxies within a cell's CNDB.

	Proxies can also be communicated from one cell to another, thereby
	allowing a cell to subscribe to another, unrelated cell, provided only
	that it is passed a proxy instance of a connection to that cell by one
	of its connections - or by its parent, at instantiation.

	See example usage in the Narrative parent Register Variable paragraph
	below.

    3. Unsubscribe

	Same as above, with unsubscribe from instead of subscribe to.

    4. Pause

	Execution will resume upon the User's pressing any key on the keyboard

    5. External Event Narrative Occurrence (EENO)

	External Event Narrative Occurrences (EENO) are occurrences in the form

			on event < src

		where
			event and src are expressions

	Any referencing term found in the EENO event and src expressions, namely

		*identifier
		.identifier
		~identifier
		*( expression )
		%( expression )
		.( expression )
		~( expression )
		expression:filter

	as well as either one of the following register variable names

		.. %? %! %| %< %<?> %<!> %<?:sub> %<!:sub>

	will be dereferenced within the narrative instance's own cell's CNDB
	PRIOR to the query's being launched

	Such is the limitation imposed, by design, to the Consensus B% inter-
	cell communication protocol, in that it allows only to query manifested
	changes - and not one another's internal condition.

    6. EENO Register Variables

	The EENO Register Variables %<?> and %<!> allow subsequent references
	to the EENO query results to be made by the narrative, using

		%<?>	to represent event = first match
		%<!>	to represent the complementary match (variable or value)
			in case of assignment expression - if marked
		%<	to represent src = proxy to the corresponding connection

	The rules of assignment described in the Changes section of this document
	apply to the %<?> and %<!> Register Variables, where the additional EENO
	Register Variable %< holds the proxy instance corresponding to the query
	result, if any.

	Note here that the occurrence expression

		on : variable : value < ?

	will yield the following results, if any

		%< 	matching source (proxy)
		%<?>	matching variable
		%<!>	matching value

	whereas the occurrence expression

		on : variable : ? < .

	will yield the following results, if any

		%< 	matching source (proxy)
		%<?>	matching value
		%<!>	matching variable

	Note that when found in occurrences the EENOV register variables %<?> %<!>
	will be dereferenced inside the local CNDB prior to the occurrence query
	being launched, with the following exceptions:

		in %<?:sub>	// as-is
		in %<!:sub>	// as-is

	where the sub expression is evaluated directly in the source CNDB, and

		do expression

	where the %<?> and %<!> expression terms, if any are present as-is (that
	is: unfiltered etc.) will be converted locally, modulo instantiation.

    7. Narrative parent Register Variable ..

	The Narrative parent Register Variable .. allows a proxy to the narrative's
	parent ( narrative, instance ) to be used - e.g.

		on : connection : ? < .
			// one of my sources just registered a new connection
			in %<?>: ..
				// that connection is my parent
			else in %<?>: .
				// already subscribed
			else
				// subscribe to my source's new connection
				do @< %<?>

    8. Narrative self Register Variable %%

	Likewise the Narrative self Register Variable %% allows a proxy to
	the current ( narrative, instance ) to be used.

Changes
		1. Narrative pipe Register Variable %|
		2. Variable assignment alternative syntax : variable, value
		3. Input alternative syntax do : input, format <
		4. Process idle event deprecated - process active event
		   introduced

	1. Narrative pipe Register Variable

	   The name of the Narrative pipe Register Variable was changed
	   from
			%! in Version-1.x
	   to
			%| in Version-2.0

	2. Variable assignment alternative syntax : variable, value

	   The following variable assignment syntax is allowed
		in/on/do : variable, value

	   as this is how, as of Version-2.0, the assignment occurrence is
	   represented internally, and how it will show in story output.

	   Note however that neither variable nor value can be filtered
	   so as to be consistent with the alternative (preferred) syntax
		in/on/do : variable : value
	  
	 . Concerning the Narrative %? and %! Register Variables, the rules
	   of assignment are as follow:

	   When the query signus ? is included in the variable part of
	   the assignment occurrence, then the %? mark register variable
	   holds the corresponding query result, if any, and the %!
	   register variable holds the instance corresponding to the value
	   part of the query result.

	   Conversely, when the query signus ? is included in the value
	   part of the assignment occurrence, then the %? mark register
	   variable holds the corresponding query result, if any, and
	   %! holds the instance corresponding to the variable part of
	   the query result.

	3. Input alternative syntax do : input, format <

	   B% Version-2.0 allows the following input syntax to be used

			do : <
			do input <
			do input, "fmt" <

	   as this is how, as of Version-2.0, input occurrences are represented
	   internally, and how they will show in story output.

	   Note that the Version-1.x syntax

			do input: <
			do input: "fmt" <

	   is still externally supported, and that input EOF now manifests as
	   on : input : ~. event occurrence, as opposed to ~( *, input ) in
	   Version-1.x, which required ( *, input ) to be pre-instantiated.

	4. Process idle event deprecated - process active event introduced

	   B% Version-2.0 allows to check process activity during the last frame
	   via the following syntax

		on .	covering ( . ), ~( . ), init and exit events altogether

	   The on ~. occurrence which was used in Version-1.x to check process
	   inactivity over the last frame now systematically fails, replaced by
	   such occurrences as

		on ~.: .
		on ~.: . < proxy
		on ~.: . < { %%, proxy }

	  Note the possibility to use %% as EENO source when it is explicitely
	  mentioned - the underlying "connection" remaining un-activable.


