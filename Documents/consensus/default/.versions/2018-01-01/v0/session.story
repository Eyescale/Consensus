
	!! $.story()
	on event < cgi >: . !!
		in ( : Edit-is->action: %event, view: ?-is->view: %event )
			do >:$< file://Edit.hcn ></>
			/
		else in : edit-is->mode : %event
			in ( : Leave-is->action : %event, view: ?-is->view: %event )
				do >:$< file://%view.hcn ></>
				/
			else in view: ?-is->view: %event
				do >< file://%view.hcn >: $\[ ?-is->contents: %event ] |
				then
					do >:$< file://%view.hcn ></>
					/
			/
		else in ( : Home-is->view: %event, : Leave-is->action: %event )
			do exit
			/
		else
			do >:$< file://Home.hcn ></>
			/
		/
	/
