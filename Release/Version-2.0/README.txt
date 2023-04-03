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

New Features

	The following features were added to the Version-1.2 feature list -
	modulo the changes mentioned in the Changes section below


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

	Signal Event Ocurrence		on ~( identifier ) < src	[UPDATE: 2023-04-03]
					on ~( identifier )		[UPDATE: 2023-04-03]

	EENO Register Variables		%<?:sub>		sub is a B% valid expression combining
					%<!:sub>		  . B% valid identifiers
					%<?>			  . the wildcard signus 
					%<!>			  . a single optional query signus ?
					%<			  . the negation signus ~

	self Register Variable		%%

	parent Register Variable	..

Changes & Extensions

	1. Narrative pipe Register Variable %|
	2. Variable assignment alternative syntax : variable, value
	3. Input alternative syntax do : input, format <
	4. Process active event introduced - idle event deprecated
	5. Base narrative perso
	6. Star (UNHACKED)

    1. Narrative pipe Register Variable

	The name of the Narrative pipe Register Variable was changed
	from
		%! in Version-1.1
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
	  of assignment are as follow:	[UPDATE: 2023-04-03]

	 When the query signus ? is included in the assignment expression,
	 then the %? mark register variable holds the corresponding query
	 result, if any, and the %! register variable holds the instance
	 corresponding to the overall query result - e.g.
		in : variable : ?
	 when successful, yields %! and %? verifying %!:((*,variable),%?)

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

    4. Process active event introduced - idle event deprecated

	B% Version-2.0 allows to check process activity during the last frame
	via the following syntax

		on . // covering ( . ), ~( . ), init and exit altogether

	The on ~. occurrence which, in Version-1.x, was used to check process
	inactivity over the last frame, now systematically fails, replaced by
	such occurrences as

		on ~.: .
		on ~.: . < proxy
		on ~.: . < { %%, proxy }

	Note the possibility to use %% as EENO source when it is explicitely
	mentioned - the underlying "connection" remaining un-activable.

    5. Base narrative perso

	We use the name perso to denote the narrative instance which is used
	to localize entities and relationship instances to the current cell's
	sub-narrative context, so that

		do .identifier
		do .( expression )

	actually perform the following instantiation(s)

		do ( perso, identifier )
		do ( perso, ( expression ) )

	and so that variables which were declared as locale

		.variable	// declaration

	are actually dereferenced as ( perso, variable )

	The purpose of this functionality is, on the one hand, to facilitate
	the reading and writing of sub-narratives, and on the other hand to
	optimize performances - as the sub-narrative instance is directly
	accessible from the context registry.

	Version-1.1 made the functionality available also in Base narrative
	context by instantiating a CNDB base entity named "this".

	In Version-2.0, we use the Self proxy instance (see below) to perform
	the same function - so that, for instance, do ~( %% ) will effectively
	release all relationship instances localized from the base narrative,
	whereas the Self proxy instance itself cannot be removed.

    6. Star (UNHACKED)

	The CNDB star * base entity is no longer created at CNDB creation time,
	nor is its address stored in db->nil->sub[1] - this was introduced in
	Version-1.2 in order to optimize assignment operations

