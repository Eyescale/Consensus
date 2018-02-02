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
	else in ( : View-is->action: %event, entity: ?-is->entity: %event )
		do >:$< file://Entity.hcn ></>
		/
	else in ( : Link-is->action: %event, entity: ?-is->entity: %event )
		do
			: via : ?-is->via: %event
			: to : ?-is->to: %event
			: entity : !! %entity-%via->%to
			>:$< file://Entity.hcn ></>
			/.
		/
	else in : Session-is->view: %event
		in ( : Add-is->entityAction: %event, entity: ?-is->entity: %event )
			do !! ?-is->entity: %event
			then
				do >:$< file://Entity.hcn ></>
				/
		else in : Remove-is->entityAction: %event
			do !~ ?-is->entity: %event
			then
				do >:$< file://Session.hcn ></>
				/
		else
			do >:$< file://Session.hcn ></>
			/
		/
	else
		do >:$< file://Session.hcn ></>
		/
	/
/
