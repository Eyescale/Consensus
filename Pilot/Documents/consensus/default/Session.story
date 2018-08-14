!! $.story()

on event < cgi >: . !!
	in : Leave-is->action: %event
		do exit
		/
	else in : Edit-is->action: %event do
		: view : ?-is->view: %event
		: ext : ?-is->ext: %event
		: entity < ?-is->entity: %event
		/
		then
			do >:$< file://Edit.hcn ></>
			/
	else in ( : Save-is->action: %event, view: ?-is->view: %event, ext: ?-is->ext: %event )
		in entity: ?-is->entity: %event
			do : entity < %entity
			then
				in path: ?-is->filename: %event do
					>< file:%path >: $.[ ?-is->contents: %event ] |
					: $[ %entity ]( %ext ) : %path
					/.
				else do
					>< file:$[ %entity ]( %ext ) >: $.[ ?-is->contents: %event ] |
					/.
				then
					do >:$[ %entity ]( html )
					/
		else
			in path: ?-is->filename: %event do
				>< file:%path >: $.[ ?-is->contents: %event ] |
				: $( %ext ) : %path
				/.
			else do
				>< file:$( %ext ) >: $.[ ?-is->contents: %event ] |
				/.
			then
				do >:$( html )
				/
		/
	else in ( : Remove-is->action: %event, entity: ?-is->entity: %event ) do
		: instance < %entity
		!~ %instance
		/
		then
			do >:$( html )
			/
	else in entity: ?-is->entity: %event
		in ( : Link-is->action: %event, arg1: ?-is->arg1: %event, arg2: ?-is->arg2: %event )
			in : ViaTo-is->binding: %event do
				: source < %entity
				: medium < %arg1
				: target < %arg2
				/.
			else in : ToFrom-is->binding: %event do
				: source < %arg2
				: medium < %entity
				: target < %arg1
				/.
			else in : FromVia-is->binding: %event do
				: source < %arg1
				: medium < %arg2
				: target < %entity
				/.
			then do
				: entity : !! %source-%medium->%target
				/.
		else in : Toggle-is->action: %event do
			: entity < %entity
			/
			then
				in : ViaTo-is->binding: %event do
					: binding : ToFrom
					/.
				else in : ToFrom-is->binding: %event do
					: binding : FromVia
					/.
				else in : FromVia-is->binding: %event do
					: binding : ViaTo
					/.
				/
		else in : Add-is->action: %event do
			: instance < %entity
			: entity : !! %instance
			/.
		else do
			: entity < %entity
			/.
		then
			do >:$[ %entity ]( html )
			/
	else
		do >:$( html )
		/
	/
/
