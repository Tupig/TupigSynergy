# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

create_test(
  NAME ServerProxyTests
  DEPENDS client
  LIBS base arch io mt net
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

# These two executables share applicationDirPath()/settings on Windows. The
# process fixture additionally takes a filesystem lock across ctest invocations.
set_tests_properties(ClientTests ServerProxyTests PROPERTIES
  RESOURCE_LOCK issue270-client-portable-settings
)
foreach(client_test_target IN ITEMS ClientTests ServerProxyTests)
  target_compile_definitions(${client_test_target} PRIVATE
    CLIENT_TEST_EXECUTABLE_DIRECTORY="$<TARGET_FILE_DIR:${client_test_target}>"
  )
endforeach()
