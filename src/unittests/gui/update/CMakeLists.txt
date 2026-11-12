# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

create_test(
  NAME UpdateVersionTests
  DEPENDS gui
  SOURCE UpdateVersionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui/update"
)

create_test(
  NAME UpdateCheckerTests
  DEPENDS gui
  SOURCE UpdateCheckerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui/update"
)

create_test(
  NAME UpdateDownloaderTests
  DEPENDS gui
  SOURCE UpdateDownloaderTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui/update"
)
