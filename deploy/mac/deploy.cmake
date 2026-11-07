# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OSX_BUNDLE ${BUILD_OSX_BUNDLE})

set(OS_STRING "macos-${BUILD_ARCHITECTURE}")

if (OSX_BUNDLE)
  # Resolve third-party runtime dependencies for both GUI and core. Qt's
  # deployment tool supplies dynamically loaded plugins; BundleUtilities then
  # closes the non-system dependency graph, also when Qt itself is static.
  set(MAC_DEPENDENCY_DIRS "")
  set(MAC_OPENSSL_ROOT "")
  foreach(library IN ITEMS "${OPENSSL_SSL_LIBRARY}" "${OPENSSL_CRYPTO_LIBRARY}")
    if(IS_ABSOLUTE "${library}")
      get_filename_component(library_dir "${library}" DIRECTORY)
      list(APPEND MAC_DEPENDENCY_DIRS "${library_dir}")
      get_filename_component(real_library "${library}" REALPATH)
      get_filename_component(real_library_dir "${real_library}" DIRECTORY)
      get_filename_component(MAC_OPENSSL_ROOT "${real_library_dir}" DIRECTORY)
    endif()
  endforeach()
  list(REMOVE_DUPLICATES MAC_DEPENDENCY_DIRS)
  set(MAC_QT_ROOT "")
  foreach(property IN ITEMS IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_RELWITHDEBINFO IMPORTED_LOCATION)
    get_target_property(qt_location Qt${QT_VERSION_MAJOR}::Core ${property})
    if(qt_location AND qt_location MATCHES "^(.*)/lib/.*")
      set(MAC_QT_ROOT "${CMAKE_MATCH_1}")
      break()
    endif()
  endforeach()
  if(QT_IS_SHARED)
    find_program(DEPLOYQT NAMES macdeployqt HINTS "${MAC_QT_ROOT}/bin")
  endif()
  # Qt's prebuilt Qt 6 OpenSSL plugin resolves ABI 3 with QLibrary, so this
  # dependency is invisible to otool/fixup_bundle when Qt is not openssl-linked.
  # A provider is used only while packaging; installed apps load their own copy.
  set(MAC_QT_SSL_ROOT "")
  set(qt_ssl_roots "${MAC_OPENSSL_ROOT}" "${MAC_QT_ROOT}")
  if(QT_IS_SHARED AND QT_VERSION_MAJOR EQUAL 6)
    # Homebrew is an explicit CI-provider exception, never a local manifest
    # fallback. Local builds must obtain the matching runtime from their SDK.
    if("$ENV{GITHUB_ACTIONS}" STREQUAL "true")
      find_program(MAC_BREW_EXECUTABLE NAMES brew)
    endif()
    if("$ENV{GITHUB_ACTIONS}" STREQUAL "true" AND MAC_BREW_EXECUTABLE)
      execute_process(COMMAND "${MAC_BREW_EXECUTABLE}" --prefix openssl@3
        RESULT_VARIABLE brew_result OUTPUT_VARIABLE brew_ssl_root
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
      if(brew_result EQUAL 0)
        list(APPEND qt_ssl_roots "${brew_ssl_root}")
      endif()
    endif()
    foreach(root IN LISTS qt_ssl_roots)
      if(EXISTS "${root}/lib/libssl.3.dylib" AND EXISTS "${root}/lib/libcrypto.3.dylib")
        get_filename_component(MAC_QT_SSL_ROOT "${root}" REALPATH)
        break()
      endif()
    endforeach()
  endif()
  set(MAC_VCPKG_SHARE "")
  if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
    set(MAC_VCPKG_SHARE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
  endif()
  configure_file(
    ${CMAKE_SOURCE_DIR}/deploy/mac/deploy-bundle.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mac-deploy-bundle.cmake
    @ONLY
  )
  install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/mac-deploy-bundle.cmake)

  # Dependency fixups receive temporary ad-hoc signatures. Sign the completed
  # bundle only after every install() has populated its resources, immediately
  # before CPack builds the DMG. Use Developer ID when configured; otherwise
  # refresh the ad-hoc signature required by unsigned Apple Silicon snapshots.
  configure_file(
    ${CMAKE_SOURCE_DIR}/deploy/mac/codesign.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/mac-codesign.cmake
    @ONLY
  )
  set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/mac-codesign.cmake)

  set(CPACK_PACKAGE_ICON "${MY_DIR}/dmg-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/deploy/mac/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/deploy/mac/synergy-eula.txt")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
