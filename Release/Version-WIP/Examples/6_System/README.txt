Name
	Consensus/Release/Version-2.0/Examples/6_System

Usage
	../../B% output.story < TM.system
	../../B% inform.story < TM.system	[WIP]
	../../B% system.story < TM.system	[TODO]

File Format Description
	tabs else
	tabs [else] (in|on|off|do|cl) "_"
	tabs [else] cl "_" do "_"
	tabs [else] (on|off) &
	tabs > "_"

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
	as its direct incidence on others, when explicitely required:

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
		CL ...   #1	|	~.
	   	ELSE DO ...	| or
				|	...
				|	ELSE CL ... #1
				|	ELSE DO ...

	1. must check to concatenate output
	2. can be any number of [ELSE] IN/ON/OFF ...

inform.story
	The idea is to build the ( guard, ( trigger:(bar,on), action ) )
	each corresponding to oen of the above-described action-threads.
	We start with

		do :< guard, bar, on >: !!

	then, on the way to the action, we traverse the levels, each
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

	In order to build guard
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

	For trigger
		same logic, except :event:OFF is not the same as ~.::event:ON

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
		if ELSE
			in : event : ?
				do ( %?, *off )
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

		do ( *guard, ((*bar,*on),(%?,OFF)))
		if followed by ELSE DO ?
			do ( *guard, ((*bar,*on),(%?,ON)))

	    otherwise when we meet [ELSE] DO ?
		if ELSE
			in : event : ?
				do ( %?, *off )
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

		do : action : ((%?,ON)|(*guard,(*bar,*on),%|))
		then foreach following > ?
			do ( *action, %? )

	All this put together, PLUS

	1. create occurrence from string, as per
	   	do : s : $((s,~.),?:...)
	   which will look up / register string in CND string arena
	   This is performed upon leaving :string in ps, to go to
	   either :ward or :take (as states)

	2. verify compliance with rule
		"Generated cannot be generative, & v.v."
	   where
		generated means CL or >
		generative means DO followed by >

	   So when we meet CL or > ?
		we need to ward %? against all prior generative actions
	   and when we meet DO ?
		we need to ward %? against all prior generated occurrences

	   This is implemented in :ward which, depending on both current
	   type and the first character of the upcoming command - '>'
	   indicates either a generative DO or a generated occurrence -
	   is entered after :string

	   Note that, in case of no-compliance, the user must be informed
	   as to what prior statement caused the conflict, on what line.
	   For this LineNo notifies the current line number as soon as
	   parent enters :line, prior to :string

	   The current line number is stored along with the current generative
	   action or generated occurrence in :ward

	3. reuse existing guard or trigger:{bar,on}
	   After :inform - which sorts out conditions and events to inform
	   current action's guard and trigger:{bar,on} - we want to reuse
	   existing guard and trigger:{bar,on} from instantiated

		( .guard, ( .trigger:(.bar,.on), (.occurrence,ON|OFF) ))

	   according to
		per ?:%( ?:~%(~%(?,*guard),?), ( ., (.,ON|OFF) ))
			// all *guard conditions apply to %?
			in ~.:( ~%(?,%?), *guard )
				// all %? conditions apply to *guard
				do { ~(*guard), ((*,guard),%?) }

	   which, if found, frees previous and use existing to instantiate
		   do ( *guard, ((*bar,*on), *action ))

	  This is implemented in :register which is entered after :inform

NOTES
	. currently we do allow CL string DO string // with same string
	. in/on/off occurrences may be specified without do at the end
	  of system file => currently just ignored

TODO
	. output informed data structure
	. integrate with cosystem and launch

