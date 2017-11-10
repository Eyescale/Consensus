# Copyright (c) Stefan Eilemann <eile@eyescale.ch>

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(Consensus VERSION 0.1.0)
set(Consensus_VERSION_ABI 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
include(Extra)

extra_application(Consensus SOURCES
	api.c
	cgic.c
	command.c
	command_util.c
	database.c
	expression.c
	expression_filter.c
	expression_solve.c
	expression_util.c
	frame.c
	hcn.c
	input.c
	input_util.c
	io.c
	kernel.c
	main.c
	narrative.c
	narrative_util.c
	output.c
	output_util.c
	path.c
	registry.c
	value.c
	variable.c
	variable_util.c
)
