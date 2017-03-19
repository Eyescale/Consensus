
	!! monitor()
		on init do
			>:	monitor() >>>>> init done
			/.
		on e_new: . !! do
			>:	monitor() >>>>> new entities: %e_new
			/.
		on e_activated: . !* do
			>:	monitor() >>>>> entities activated: %e_activated
			/.
		on e_deactivated: . !_ do
			>:	monitor() >>>>> entities deactivated: %e_deactivated
			/.
		on e_released: . !~ do
			>:	monitor() >>>>> entities released: %e_released
			>:	monitor() >>>>> entities remaining: %[ . ]
			/.
		/
	!* monitor()
