# SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
# SPDX-License-Identifier: MIT

add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(update)

# R3 contract checks preserve R2 target/evidence lineage.
if(WIN32)
  set(extra_libs version)
endif()
foreach(r3_test R3OptionContractTests R3SerializationTests R3RuntimeContextTests R3VisualContractTests R3TrayQuitTests)
  create_test(NAME ${r3_test} DEPENDS gui LIBS ${extra_libs}
    SOURCE ${r3_test}.cpp WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui")
  target_include_directories(${r3_test} PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
  if(TARGET app_translations)
    add_dependencies(${r3_test} app_translations)
    add_custom_command(TARGET ${r3_test} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${r3_test}>/translations"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
        "$<TARGET_FILE_DIR:${r3_test}>/translations" COMMAND_EXPAND_LISTS VERBATIM)
  endif()
  if(UNIX AND NOT APPLE)
    set_property(TEST ${r3_test} APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  endif()
endforeach()

create_test(NAME R3MotionContractTests DEPENDS gui LIBS ${extra_libs}
  SOURCE R3MotionContractTests.cpp WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui")
target_include_directories(R3MotionContractTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
if(UNIX AND NOT APPLE)
  set_property(TEST R3MotionContractTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Same production main source and real hooks; only this target enables probe callbacks.
# No installation rule and no production activation option or Info.plist override.
if(BUILD_GUI)
  add_executable(R3StartupProbe EXCLUDE_FROM_ALL MACOSX_BUNDLE
    ${CMAKE_SOURCE_DIR}/src/apps/synergy-gui/synergy-gui.cpp
    R3StartupProbeBootstrap.cpp
    ${CMAKE_SOURCE_DIR}/src/apps/res/branding/synergy.qrc)
  target_compile_definitions(R3StartupProbe PRIVATE SYNERGY_EXTRA_HEADER SYNERGY_STARTUP_PROBE)
  target_include_directories(R3StartupProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/lib)
  target_link_libraries(R3StartupProbe PRIVATE gui common Qt6::Core Qt6::Widgets Qt6::Network ${extra_libs})
  set_target_properties(R3StartupProbe PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.tupig.synergy.r3-startup-probe"
    MACOSX_BUNDLE_BUNDLE_NAME "TuPig Synergy R3 Startup Probe")
  create_test(NAME R3StartupContractTests DEPENDS gui LIBS synergy-gui
    SOURCE R3StartupContractTests.cpp WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui")
  target_compile_definitions(R3StartupContractTests PRIVATE R3_STARTUP_PROBE_PATH="$<TARGET_FILE:R3StartupProbe>")
  add_dependencies(R3StartupContractTests R3StartupProbe)
  # COMPLETE-R1 test payload completeness; auto-core remains disabled by probe.
  add_dependencies(R3StartupProbe ${CMAKE_PROJECT_NAME}-core)
  add_custom_command(TARGET R3StartupProbe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:${CMAKE_PROJECT_NAME}-core>"
      "$<TARGET_FILE_DIR:R3StartupProbe>/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}-core>"
    VERBATIM)

  if(TARGET app_translations)
    add_dependencies(R3StartupProbe app_translations)
    add_custom_command(TARGET R3StartupProbe POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:R3StartupProbe>/translations"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
        "$<TARGET_FILE_DIR:R3StartupProbe>/translations" COMMAND_EXPAND_LISTS VERBATIM)
  endif()
endif()

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
  NAME NativeDesignTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE NativeDesignTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(NativeDesignTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
target_compile_definitions(NativeDesignTests PRIVATE UI_TEST_TRANSLATIONS_DIR="${CMAKE_BINARY_DIR}/translations")
if(TARGET app_translations)
  add_dependencies(NativeDesignTests app_translations)
  add_custom_command(TARGET NativeDesignTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NativeDesignTests>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
      "$<TARGET_FILE_DIR:NativeDesignTests>/translations"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
endif()
if(UNIX AND NOT APPLE)
  set_property(TEST NativeDesignTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

create_test(
  NAME LayoutAcceptanceTests
  DEPENDS gui
  LIBS ${extra_libs}
  SOURCE LayoutAcceptanceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(LayoutAcceptanceTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib/gui)
target_compile_definitions(LayoutAcceptanceTests PRIVATE UI_TEST_TRANSLATIONS_DIR="${CMAKE_BINARY_DIR}/translations")
if(TARGET app_translations)
  add_dependencies(LayoutAcceptanceTests app_translations)
  add_custom_command(TARGET LayoutAcceptanceTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:LayoutAcceptanceTests>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
      "$<TARGET_FILE_DIR:LayoutAcceptanceTests>/translations"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
endif()
if(UNIX AND NOT APPLE)
  set_property(TEST LayoutAcceptanceTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
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

# Explicit native observation harness; never part of default builds or CTest.
# Additive executable checks for accepted M-C01/M-C05/M-C06. The core is copied
# only to satisfy constructor payload checks; the fixture blocks all core paths.
create_test(NAME MainWindowFlowTests DEPENDS gui LIBS ${extra_libs}
  SOURCE MainWindowFlowTests.cpp WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui")
target_include_directories(MainWindowFlowTests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib ${CMAKE_SOURCE_DIR}/src/lib/gui)
target_sources(MainWindowFlowTests PRIVATE ${CMAKE_SOURCE_DIR}/src/apps/res/branding/synergy.qrc)
add_dependencies(MainWindowFlowTests ${CMAKE_PROJECT_NAME}-core)
add_custom_command(TARGET MainWindowFlowTests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${CMAKE_PROJECT_NAME}-core>"
    "$<TARGET_FILE_DIR:MainWindowFlowTests>/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}-core>"
  VERBATIM)
if(TARGET app_translations)
  add_dependencies(MainWindowFlowTests app_translations)
  add_custom_command(TARGET MainWindowFlowTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MainWindowFlowTests>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
      "$<TARGET_FILE_DIR:MainWindowFlowTests>/translations" COMMAND_EXPAND_LISTS VERBATIM)
endif()
if(UNIX AND NOT APPLE)
  set_property(TEST MainWindowFlowTests APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
# Six window-case groups accumulate on Windows; per-case limits stay unchanged.
if(WIN32)
  set_tests_properties(MainWindowFlowTests PROPERTIES TIMEOUT 75)
else()
  set_tests_properties(MainWindowFlowTests PROPERTIES TIMEOUT 45)
endif()

add_executable(R2UiPreview EXCLUDE_FROM_ALL MACOSX_BUNDLE
  R2UiPreview.cpp
  ${CMAKE_SOURCE_DIR}/src/apps/res/branding/synergy.qrc
)
target_link_libraries(R2UiPreview PRIVATE gui common Qt6::Core Qt6::Widgets Qt6::Network ${extra_libs})
target_include_directories(R2UiPreview PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
add_dependencies(R2UiPreview ${CMAKE_PROJECT_NAME}-core)
add_custom_command(TARGET R2UiPreview POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${CMAKE_PROJECT_NAME}-core>"
    "$<TARGET_FILE_DIR:R2UiPreview>/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}-core>"
  VERBATIM
)
set_target_properties(R2UiPreview PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.tupig.synergy.r2-ui-preview"
  MACOSX_BUNDLE_BUNDLE_NAME "TuPig Synergy R2 Preview"
  MACOSX_BUNDLE_INFO_STRING "Isolated native UI observation harness"
)
if(APPLE)
  target_link_options(R2UiPreview PRIVATE "-Wl,-map,${CMAKE_CURRENT_BINARY_DIR}/R2UiPreview-link.map")
endif()
if(TARGET app_translations)
  add_dependencies(R2UiPreview app_translations)
  add_custom_command(TARGET R2UiPreview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:R2UiPreview>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
      "$<TARGET_FILE_DIR:R2UiPreview>/translations"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
endif()

# R4 observes the same production main/hooks and the already reviewed isolated
# bootstrap. No alternative appearance setup or product activation option.
if(BUILD_GUI)
  add_executable(R4VisualProbe MACOSX_BUNDLE
    ${CMAKE_SOURCE_DIR}/src/apps/synergy-gui/synergy-gui.cpp
    R3StartupProbeBootstrap.cpp R4VisualContractTests.cpp
    ${CMAKE_SOURCE_DIR}/src/apps/res/branding/synergy.qrc)
  target_compile_definitions(R4VisualProbe PRIVATE SYNERGY_EXTRA_HEADER SYNERGY_STARTUP_PROBE SYNERGY_R4_VISUAL_PROBE
    R4_VISUAL_RESULT_PATH="${CMAKE_BINARY_DIR}/src/unittests/test-results/R4VisualContractTests.txt")
  target_include_directories(R4VisualProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/lib ${CMAKE_SOURCE_DIR}/src/lib/gui)
  target_link_libraries(R4VisualProbe PRIVATE gui common Qt6::Core Qt6::Widgets Qt6::Network Qt6::Test ${extra_libs})
  set_target_properties(R4VisualProbe PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.tupig.synergy.r4-visual-probe"
    MACOSX_BUNDLE_BUNDLE_NAME "TuPig Synergy R4 Visual Probe")
  add_dependencies(R4VisualProbe ${CMAKE_PROJECT_NAME}-core)
  add_custom_command(TARGET R4VisualProbe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:${CMAKE_PROJECT_NAME}-core>" "$<TARGET_FILE_DIR:R4VisualProbe>/$<TARGET_FILE_NAME:${CMAKE_PROJECT_NAME}-core>" VERBATIM)
  if(TARGET app_translations)
    add_dependencies(R4VisualProbe app_translations)
    add_custom_command(TARGET R4VisualProbe POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:R4VisualProbe>/translations"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SYNERGY_TRANSLATION_FILES}
        "$<TARGET_FILE_DIR:R4VisualProbe>/translations" COMMAND_EXPAND_LISTS VERBATIM)
  endif()
  add_test(NAME R4VisualContractTests COMMAND $<TARGET_FILE:R4VisualProbe>)
  set_tests_properties(R4VisualContractTests PROPERTIES ENVIRONMENT "SYNERGY_R4_AUTOMATED=1" TIMEOUT 240
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui")
  set_property(GLOBAL APPEND PROPERTY ${CMAKE_PROJECT_NAME}_tests R4VisualContractTests)
endif()

# Bound hangs with per-target receipts rather than an unbounded native CI step.
set_tests_properties(R3OptionContractTests R3SerializationTests R3RuntimeContextTests
  R3MotionContractTests R3VisualContractTests R3TrayQuitTests PROPERTIES TIMEOUT 45)
if(TARGET R3StartupContractTests)
  set_tests_properties(R3StartupContractTests PROPERTIES TIMEOUT 75)
endif()
set_tests_properties(PageHostTests EditorTransactionTests NativeDesignTests LayoutAcceptanceTests PROPERTIES TIMEOUT 60)
