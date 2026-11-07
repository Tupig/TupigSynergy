# SPDX-License-Identifier: MIT
# Windows OS APIs only. Unversioned icuuc.dll is the Windows 10 ICU C API.
# VC runtime, Qt, OpenSSL, third-party ICU and D3D compiler DLLs deliberately
# are not system exemptions, even when installed on the runner.
set(SYNERGY_WINDOWS_SYSTEM_DLLS
  advapi32.dll authz.dll avrt.dll bcrypt.dll bcryptprimitives.dll cfgmgr32.dll clbcatq.dll
  comctl32.dll comdlg32.dll crypt32.dll cryptbase.dll cryptsp.dll d2d1.dll
  d3d9.dll d3d11.dll d3d12.dll dbghelp.dll dhcpcsvc.dll dnsapi.dll dwmapi.dll
  dwrite.dll dxcore.dll dxgi.dll gdi32.dll glu32.dll hid.dll icuuc.dll imagehlp.dll imm32.dll iphlpapi.dll
  hvsifiletrust.dll kernel32.dll kernelbase.dll mpr.dll msimg32.dll msvcrt.dll ncrypt.dll netapi32.dll
  normaliz.dll ntasn1.dll ntdll.dll ole32.dll oleacc.dll oleaut32.dll opengl32.dll
  pathcch.dll powrprof.dll propsys.dll psapi.dll rpcrt4.dll runtimeobject.dll
  secur32.dll setupapi.dll shcore.dll shell32.dll shlwapi.dll user32.dll
  userenv.dll usp10.dll uxtheme.dll version.dll win32u.dll winhttp.dll wininet.dll
  winmm.dll winscard.dll winspool.drv wintrust.dll wldap32.dll ws2_32.dll
  wtsapi32.dll ucrtbase.dll xinput1_4.dll)

function(synergy_windows_is_system name result)
  string(TOLOWER "${name}" _name)
  if(_name MATCHES "^(api-ms-|ext-ms-)" OR _name IN_LIST SYNERGY_WINDOWS_SYSTEM_DLLS)
    set(${result} TRUE PARENT_SCOPE)
  else()
    set(${result} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(synergy_windows_imports binary dumpbin result)
  # /IMPORTS explicitly includes ordinary and delay-loaded DLL imports.
  execute_process(COMMAND "${dumpbin}" /IMPORTS "${binary}"
    RESULT_VARIABLE _status OUTPUT_VARIABLE _imports ERROR_VARIABLE _error)
  if(NOT _status EQUAL 0)
    message(FATAL_ERROR "Cannot inspect imports: ${binary}: ${_error}")
  endif()
  string(REPLACE "\r" "" _imports "${_imports}")
  string(REPLACE "\n" ";" _lines "${_imports}")
  set(_names "")
  foreach(_line IN LISTS _lines)
    string(STRIP "${_line}" _name)
    string(TOLOWER "${_name}" _lower)
    if(_lower MATCHES "^[a-z0-9_.+-]+\\.(dll|drv)$")
      list(APPEND _names "${_name}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _names)
  set(${result} "${_names}" PARENT_SCOPE)
endfunction()
