# Copyright (c) Stefan Eilemann <eile@eyescale.ch>

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(Consensus VERSION 0.1.0)
set(Consensus_VERSION_ABI 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
include(Extra)

extra_application(Consensus SOURCES
	expression_solve.c
  api.c
  command.c
  database.c
  expression_solve.c
  expression_util.c
  expression.c
  filter_util.c
  frame.c
  hcn.c
  input.c
  kernel.c
  main.c
  narrative_util.c
  narrative.c
  output.c
  registry.c
  string_util.c
  value.c
  variables.c
)
