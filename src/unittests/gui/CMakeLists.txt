# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(update)

# Settings/SecureUtils 使用 arch 的 Windows 版本资源辅助函数。
if(WIN32)
  set(extra_libs version)
endif()

create_test(
  NAME PageHostTests
  DEPENDS gui
  SOURCE PageHostTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
# macOS 静态 Qt 仅包含 cocoa；测试不显示窗口，也不发送宿主输入。
if(UNIX AND NOT APPLE)
  set_property(TEST PageHostTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

create_test(
  NAME DialogRenderTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE DialogRenderTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(DialogRenderTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
if(UNIX AND NOT APPLE)
  set_property(TEST DialogRenderTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

create_test(
  NAME EditorTransactionTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE EditorTransactionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(EditorTransactionTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
if(UNIX AND NOT APPLE)
  set_property(TEST EditorTransactionTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

create_test(
  NAME LoggerTests
  DEPENDS gui
  SOURCE LoggerTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME KeySequenceTests
  DEPENDS gui
  SOURCE KeySequenceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ScreenSetupModelTests
  DEPENDS gui
  SOURCE ScreenSetupModelTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME ValidatorTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE ValidatorTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

create_test(
  NAME TlsUtilityTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE TlsUtilityTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# The migration overlay lives in the synergy-gui sublibrary, which only
# exists when the GUI is built.
if(BUILD_GUI)
  create_test(
    NAME SettingsMigrationTests
    DEPENDS gui
    LIBS synergy-gui
    SOURCE SettingsMigrationTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
  )
  target_include_directories(SettingsMigrationTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
endif()
