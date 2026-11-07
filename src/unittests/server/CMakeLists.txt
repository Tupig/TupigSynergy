# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  set(extra_libs version app mt net)
endif()

create_test(
  NAME ServerConfigTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE ServerConfigTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME ServerTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE ServerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME ConfigReadTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE ConfigReadTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME InputFilterTests
  DEPENDS server
  LIBS base arch ${extra_libs}
  SOURCE InputFilterTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

create_test(
  NAME ClientProxyTests
  DEPENDS server
  LIBS base arch io ${extra_libs}
  SOURCE ClientProxyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)
set_tests_properties(ClientProxyTests PROPERTIES TIMEOUT 30)

create_test(
  NAME KeyInfoOwnershipTests
  DEPENDS server
  LIBS base arch mt ${extra_libs}
  SOURCE KeyInfoOwnershipTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)
set_tests_properties(KeyInfoOwnershipTests PROPERTIES TIMEOUT 15)

# Each real-Screen fixture owns its Windows portable settings marker.
foreach(test ServerTests ClientProxyTests KeyInfoOwnershipTests)
  set_target_properties(${test} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${test}-runtime")
endforeach()