New Feature Description

	1. New operator !!
	2. Subscribe / Unsubscribe
	3. Pause
	4. External Event Narrative Occurrence (EENO)
	5. Signal Event Occurrence
	6. EENO Register Variables
	7. Narrative self Register Variable %%
	8. Narrative parent Register Variable ..

    1. New operator !!

	CNCell instances are created upon either one of the following parent
	cell's narrative occurrences:

		do !! Narrative ( ... )
		do !! Narrative ( ... ) @<
		do !! Narrative ( ... ) ~<

		do : handle : !! Narrative ( ... )
		do : handle : !! Narrative ( ... ) @<
		do : handle : !! Narrative ( ... ) ~<

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

	The instantiated cell's init condition are specified within the
	parentheses - in lieu of the ellipsis symbol - as a list of comma-
	and/or newline- separated expressions. Referencing terms, namely

		*identifier
		.identifier
		~identifier
		*( expression )
		%( expression )
		.( expression )
		~( expression )
		expression:filter

	as well as either one of the following register variable names

		%% .. %? %! %| %< %<?> %<!> %<?:sub> %<!:sub>

	are allowed in these expressions. They will be dereferenced within
	the parent cell's CNDB and the resulting parent CNDB entities will
	be replicated into the new cell's CNDB.

	.identifier and .( expression ) are also dereferenced, so that the
	corresponding identifier resp. expression is replicated into the new
	cell's CNDB if it is associated with the current narrative perso.

	Note that the main use case for .identifier is to pass the value of
	a sub-narrative's argument (declared as .identifier in the narrative
	definition) to a cell instantiated in the sub-narrative's context.

	Otherwise, that is, when they are neither constitutive of referencing
	terms nor dotted, identifiers in cell's init conditions are replicated
	literally.

    2. Subscribe / Unsubscribe

	CNCell instantiation involves instantiating connections, which can
	be handled and stored as proxies within a cell's CNDB.

	Proxies can also be communicated from one cell to another, allowing 
	a cell to subscribe to [resp. unsubscribe from] another, unrelated
	cell, provided only that it is passed a proxy instance of a connection
	to that cell by one of its connections - or possibly by its parent,
	at instantiation.

	See example usage in the Narrative parent Register Variable paragraph
	below.

    3. Pause

	Execution will resume upon the User's pressing any key on the keyboard

    4. External Event Narrative Occurrence (EENO)

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

		%% .. %? %! %| %< %<?> %<!> %<?:sub> %<!:sub>

	will be dereferenced within the narrative instance's own cell's CNDB
	PRIOR to the occurrence query being launched.

	Such is the limitation imposed, by design, to the Consensus B% inter-
	cell communication protocol, that it allows only to query manifested
	changes - and not one another's internal conditions.

    5. Signal event occurrence - [UPDATE: 2023-04-03]

	B% Version-2.0 allows the following alternative syntax to be used

		on ~( identifier )
		on ~( identifier ) < src	// EENO

	to test signal events - which are release events associated with CNDB
	base entities. See example usage in

		Examples/1_Schematize/yak.new
		Examples/3_TM.multi/TM-head_cell.story

    6. EENO Register Variables	[UPDATE:2023-04-03]

	The EENO Register Variables %<?> and %<!> allow subsequent references
	to the EENO query results to be made by the narrative, using

		%<?>	to represent event = first match
		%<!>	to represent the overall query result in case of
			an assignment expression - if marked
		%<	to represent src = proxy to the corresponding connection

	The rules of assignment described in the Changes section of this document
	apply to the %<?> and %<!> Register Variables, where the additional EENO
	Register Variable %< holds the proxy instance to the EENO source.

	Note here that the occurrence expression

		on : variable : value < ?

	will yield the following results, if any

		%< 	matching source (proxy)
		%<?>	matching source (proxy)		[UPDATE: 2023-04-03]
		%<!>	matching ((*,variable),value)

	whereas the occurrence expression

		on : variable : ? < .

	will yield the following results, if any

		%< 	matching source (proxy)
		%<?>	matching value
		%<!>	matching ((*variable),value)	[UPDATE: 2023-04-03]

	And that when found on their own in occurrences the EENO register
	variables will be dereferenced inside the local CNDB prior to the
	occurrence query being launched, with the following exceptions:

	   1.	in %<?>			// as-is
		in %<!>			// as-is
		in %<?:sub>		// as-is
		in %<!:sub>		// as-is

		where the expression is evaluated as it is,

	   2.	do > "fmt" : %<?>	// as-is
		do > "fmt" : %<!>	// as-is
		do > "fmt" : %<?:sub>	// as-is
		do > "fmt" : %<!:sub>	// as-is

		where the expression is printed out as it is,

	   3.	do expression

		where expression will be substantiated into the local CNDB.

    7. Narrative self Register Variable %%

	The Narrative self Register Variable %% allows to reference
	the executing cell in narratives - e.g.

		on : connection : ? < .
			// one of my sources just registered a new connection
			in %<?>: %%
				// that connection is me
			else in %<?>: .
				// already registered
			else
				// subscribe to my source's new connection
				do %<?> @<

    8. Narrative parent Register Variable ..

	Likewise the Narrative parent Register Variable .. allows a proxy to
	the narrative's parent connection to be used.

