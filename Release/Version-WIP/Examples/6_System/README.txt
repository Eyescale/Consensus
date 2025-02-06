Name
	Consensus/Release/Version-2.0/Examples/6_System

Usage
	../../B% system.story < TM.system
	../../B% output.story < TM.system

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

Implementation
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
					> current conditions

	Now with current pointing to an element in *tab, so that we do have
		*tab:%( *current, ... )

	Upon a new occurrence,
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
		// which btw here would leave (ELSE_OFF,$) dangling
	  and apply the same rules as above, with the exception that here, ie.
	  after we pop, we do accept ELSE_XX iff current tab is not marked, and
	  the last condition is not ELSE

	Note: implementation follows these rules without marking

TODO:
	. output
	. create in CNDB
	. integrate with cosystem
