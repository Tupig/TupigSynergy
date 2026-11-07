# SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <https://github.com/Tupig/Tupig_synergy/issues>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})

# App-local official VC redistributable files give MSI and portable archives the
# same dependency closure. Every flavor, including development, must be usable.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
set(_crt_dlls "")
if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
  include(InstallRequiredSystemLibraries)
  set(_crt_dlls "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
  if(NOT _crt_dlls)
    string(TOLOWER "${BUILD_ARCHITECTURE}" _crt_arch)
    if(_crt_arch MATCHES "^(x86_64|amd64)$")
      set(_crt_arch x64)
    elseif(_crt_arch STREQUAL "aarch64")
      set(_crt_arch arm64)
    endif()
    set(_crt_dirs "")
    foreach(_redist_root IN ITEMS "$ENV{VCToolsRedistDir}" "$ENV{VCINSTALLDIR}/Redist/MSVC")
      if(_redist_root AND IS_DIRECTORY "${_redist_root}")
        file(GLOB_RECURSE _redist_files "${_redist_root}/*.dll")
        foreach(_file IN LISTS _redist_files)
          string(TOLOWER "${_file}" _lower)
          if(_lower MATCHES "/${_crt_arch}/microsoft\\.vc[0-9]+\\.crt/[^/]+\\.dll$" AND
              NOT _lower MATCHES "(debug|spectre|onecore)")
            get_filename_component(_directory "${_file}" DIRECTORY)
            list(APPEND _crt_dirs "${_directory}")
          endif()
        endforeach()
      endif()
    endforeach()
    if(_crt_dirs)
      list(REMOVE_DUPLICATES _crt_dirs)
      list(SORT _crt_dirs COMPARE NATURAL ORDER DESCENDING)
      list(GET _crt_dirs 0 _crt_directory)
      file(GLOB _crt_dlls "${_crt_directory}/*.dll")
    endif()
  endif()
  if(NOT _crt_dlls)
    message(FATAL_ERROR "Dynamic CRT packaging requires official VC redistributable DLLs for this architecture")
  endif()
  install(FILES ${_crt_dlls} DESTINATION "${CMAKE_INSTALL_BINDIR}")
else()
  message(STATUS "Static CRT: no VC redistributable payload is needed")
endif()

# Paths used only at packaging time. They never enter installed configuration.
set(WINDOWS_PACKAGE_SEARCH_DIRS "${OPENSSL_EXE_DIR};${OPENSSL_ROOT_DIR}/bin")
foreach(_crt_dll IN LISTS _crt_dlls)
  get_filename_component(_crt_dir "${_crt_dll}" DIRECTORY)
  list(APPEND WINDOWS_PACKAGE_SEARCH_DIRS "${_crt_dir}")
endforeach()
set(WINDOWS_QT_PREFIX "")
foreach(_location_property IN ITEMS IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_RELWITHDEBINFO IMPORTED_LOCATION)
  get_target_property(_qt_core Qt${QT_VERSION_MAJOR}::Core "${_location_property}")
  if(_qt_core AND EXISTS "${_qt_core}")
    get_filename_component(_qt_bin "${_qt_core}" DIRECTORY)
    get_filename_component(WINDOWS_QT_PREFIX "${_qt_bin}/.." ABSOLUTE)
    list(APPEND WINDOWS_PACKAGE_SEARCH_DIRS "${_qt_bin}")
    break()
  endif()
endforeach()
if(QT_IS_SHARED)
  find_program(DEPLOYQT NAMES windeployqt HINTS "${WINDOWS_QT_PREFIX}/bin")
endif()
set(WINDOWS_VCPKG_SHARE "")
if(VCPKG_INSTALLED_DIR AND VCPKG_TARGET_TRIPLET)
  set(WINDOWS_VCPKG_SHARE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
endif()
configure_file(${MY_DIR}/pre-cpack.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack.cmake @ONLY)
set(CPACK_PRE_BUILD_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/pre-cpack.cmake)
configure_file(${MY_DIR}/cpack-options.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/cpack-options.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/cpack-options.cmake)

set(OS_STRING "win-${BUILD_ARCHITECTURE}")

list(APPEND CPACK_GENERATOR "7Z")

# If WiX 4+ is installed make a package. CPack drives the WiX v4 toolchain
# schema; CI installs wix 5.0.2 which still accepts v4 documents. Keep this
# in step with .github/actions/install-dependencies/action.yml (A-23 (#47)).
find_program(WIX_APP wix)
if(WIX_APP)
  set(CPACK_WIX_VERSION 4)
  set(CPACK_WIX_ARCHITECTURE ${BUILD_ARCHITECTURE})
  list(APPEND CPACK_GENERATOR "WIX")
endif()

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_PROPER_NAME}")

# Menu Entry
set(CPACK_WIX_PROGRAM_MENU_FOLDER "${CMAKE_PROJECT_PROPER_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "${SYNERGY_GUI_EXE_NAME}" "${CMAKE_PROJECT_PROPER_NAME}")

# Default Install Path
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_PROJECT_PROPER_NAME}")

# Wix Specific Values
set(CPACK_WIX_UPGRADE_GUID "027D1C8A-E7A5-4754-BB93-B2D45BFDBDC8")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/deploy/windows/wix-banner.png")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/deploy/windows/wix-dialog.png")

# Required Extra Extenstions
list(APPEND CPACK_WIX_EXTENSIONS "WixToolset.Util.wixext" "WixToolset.Firewall.wixext")

# Make sure to also put the xmlns for the ext into the wix block on generated files
list(APPEND CPACK_WIX_CUSTOM_XMLNS "util=http://wixtoolset.org/schemas/v4/wxs/util" "firewall=http://wixtoolset.org/schemas/v4/wxs/firewall")

# Preserve MSI service and firewall registration.
configure_file(
  ${MY_DIR}/wix-patch.xml.in
  ${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml @ONLY
)

# This patch sets up firewall rules and the service.
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_BINARY_DIR}/wix-patch.xml")
