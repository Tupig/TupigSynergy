# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

create_test(
  NAME ServerProxyTests
  DEPENDS client
  LIBS base arch io mt
  SOURCE ServerProxyTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)
create_test(
  NAME ClientTests
  DEPENDS client
  LIBS base arch io mt net
  SOURCE ClientTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/client"
)

# Isolate the Windows portable settings marker from other test executables.
set_target_properties(ClientTests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/ClientTests-runtime")
