# SPDX-FileCopyrightText: (C) 2026 TuPig Synergy Contributors
# SPDX-License-Identifier: MIT

# LPic stores big-endian 16-bit fields: default region, count, then
# (region, resource ID minus 5000, two-byte flag) for each language.
# CPack writes flag 0 for every language. Keep English (0, 0, 0) and
# write Simplified Chinese as (52, 1, 1) so macOS decodes its menu and text.
foreach(package IN LISTS CPACK_PACKAGE_FILES)
  if(package MATCHES "[.]dmg$")
    execute_process(
      COMMAND /usr/bin/hdiutil udifrez -xml
        "${CMAKE_CURRENT_LIST_DIR}/dmg-license-languages.plist" unused "${package}"
      COMMAND_ERROR_IS_FATAL ANY
    )
  endif()
endforeach()
