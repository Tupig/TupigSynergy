@echo off
setlocal DisableDelayedExpansion
rem Native WiX fixture intake only. No installer or custom action is executed.
if "%~1"=="" (
  echo Usage: test-msi64-intake.bat x64^|arm64 [fresh-run-name]
  exit /b 2
)
set "MSI_INTAKE_ARCH=%~1"
set "MSI_INTAKE_RUN=%~2"
if not defined MSI_INTAKE_RUN set "MSI_INTAKE_RUN=run01"
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake is required by the existing native CI toolchain.
  exit /b 2
)
cmake "-DEXPECTED_ARCH=%MSI_INTAKE_ARCH%" "-DRUN_ID=%MSI_INTAKE_RUN%" -P "%~dp0testdata\msi64-intake\intake.cmake"
exit /b %ERRORLEVEL%
