
	!! $.cgi()
	on init
		in ~: operator
			do >: $< file://OOOO.hcn ></>
			/
		else in ~: ?-is->session
			do >: $< file://Welcome.hcn ></>
			/
		else in : Join-is->action
			do >@ operator: !! < session:%session > |
			then
				do >@ %session:\< cgi >: %[ .is. ] | >:
				/
		else in : Leave-is->action
			do >@ %[ ?-is->session ]:\< cgi >: %[ .is. ] |
			then
				do >: $< file://Welcome.hcn ></>
				/
		else
			do >@ %session:\< cgi >: %[ .is. ] | >:
			/
		/
	then
		do exit
		/

