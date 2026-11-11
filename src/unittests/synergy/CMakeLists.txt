# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  set(extra_libs platform version)
endif()

create_test(
  NAME ClipboardTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE ClipboardTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME ClipboardChunksTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE ClipboardChunksTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME ProtocolUtilTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE ProtocolUtilTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME ProtocolUtilAllocationTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE ProtocolUtilAllocationTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)
set_tests_properties(ProtocolUtilAllocationTests PROPERTIES TIMEOUT 10)

# Manual Release measurement only: never gate CTest on noisy wall-clock timings.
add_executable(ProtocolUtilBenchmark EXCLUDE_FROM_ALL ProtocolUtilBenchmark.cpp)
target_link_libraries(ProtocolUtilBenchmark PRIVATE app arch base io ${extra_libs} Qt6::Core)
target_compile_definitions(ProtocolUtilBenchmark PRIVATE
  SYNERGY_BENCHMARK_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
  SYNERGY_BENCHMARK_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}"
)

create_test(
  NAME PacketStreamFilterTests
  DEPENDS app
  LIBS arch base io mt ${extra_libs}
  SOURCE PacketStreamFilterTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME IKeyStateTests
  DEPENDS app
  LIBS arch base ${extra_libs}
  SOURCE IKeyStateTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME KeyMapTests
  DEPENDS app
  LIBS arch base ${extra_libs}
  SOURCE KeyMapTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME KeyStateTests
  DEPENDS app
  LIBS arch base mt ${extra_libs}
  SOURCE KeyStateTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME KeyboardLayoutManagerTests
  DEPENDS app
  LIBS arch base ${extra_libs}
  SOURCE KeyboardLayoutManagerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME InputValidatorTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE InputValidatorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileChunkTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileChunkTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileTransferPathTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileTransferPathTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileTransferReceiverTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileTransferReceiverTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileTransferSourceTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileTransferSourceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileTransferSenderTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileTransferSenderTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME FileTransferOutboundTests
  DEPENDS app
  LIBS arch base io ${extra_libs}
  SOURCE FileTransferOutboundTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

create_test(
  NAME IpcServerTests
  DEPENDS app
  LIBS arch base io mt ${extra_libs}
  SOURCE IpcServerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
)

if(BUILD_X11_SUPPORT)
  create_test(
    NAME X11LayoutParserTests
    DEPENDS app
    LIBS arch base ${extra_libs}
    SOURCE X11LayoutParserTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/synergy"
  )
endif()

