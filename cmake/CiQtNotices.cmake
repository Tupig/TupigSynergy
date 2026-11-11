# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

# Only the explicit prebuilt CI provider gets this source-notice supplement.
# Local manifest builds retain vcpkg's own license provenance.
function(synergy_configure_ci_qt_notices sdk version)
  get_filename_component(CI_QT_SOURCE_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  set(SYNERGY_CI_QT_NOTICES "${SYNERGY_CI_QT_NOTICES}" CACHE PATH
    "Verified source notices for the selected prebuilt CI Qt SDK")
  set(SYNERGY_CI_QT_NOTICES_SHA256 "${SYNERGY_CI_QT_NOTICES_SHA256}" CACHE STRING
    "Original manifest SHA256 recorded by the CI Qt notice collector")
  set(CI_QT_NOTICES_ROOT "${SYNERGY_CI_QT_NOTICES}")
  set(CI_QT_NOTICES_SHA256 "${SYNERGY_CI_QT_NOTICES_SHA256}")
  set(CI_QT_SDK "${sdk}")
  set(CI_QT_VERSION "${version}")
  if(CI_QT_NOTICES_ROOT)
    if(NOT "$ENV{GITHUB_ACTIONS}" STREQUAL "true" OR NOT QT_IS_SHARED)
      message(FATAL_ERROR "Qt source notice supplements are restricted to shared prebuilt CI providers")
    endif()
    string(LENGTH "${CI_QT_NOTICES_SHA256}" digest_length)
    if(NOT digest_length EQUAL 64 OR CI_QT_NOTICES_SHA256 MATCHES "[^0-9a-f]")
      message(FATAL_ERROR "CI Qt notices require the original collection manifest SHA256")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CI_QT_SOURCE_DIR}/scripts/ci/collect-qt-notices.py"
      --sdk-root "${sdk}" --version "${version}" --output "${CI_QT_NOTICES_ROOT}"
      --verify --manifest-sha256 "${CI_QT_NOTICES_SHA256}"
      RESULT_VARIABLE checked OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT checked EQUAL 0)
      message(FATAL_ERROR "Selected CI Qt notice verification failed: ${output}${error}")
    endif()
  elseif(CI_QT_NOTICES_SHA256)
    message(FATAL_ERROR "CI Qt notice SHA256 was supplied without a notice root")
  endif()
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CiQtNotices.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ci-qt-notices.cmake" @ONLY)
endfunction()
