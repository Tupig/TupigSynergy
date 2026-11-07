# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

# Resolve host tools at configure time; none becomes an installed dependency.
find_program(SYNERGY_PATCHELF patchelf REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SYNERGY_PRIVATE_DIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(SYNERGY_RUNTIME_TARGETS ${CMAKE_PROJECT_NAME}-core)
if(TARGET ${CMAKE_PROJECT_NAME})
  list(APPEND SYNERGY_RUNTIME_TARGETS ${CMAKE_PROJECT_NAME})
endif()
set(SYNERGY_RUNTIME_FILES "")
foreach(target IN LISTS SYNERGY_RUNTIME_TARGETS)
  # Installed ELF files are relocated below by the bundler. This also prevents
  # automatic incorporation of build-machine search directories.
  set_target_properties(${target} PROPERTIES
    INSTALL_RPATH "$ORIGIN/lib" INSTALL_RPATH_USE_LINK_PATH FALSE)
  list(APPEND SYNERGY_RUNTIME_FILES "$<TARGET_FILE:${target}>")
endforeach()

set(SYNERGY_QT_PLUGIN_DIR "")
if(QT_IS_SHARED)
  get_target_property(_qmake Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
  if(NOT _qmake OR NOT EXISTS "${_qmake}")
    message(FATAL_ERROR "Shared Qt packaging requires its imported qmake host tool")
  endif()
  execute_process(COMMAND "${_qmake}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE SYNERGY_QT_PLUGIN_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _qt_query)
  if(NOT _qt_query EQUAL 0 OR NOT IS_DIRECTORY "${SYNERGY_QT_PLUGIN_DIR}")
    message(FATAL_ERROR "Cannot locate the selected Qt runtime plugins")
  endif()
endif()

set(SYNERGY_EXTRA_LICENSE_DIRS "" CACHE STRING
  "License directories for source-built dependencies not owned by the system package manager")
set(SYNERGY_SOURCE_LICENSE_MAP "" CACHE STRING
  "Exact source-built library SONAME=license-directory pairs")
configure_file("${CMAKE_CURRENT_LIST_DIR}/bundle-runtime.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/bundle-runtime-configured.cmake" @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/bundle-runtime-$<CONFIG>.cmake"
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/bundle-runtime-configured.cmake")
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/bundle-runtime-$<CONFIG>.cmake")
