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
	else in : Save-is->action: %event 
		in ~: ?-is->filename: %event do
			: view: ?-is->view: %event
			: ext: ?-is->ext: %event
			: entity < ?-is->entity: %event
			: contents : ?-is->contents: %event
			/
			then
				do >:$< file://Edit.hcn ></>
				/
		else do
			: view: ?-is->view: %event
			: ext: ?-is->ext: %event
			>< file://%view.%ext >: $\[ ?-is->contents: %event ] |
			/
			then
				do >:$< file://%view.hcn ></>
				/
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
