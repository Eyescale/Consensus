
	!! $.cgi()
	on init
		in ~: operator
			do >: $< file://OOOO.hcn ></>
			/
		else in session: ?-is->session
			in : Join-is->action
				do >@ operator: !! < session:%session > |
				then
					do >@ %session:\< cgi >: %[ .is. ] | >:
					/
			else in : Leave-is->action
				do >@ %session:\< cgi >: %[ .is. ] |
				then
					do >: $< file://Welcome.hcn ></>
					/
			else
				do >@ %session:\< cgi >: %[ .is. ] | >:
				/
			/
		else
			do >: $< file://Welcome.hcn ></>
			/
		/
	then
		do exit
		/

