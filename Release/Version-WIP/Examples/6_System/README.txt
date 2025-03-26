Name
	Consensus/Release/Version-2.0/Examples/6_System

Usage
	../../B% output.story < TM.system
	../../B% inform.story < TM.system	[WIP]
	../../B% system.story < TM.system	[TODO]

File Format
	tabs else
	tabs [else] (in|on|off|do|cl) "_"
	tabs [else] cl "_" do "_"
	tabs [else] (on|off) &
	tabs  > "_"
	tabs  > &

Rules
	. type '>' cannot be ELSE, must be level, and preceded by DO 
	. '&' references the last CL resp. DO string in narrative,
	  regardless of level - we call it 'that' -
	  and can only be used in IN, ON, OFF expressions
	. Special case: condition / no action
		no problem: just added on stack of conditions
	. output only takes place when action definition is complete
		=> all '>' have been defined
		=> need to wait for the next cmd to know
	. Note that the same action may be involved in
		different places of the pseudo-narrative
			=> will be repeated
	  Also currently cannot detect guard / trigger repetition
	  This will change when store the narrative in CNDB

Data Structure
	We use tab, *tab and **tab to keep track of the path to action's current
	conditions, as follow:

		: tab : ((( tab, '\t' ), ... ), '\t' )
			*tab -----------------^

		: *tab : (((( .(*tab), (IN,$)), (ELSE_IN,$)), ... ), (ELSE_OFF,$))
			      ^------ ( %%, *tab )
	where
		$ is (( '\0', ... ), %((?,...):*s) ) /* aka. string, which
					is input (s,...) in reverse */

	and as illustrated below:
	
			**tab grows vertically
				 |
		in $ ------------+--> *tab grows horizontally
				 |
			...	 |
				 v
				in $
				else in $
					on $	<<<< CANNOT BE same level
				...
				else off $
					current conditions

	Now with current pointing to an element in *tab - so that we do have
		: tab : %( *current, ... )
	then upon a new occurrence:

	. No more than the number of tabs in *tab may have been entered
	. The same number of tabs as are in *tab must have been entered for
	  the current conditions to apply, in which case
	  . we cannot accept ELSE_XX
	  . if IN, ON, or OFF is entered then we add one tab and start new condition
	  . if DO or CL is entered then we must mark *tab, so that when we come
	    back from subsequent new condition we do not accept ELSE_XX either
	  . Note that we also do not accept ELSE_XX if last condition is ELSE
	. if less tabs than are in *tab were entered, then we do
		: tab : ( *current, ~. )
		// which btw here above would leave (ELSE_OFF,$) dangling
	  and apply the same rules as above, with the exception that here, ie.
	  after we pop, we do accept ELSE_XX iff current tab is not marked, and
	  the last condition is not ELSE

	Note: implementation follows these rules without marking

