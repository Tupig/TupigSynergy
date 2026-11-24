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
if /i "%GITHUB_ACTIONS%"=="true" (
  if not defined SYNERGY_CI_CMAKE exit /b 1
  if not exist "%SYNERGY_CI_CMAKE%" exit /b 1
  python -c "import subprocess,sys; sys.exit(0 if subprocess.check_output([sys.argv[1],'--version'],text=True).splitlines()[0]=='cmake version 4.3.4' else 1)" "%SYNERGY_CI_CMAKE%" || exit /b 1
  "%SYNERGY_CI_CMAKE%" "-DEXPECTED_ARCH=%PACKAGE_EXPECTED_ARCH%" "-DBUILD_DIR=%PACKAGE_BUILD%" -P "%~dp0verify-package-windows.cmake"
) else (
  cmake "-DEXPECTED_ARCH=%PACKAGE_EXPECTED_ARCH%" "-DBUILD_DIR=%PACKAGE_BUILD%" -P "%~dp0verify-package-windows.cmake"
)
exit /b %ERRORLEVEL%
