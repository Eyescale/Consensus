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
		in path: ?-is->filename: %event do
			>< file:%path >: $.[ ?-is->contents: %event ] |
			: entity < ?-is->entity: %event
			/
			then
				in : %entity
					do : $[ %entity ]( %ext ) : %path
					then do >:$[ %entity ]( html )
				else
					do : $( %ext ) : %path
					then do >:$( html )
				/
		else in entity: ?-is->entity: %event do
			: entity < %entity
			> path: $[ %entity ]( %ext )
			/
			then
				in : %path
					do >< file:%path >: $.[ ?-is->contents: %event ] |
					then do >:$[ %entity ]( html )
				else
					do : contents : ?-is->contents: %event
					then do >:$< file://Edit.hcn ></>
				/
		else
			do > path : $( %ext )
			then
				in : %path
					do >< file:$( %ext ) >: $.[ ?-is->contents: %event ] |
					then do >:$( html )
				else
					do : contents : ?-is->contents: %event
					then do >:$< file://Edit.hcn ></>
				/
		/
	else in entity: ?-is->entity: %event
		in ( : Link-is->action: %event, arg1: ?-is->arg1: %event, arg2: ?-is->arg2: %event )
			in : ViaTo-is->binding: %event do
				: source < %entity
				: medium < %arg1
				: target < %arg2
				: entity : !! %source-%medium->%target
				/
				then
					do >:$[ %entity ]( html )
					/
			else in : ToFrom-is->binding: %event do
				: source < %arg2
				: medium < %entity
				: target < %arg1
				: entity : !! %source-%medium->%target
				/
				then
					do >:$[ %entity ]( html )
					/
			else in : FromVia-is->binding: %event do
				: source < %arg1
				: medium < %arg2
				: target < %entity
				: entity : !! %source-%medium->%target
				/
				then
					do >:$[ %entity ]( html )
					/
			/
		else in : Add-is->action: %event do
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