output.story
	generates the following output, which represents for each action
	individually the complete set of enabling circumstances, as well
	as its direct incidence on others when it is specified:

	...
	--------------------------------------------------------
	IN ....
	ON ....
	ELSE IN .....
	ELSE ON .....
		IN .....
		ELSE OFF ....
		ELSE
			DO .....
			> ......
			> ......
	--------------------------------------------------------
	...

	We have %?:(( *q, ... ), (cmd,string))
	where
             : p : (( tab, ... ), '\t' )
             : q : ( take, *p )
	
	1. output all the tabs from *p going backward
             do >"%$": %(%((?,...):*p):(.,?))
	
	2. output ( cmd, string )
	   where
		cmd:	%(%?:((.,(?,.))
		string: %(%?:((.,(.,?))
	   and
	   	cmd can be singleton or pair ( ELSE, . )

	The last ( *q, ... ) list can be either

		CL ...		| or
	   or			|	... #2
	   	DO ...		|	ELSE CL ...
		> ....		| or
	   	> ....		|	... #2
	   or			|	ELSE DO ...
		CL ...   #1	| or
	   	ELSE DO ...	|	... #2
				|	ELSE CL ... #1
				|	ELSE DO ...

	1. must check to concatenate output
	2. can be any number of [ELSE] IN/ON/OFF ...

inform.story
	The idea is to instantiate {( guard, ( trigger:(on,bar), action ) )}
	each corresponding to one of the above-described action-threads,
	and thence to generate the following report, per action

	: "occurrence" ON|OFF	// action
	> "occurrence"
	> "occurrence"
	> ...
	  guard
		"occurrence" ON|OFF
		"occurrence" ON|OFF
		...
	    trigger
		"occurrence" ON|OFF
		"occurrence" ON|OFF
		...
	      /
		"occurrence" ON|OFF
		"occurrence" ON|OFF
		...
	    trigger
		...
	      /
		...
	  guard
		...
	    trigger
		...
	      /
		...
	
	The construction happens in :inform which takes the place of :output
	from output.story.

	We start with
		do :< guard, on, bar >: !!
	Then, on the way to the action, we traverse the levels, each
	consisting of a number of
			IN	// starting the level only*
			ON	// starting the level only*
			OFF	// starting the level only*
			ELSE IN
			ELSE ON
			ELSE OFF
		* mutually exclusive
	until the last level, which is  EITHER
		of the same kind, finishing with either
			ELSE CL followed by ELSE DO
		   	ELSE CL
		   	ELSE DO	
	   OR
		solely made of either
		   	CL
			CL followed by ELSE DO
		   	DO
			DO followed by a number of >

	We use the following logic to build guard along the way
		only the last [ELSE] IN per level may represent a condition ON
		and that is only if it is not followed at that level by ELSE .
		all prior [ELSE] IN at that level are condition OFF

		So when we meet [ELSE] IN ?
			in : condition : ? // dispose of previous candidate
				do (( %?, OFF ), *guard )
			do : condition : %?
		otherwise when we meet ELSE anything
			in : condition : ?
				do (( %?, OFF ), *guard )
				do : condition : ~.
		and when we reach the end of that level
			in : condition : ?
				do (( %?, ON ), *guard )

	We use the same logic to build trigger, with the difference that
		:event:OFF is not the same as ~.::event:ON
		:event:ON  is not the same as ~.::event:OFF
	therefore we must use a double trigger { on, bar }

		So when we meet either [ELSE] ON or [ELSE] OFF ?
			in : event : ? // dispose of previous candidate
				do ( %?, *bar )
			in case [ELSE] ON
				do : event : ( %?, ON )
			else // case [ELSE] OFF
				do : event : ( %?, OFF )
		otherwise when we meet ELSE anything
			in : event : ?
				do ( %?, *bar )
				do : event : ~.
		and when we reach the end of that level
			in : event : ?
				do ( %?, *on )

	Finally for action
	    when we meet [ELSE] CL ?
		if ELSE CL
			in : event : ?
				do ( %?, *bar )
				do : event : ~.
			in : condition : ?
				do ((%?,OFF), *guard )
				do : condition : ~.
		otherwise
			in : event : ?
				do ( %?, *on )
				do : event : ~.
			in : condition : ?
				do ((%?,ON), *guard )
				do : condition : ~.

		do ( *guard, ((*on,*bar),(%?,OFF)))
		if followed by ELSE DO ?
			do ( *guard, ((*on,*bar),(%?,ON)))

	    otherwise when we meet [ELSE] DO ?
		if ELSE DO
			same as above
		otherwise
			same as above

		do ( *guard, ((*on,*bar),(%?,ON)))
		do : action : %?
		so that foreach following > ?
			do ( *action, %? )

	All this put together, PLUS

	1. create occurrence from string, as per
	   	do : s : $((s,~.),?:...)
	   which looks up and/or registers string in string arena.
	   This is performed in :ps prior to transitioning to :ward

	2. verify compliance with rule
			"Generated cannot be generative, & v.v."
		where
			generated means CL or >
			generative means DO followed by >

	   So when we meet CL or > ?
		we need to ward %? against all prior generative actions
	   and when we meet DO ?
		we need to ward %? against all prior generated occurrences

	   This is implemented in :ward which is entered from :ps and
	   performs according to both current type and the first character
	   of the upcoming command, as '>' indicates either a generative DO
	   or a generated occurrence.

	   There the current line number is stored along with the action
	   occurrence & type, so that in case of non-compliance the user
	   is informed as to what prior statement caused the conflict,
	   on what line.

	   Note that generated occurrences must not be set or cleared
	   anywhere else, which is also ascertained in :ward

	3. instantiate
		( .guard, ( .trigger:(.on,.bar), (.occurrence,ON|OFF) ))

	   This takes place in :instantiate which is entered after
		:ward--see above--then
	   	:take--which builds the current action's thread--then
		:inform--which informs { guard, on, bar, action }
	   and performs
		   do ( *guard, ((*on,*bar), *action ))

	   We reuse existing guards and/or triggers { on, bar } as per
	   the following approach

	   // find %? so that all *guard conditions apply to %?
	   per ?:%( ?:~%(~%(?,*guard),?), ((!!,!!), (.,/(ON|OFF)/) ))
		// verify that all %? conditions apply to *guard
		in ~.:( ~%(?,%?):~(*,.), *guard )
			// free current *guard and use existing
			do { ~(*guard), ((*,guard),%?) }

	   and likewise for *on and for *bar

system.story
	Finally system.story executes TM.system, using as includes the
	appropriate cosystem/*.bm class definition files so as to

	1. perform the occurrence <-> cosystem association as per
			en ascribe(( !!, DO ), . )
	   with
		.: ascribe(( .pivot, DO ), .occurrence )
			do (( pivot, CO ), ?::occurrence )
	   where
		?::occurrence returns the identifier of the cosystem
		containing the occurrence's string narrative

	2. launch all cosystem instances (aka. class actors) as per
			do : %((!!,CO),?) : !!( launch-formula )
	   where
		do : identifier : !!( launch-formula )
		instantiates one identifier class actor

  Special cases
	init
		Assuming that we have, in system.story
			in ?: "init"
				do : init : ( %?, ON )
				do !! | {
					((%|,SET),%?),
					((%|,CO),System) }
		and that Cosystem init performs
			in ?: "init"
				on ( %?, ON )
					do : %? : ON
		then in launch formula, we create
			?:( *init:OCC )( %?, *^^ )
	exit
		Assuming that we have, in system.story
			in ?: "exit"
				do : exit : ( %?, ON )
				do !! | {
					((%|,SET),%?),
					((%|,CO),System) }
			as well as the causal relationship instance
				( !!, ((!!,!!),*exit) )
		and that all cosystems somehow implement
			: "exit"
				do exit
		then in launch formula, we create
			?:( *exit:~OCC )( %?, *^COS )

	subscription
		All cosystems must perform, upon initialization
			do %(( ., /(ON|OFF)/), ?:~%% ) @<

NOTES
	. guard, on, bar may have no associated condition/event in which
	  case inform.story will report - here with all three barren:
		: "occurrence" ON|OFF
		  in
		    on
		      /
		: "next occurrence" ON|OFF
		  ...
	. output.story allows in/on/off occurrences to be specified with
	  no action at the end of system description, whereas
	  inform.story will report error and exit in this case.

