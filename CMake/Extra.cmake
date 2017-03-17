# Copyright (c) BBP/EPFL 2017
#                        Stefan.Eilemann@epfl.ch

include(CMakePackageConfigHelpers)
include(CMakeParseArguments)
include(GenerateExportHeader)

cmake_policy(SET CMP0020 NEW) # Automatically link Qt executables to qtmain target on Windows.
cmake_policy(SET CMP0037 NEW) # Target names should not be reserved and should match a validity pattern.
cmake_policy(SET CMP0038 NEW) # Targets may not link directly to themselves.
cmake_policy(SET CMP0048 NEW) # The project() command manages VERSION variables.
cmake_policy(SET CMP0054 OLD) # Only interpret if() arguments as variables or keywords when unquoted.

# WAR for CMake >=3.1 bug (observed with 3.2.3)
# If not set to false, any call to pkg_check_modules() or pkg_search_module()
# ERASES the $ENV{PKG_CONFIG_PATH}, so subsequent calls may fail to locate
# a dependency which depends on this variable (e.g. in FindPoppler.cmake).
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # -fPIC
set(CMAKE_INSTALL_MESSAGE LAZY) # no up-to-date messages on installation
set(CMAKE_CXX_STANDARD_REQUIRED ON) # value of CXX_STANDARD on targets is required
set_property(GLOBAL PROPERTY USE_FOLDERS ON) # organize targets into folders
enable_testing()

function(extra_library Name)
  set(_opts)
  set(_singleArgs)
  set(_multiArgs PUBLIC_HEADERS HEADERS SOURCES PUBLIC_LIBRARIES PRIVATE_LIBRARIES)
  cmake_parse_arguments(THIS "${_opts}" "${_singleArgs}" "${_multiArgs}"
    ${ARGN})
  foreach(_multiArg ${_multiArgs})
    if(THIS_${_multiArg})
      list(SORT THIS_${_multiArg})
    endif()
  endforeach()

  string(TOLOWER ${Name} name)
  string(TOUPPER ${Name} NAME)

  if(THIS_SOURCES)
    add_library(${Name} SHARED
      ${THIS_SOURCES} ${THIS_HEADERS} ${THIS_PUBLIC_HEADERS})
    set_target_properties(${Name} PROPERTIES
      VERSION ${${PROJECT_NAME}_VERSION}
      SOVERSION ${${PROJECT_NAME}_VERSION_ABI}
      OUTPUT_NAME ${Name} FOLDER ${PROJECT_NAME})
    target_link_libraries(${Name}
      PUBLIC ${THIS_PUBLIC_LIBRARIES} PRIVATE ${THIS_PRIVATE_LIBRARIES})
    generate_export_header(${Name}
      EXPORT_MACRO_NAME ${NAME}_API EXPORT_FILE_NAME api.h)
    target_include_directories(${Name} PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")
      set_property(TARGET ${Name} PROPERTY CXX_STANDARD 11)
  else()
    add_library(${Name} INTERFACE)
  endif()

  install(TARGETS ${Name}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION lib COMPONENT dev
    RUNTIME DESTINATION bin COMPONENT lib
    LIBRARY DESTINATION lib COMPONENT lib
    INCLUDES DESTINATION include)
  foreach(_header ${THIS_PUBLIC_HEADERS})
    string(REGEX MATCH "(.*)[/\\]" _dir ${_header})
    install(FILES ${_header} DESTINATION include/${name}/${_dir})
  endforeach()
endfunction()

function(extra_test Name)
  if(NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY)
    return()
  endif()
  if(NOT WIN32) # tests want to be with DLLs on Windows - no rpath support
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  set(_opts)
  set(_singleArgs)
  set(_multiArgs SOURCES LIBRARIES)
  cmake_parse_arguments(THIS "${_opts}" "${_singleArgs}" "${_multiArgs}"
    ${ARGN})
  foreach(_multiArg ${_multiArgs})
    if(THIS_${_multiArg})
      list(SORT THIS_${_multiArg})
    endif()
  endforeach()

  add_executable(${Name} ${THIS_SOURCES})
  target_link_libraries(${Name} ${THIS_LIBRARIES}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
  target_include_directories(${Name} PRIVATE
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
    BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
  target_compile_definitions(${Name} PRIVATE -DBOOST_TEST_DYN_LINK)
  set_property(TARGET ${Name} PROPERTY CXX_STANDARD 11)

  add_test(NAME ${Name}
    COMMAND $<TARGET_FILE:${Name}>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

function(extra_application Name)
  set(_opts)
  set(_singleArgs)
  set(_multiArgs SOURCES LIBRARIES)
  cmake_parse_arguments(THIS "${_opts}" "${_singleArgs}" "${_multiArgs}"
    ${ARGN})
  foreach(_multiArg ${_multiArgs})
    if(THIS_${_multiArg})
      list(SORT THIS_${_multiArg})
    endif()
  endforeach()

    add_executable(${Name} ${THIS_SOURCES})
    target_link_libraries(${Name} ${THIS_LIBRARIES})
    target_include_directories(${Name} PRIVATE
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")
    set_property(TARGET ${Name} PROPERTY CXX_STANDARD 11)

  install(TARGETS ${Name} DESTINATION bin)
endfunction()

function(extra_test Name)
  if(NOT Boost_UNIT_TEST_FRAMEWORK_LIBRARY)
    return()
  endif()
  if(NOT WIN32) # tests want to be with DLLs on Windows - no rpath support
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()

  set(_opts)
  set(_singleArgs)
  set(_multiArgs SOURCES LIBRARIES)
  cmake_parse_arguments(THIS "${_opts}" "${_singleArgs}" "${_multiArgs}"
    ${ARGN})
  foreach(_multiArg ${_multiArgs})
    if(THIS_${_multiArg})
      list(SORT THIS_${_multiArg})
    endif()
  endforeach()

  add_executable(${Name} ${THIS_SOURCES})
  target_link_libraries(${Name} ${THIS_LIBRARIES}
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
  target_include_directories(${Name} PRIVATE
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
    BEFORE SYSTEM ${Boost_INCLUDE_DIRS})
  target_compile_definitions(${Name} PRIVATE -DBOOST_TEST_DYN_LINK)
  set_property(TARGET ${Name} PROPERTY CXX_STANDARD 11)

  add_test(NAME ${Name}
    COMMAND $<TARGET_FILE:${Name}>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

function(extra_export_project)
  if(MSVC)
    set(CMAKE_MODULE_INSTALL_PATH ${PROJECT_NAME}/CMake)
  else()
    set(CMAKE_MODULE_INSTALL_PATH share/${PROJECT_NAME}/CMake)
  endif()

  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion)

  file(WRITE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.in
    "if(NOT TARGET @PROJECT_NAME@)
      include(${PROJECT_BINARY_DIR}/@PROJECT_NAME@Targets.cmake)
    endif()")

  configure_package_config_file(
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_MODULE_INSTALL_PATH})

  install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
          DESTINATION ${CMAKE_MODULE_INSTALL_PATH})

  export(EXPORT ${PROJECT_NAME}Targets
    FILE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake")

  install(EXPORT ${PROJECT_NAME}Targets FILE ${PROJECT_NAME}Targets.cmake
          DESTINATION ${CMAKE_MODULE_INSTALL_PATH})
endfunction()
