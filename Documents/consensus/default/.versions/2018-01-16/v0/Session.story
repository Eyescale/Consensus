!! $.story()

on event < cgi >: . !!
	in : Leave-is->action: %event
		do exit
		/
	else in ( : Edit-is->action: %event, view: ?-is->view: %event, ext: ?-is->ext: %event )
		do >:$< file://Edit.hcn ></>
		/
	else in ( : Save-is->action: %event, view: ?-is->view: %event, ext: ?-is->ext: %event )
		do >< file://%view.%ext >: $\[ ?-is->contents: %event ] |
		then
			do >:$< file://%view.hcn ></>
			/
	else in entity: ?-is->entity: %event
		in : Add-is->action: %event do
			: instance < ?-is->entity: %event
			: entity : !! %instance
			/
			then
				do >:$< file://Entity.hcn ></>
				/
		else in : Remove-is->action: %event do
			: instance < ?-is->entity: %event
			!~ %instance
			/
			then
				do >:$< file://Session.hcn ></>
				/
		else in : Link-is->action: %event do
			: source < %entity
			: medium < ?-is->via: %event
			: target < ?-is->to: %event
			: entity : !! %source-%medium->%target
			/
			then
				do >:$< file://Entity.hcn ></>
				/
		else
			do >:$< file://Entity.hcn ></>
			/
		/
	else
		do >:$< file://Session.hcn ></>
		/
	/
/
