# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

if(WIN32)
  set(extra_libs version)
endif()

create_test(
  NAME SecureUtilsTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE SecureUtilsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)

create_test(
  NAME FingerprintTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE FingerprintTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)

create_test(
  NAME FingerprintDatabaseTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE FingerprintDatabaseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)

create_test(
  NAME QtSocketFactoryTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE QtSocketFactoryTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)

create_test(
  NAME QtNetworkTransportTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE QtNetworkTransportTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)
set_tests_properties(QtNetworkTransportTests PROPERTIES TIMEOUT 30)

create_test(
  NAME NetworkAddressTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE NetworkAddressTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)

create_test(
  NAME SecureSocketTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE SecureSocketTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)
set_tests_properties(SecureSocketTests PROPERTIES TIMEOUT 120)

create_test(
  NAME SocketMultiplexerTests
  DEPENDS net
  LIBS base arch mt io ${extra_libs}
  SOURCE SocketMultiplexerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/net"
)
set_tests_properties(SocketMultiplexerTests PROPERTIES TIMEOUT 30)
