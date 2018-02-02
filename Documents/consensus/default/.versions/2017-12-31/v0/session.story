
	!! $.story()
	on event < cgi >: . !!
		in : Welcome-is->view : %event
			do >:$< file://Home.hcn ></>
			/
		else in : Home-is->view : %event
			in : Leave-is->action : %event
				do exit
				/
			else in : Edit-is->action : %event
				do >:$< file://Edit.hcn ></>
				/
			/
		else in : Edit-is->view : %event
			in : Leave-is->action : %event
				do >:$< file://Home.hcn ></>
				/
			else do
				!~ ?-is->oneliner
				!! .-is->oneliner: %event
				!~ ?-is->description
				!! .-is->description: %event
				/
				then
					do >:$< file://Home.hcn ></>
					/
			/
		/
	/
