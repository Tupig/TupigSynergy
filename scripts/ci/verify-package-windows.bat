@echo off
setlocal
rem Verify extracted MSI and portable payloads without installing the service.
set "PACKAGE_BUILD=%~1"
if not defined PACKAGE_BUILD set "PACKAGE_BUILD=%~dp0..\..\build"
set "PACKAGE_EXPECTED_ARCH=%~2"
if not defined PACKAGE_EXPECTED_ARCH (
  echo ERROR: expected target architecture required: x64 or arm64.
  exit /b 1
)
cmake "-DEXPECTED_ARCH=%PACKAGE_EXPECTED_ARCH%" "-DBUILD_DIR=%PACKAGE_BUILD%" -P "%~dp0verify-package-windows.cmake"
exit /b %ERRORLEVEL%
