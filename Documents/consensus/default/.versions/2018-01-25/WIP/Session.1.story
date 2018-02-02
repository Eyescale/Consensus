!! $.story()

on event < cgi >: . !!
	in : Leave-is->action: %event
		do exit
		/
	else in : Edit-is->action: %event do
		: view: ?-is->view: %event
		: ext: ?-is->ext: %event
		: entity < ?-is->entity: %event
		/
		then
			do >:$< file://Edit.hcn ></>
			/
	else in ( : Save-is->action: %event, view: ?-is->view: %event, ext: ?-is->ext: %event )
		do > path : $( %ext )
		then
			in : %path do
				>< file:%path >: $.[ ?-is->contents: %event ]
				>: $( html )
				/.
#			else do
#				: contents : ?-is->contents: %event
#				>:$< file://Edit.hcn ></>
#				/.
			else
				do : contents : ?-is->contents: %event
				then do >:$< file://Edit.hcn ></>
			/
	else in entity: ?-is->entity: %event
		in : Add-is->action: %event do
			: instance < %entity
			: entity : !! %instance
			/
			then
				do >:$[ %entity ]( html )
				/
		else in : Remove-is->action: %event do
			: instance < %entity
			!~ %instance
			/
			then
				do >:$( html )
				/
		else in : Link-is->action: %event do
			: source < %entity
			: medium < ?-is->medium: %event
			: target < ?-is->target: %event
			: entity : !! %source-%medium->%target
			/
			then
				do >:$[ %entity ]( html )
				/
		else do
			: entity < %entity
			/
			then
				do >:$[ %entity ]( html )
				/
		/
	else
		do >:$( html )
		/
	/
/
