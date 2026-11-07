@echo off
setlocal
rem Verify extracted MSI and portable payloads without installing the service.
set "PACKAGE_BUILD=%~1"
if not defined PACKAGE_BUILD set "PACKAGE_BUILD=%~dp0..\..\build"
cmake "-DBUILD_DIR=%PACKAGE_BUILD%" -P "%~dp0verify-package-windows.cmake"
exit /b %ERRORLEVEL%
