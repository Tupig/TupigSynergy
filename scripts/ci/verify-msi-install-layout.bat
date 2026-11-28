@echo off
rem SPDX-License-Identifier: MIT
setlocal EnableExtensions DisableDelayedExpansion

rem Run only on a disposable Windows test host with a trusted build artifact.
if /i not "%~1"=="--disposable" goto :usage
if "%~4"=="" goto :usage
if not "%~5"=="" goto :usage
set "package_dir=%~f2"
set "install_root=%~f3"
set "evidence=%~f4"
set "system_bin=%SystemRoot%\System32"
set "service_name=TuPig Synergy"
set "arp=HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
set "package="
for %%F in ("%package_dir%\*.msi") do if exist "%%~fF" (
    if defined package goto :usage
    set "package=%%~fF"
)
if not defined package goto :usage
if exist "%install_root%" goto :unsafe_host
if exist "%evidence%" goto :unsafe_host
"%system_bin%\sc.exe" query "%service_name%" >nul 2>&1
if not "%ERRORLEVEL%"=="1060" goto :unsafe_host
"%system_bin%\reg.exe" query "%arp%" /s /f "%service_name%" /d /e /reg:64 >nul 2>&1
if "%ERRORLEVEL%"=="0" goto :unsafe_host
"%system_bin%\fltmc.exe" >nul 2>&1
if errorlevel 1 goto :unsafe_host

mkdir "%evidence%" || exit /b 1
start "" /wait "%system_bin%\msiexec.exe" /i "%package%" INSTALL_ROOT="%install_root%" /qn /norestart /L*v "%evidence%\install.log"
if errorlevel 1 goto :failed
for %%F in (synergy.exe synergy-core.exe synergy-daemon.exe uninstall.exe) do (
    if not exist "%install_root%\bin\%%F" goto :failed
)
if exist "%install_root%\synergy.exe" goto :failed
if not exist "%install_root%\licenses\LICENSE" goto :failed
"%system_bin%\sc.exe" qc "%service_name%" > "%evidence%\service.txt" || goto :failed
"%system_bin%\findstr.exe" /i /l /c:"%install_root%\bin\synergy-daemon.exe" "%evidence%\service.txt" >nul || goto :failed
"%system_bin%\sc.exe" query "%service_name%" > "%evidence%\service-state.txt" || goto :failed
"%system_bin%\findstr.exe" /r /c:": *4 " "%evidence%\service-state.txt" >nul || goto :failed
"%system_bin%\reg.exe" query "%arp%" /s /f "%install_root%\" /d /e /reg:64 > "%evidence%\registered-location.txt" || goto :failed
"%system_bin%\findstr.exe" /i /l /c:"InstallLocation" "%evidence%\registered-location.txt" >nul || goto :failed

call :remove_msi || goto :failed
"%system_bin%\sc.exe" query "%service_name%" > "%evidence%\service-removed.txt" 2>&1
if not "%ERRORLEVEL%"=="1060" goto :failed
if exist "%install_root%\bin\synergy.exe" goto :failed
if exist "%install_root%\bin\uninstall.exe" goto :failed

echo PASS: custom MSI path, registered location, service startup and MSI removal.
exit /b 0

:remove_msi
start "" /wait "%system_bin%\msiexec.exe" /x "%package%" /qn /norestart /L*v "%evidence%\uninstall.log"
exit /b %ERRORLEVEL%

:failed
echo ERROR: MSI layout or lifecycle check failed. Logs: "%evidence%"
start "" /wait "%system_bin%\msiexec.exe" /x "%package%" /qn /norestart /L*v "%evidence%\recovery.log"
exit /b 1

:unsafe_host
echo ERROR: Use a fresh, elevated disposable Windows test host and unused directories.
exit /b 1

:usage
echo Usage: %~nx0 --disposable package-directory custom-install-directory evidence-directory
exit /b 2
