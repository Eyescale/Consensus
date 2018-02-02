
	!! $.story()
	on event < cgi >: . !!
		in ( : Edit-is->action: %event, view: ?-is->view: %event )
			do >:$< file://Edit.hcn ></>
			/
		else in ( : Save-is->action: %event, view: ?-is->view: %event )
			do >< file://%view.hcn >: $\[ ?-is->contents: %event ] |
			then
				do >:$< file://%view.hcn ></>
				/
		else in : Home-is->view: %event
			in : Leave-is->action: %event
				do exit
				/
			else in : Add-is->entityAction: %event
				do !! ?-is->entity: %event
				then
					do >:$< file://Home.hcn ></>
					/
			else in : Remove-is->entityAction: %event
				do !~ ?-is->entity: %event
				then
					do >:$< file://Home.hcn ></>
					/
			else
				do >:$< file://Home.hcn ></>
				/
			/
		else
			do >:$< file://Home.hcn ></>
			/
		/
	/
