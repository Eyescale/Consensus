
Name
	./Examples/System

Overview
	The purpose of Consensus is to allow to create Systems of information which are
	sensitive, that is: which subscribe to and rely upon each other's changes
	to inform their own states, as opposed to trying to change the other Systems'
	states to have them align with and preserve their own.

	The example source code under the ./Examples/System directory illustrates how
	such systems can be architected using linear programming languages such as C.

Usage
	./system

Execution
	. The program will read anything from stdin and
		  1. output "hello, world\n" upon carriage return
		  2. exit upon '/' followed by carriage return

Description
	. Implements CNSystem execution model and interfaces - including
		. Subsystem class definition and interfaces
	  	. CNIn, CNOn, CNDo API - in include/system.h
	. Implements input and parser subsystems, executing in parallel

Contents
	. main.c
		System definition - CNSystemBegin() / CNSystemEnd()
		. declares Operator system together with parser and input subsystems

		Subsystem definition - via subsystems ID and init() method
			. invokes each subsystem's init() method
			. init() assigns other subsystem's methods

		System execution - SystemFrame()
		. invokes each subsystem's frame() method
			. allows subsytems to subscribe to each other data changes
			. allows each subsystem to process changes from last frame
		. invokes each subsystem's sync() method
			. allows each subsystem to map update notifications into
			  its own worldview and log changes for the next frame
		. swaps the back and front buffers of each subsystem
			. these buffers hold the subsystem's change information, ie.
			  the memory addresses which were accessed via CNDo

		System exit - SystemFrame()
		. SystemFrame() returns 0 when all subsystems' frontbuffer are empty
		  ie. when no subsystem has any event to process
		. otherwise, SystemFrame() returns 1

	. input.c
	  parser.c
		Subsystem implementations - data and methods
		. SubsystemInit() - allocate subsystem-specific data and methods
		. SubsystemFrame() - processes changes from last frame
		. SubsystemSync() - map external change notifications into own
		  Subsystem's worldview and log local changes for the next frame

	. include/input.h
	  include/parser.h
		. Subsystem manifests - system interface and data structures
		  access to data structure is needed for other subsystems to subscribe

	./operator.c
	./include/operator.h
		. System interface definition
			. Subsystem class definition
			. CNIn, CNOn, CNDo implementation - uses macros to simplify
Dependencies
	./Base/libcn.so
		. Consensus library

