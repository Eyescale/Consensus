# Copyright (c) Stefan Eilemann <eile@eyescale.ch>

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(Consensus VERSION 0.1.0)
set(Consensus_VERSION_ABI 1)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)
include(Extra)

extra_application(Consensus SOURCES
    Pilot/api.c
    Pilot/cgic.c
    Pilot/command.c
    Pilot/command_util.c
    Pilot/database.c
    Pilot/expression.c
    Pilot/expression_sieve.c
    Pilot/expression_solve.c
    Pilot/expression_util.c
    Pilot/frame.c
    Pilot/list.c
    Pilot/hcn.c
    Pilot/input.c
    Pilot/input_util.c
    Pilot/io.c
    Pilot/kernel.c
    Pilot/main.c
    Pilot/narrative.c
    Pilot/narrative_util.c
    Pilot/output.c
    Pilot/output_util.c
    Pilot/path.c
    Pilot/registry.c
    Pilot/string_util.c
    Pilot/value.c
    Pilot/variable.c
    Pilot/variable_util.c
    Pilot/xl.c
)
target_include_directories(Consensus PRIVATE Pilot/include)
