# SPDX-License-Identifier: MIT

if(WIN32)
  add_executable(synergy-uninstall WIN32 WindowsUninstall.cpp)
  set_target_properties(synergy-uninstall PROPERTIES OUTPUT_NAME uninstall)
  target_compile_definitions(synergy-uninstall PRIVATE
    UNICODE _UNICODE "SYNERGY_WINDOWS_UPGRADE_CODE=L\"{${SYNERGY_WINDOWS_UPGRADE_GUID}}\"")
  target_link_libraries(synergy-uninstall PRIVATE msi user32)
  if(MINGW)
    target_link_options(synergy-uninstall PRIVATE -municode)
  endif()
  install(TARGETS synergy-uninstall RUNTIME DESTINATION .)
endif()
