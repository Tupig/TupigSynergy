@echo off
setlocal EnableExtensions EnableDelayedExpansion
if not defined RUNNER_TEMP exit /b 1
if not defined GITHUB_PATH exit /b 1
set "size="
set "hash="
set "cmakepath="
set "cpackpath="
set "url=https://files.pythonhosted.org/packages/8b/99/6600831fe0b1305d88494feaff5e995a5988332dd478b12457f92a79daf8/cmake-4.3.4-py3-none-win_amd64.whl"
set "root=%RUNNER_TEMP%\cmake-4.3.4"
set "wheel=%RUNNER_TEMP%\cmake-4.3.4.whl"
if exist "%root%" rmdir /s /q "%root%" || exit /b 1
if exist "%wheel%" del /f /q "%wheel%" || exit /b 1
curl.exe --fail --location --retry 2 --output "%wheel%" "%url%" || exit /b 1
for %%I in ("%wheel%") do set "size=%%~zI"
if not "!size!"=="41282263" exit /b 1
for /f "delims=" %%H in ('python -c "import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],\"rb\").read()).hexdigest())" "%wheel%"') do set "hash=%%H"
if /i not "!hash!"=="1ab14fad08f03e85a1fc837f18e12f72acb215c44eff91231dbe7f871cb5112e" exit /b 1
mkdir "%root%" || exit /b 1
tar.exe -xf "%wheel%" -C "%root%" || exit /b 1
set "toolbin=%root%\cmake\data\bin"
if not exist "%toolbin%\cmake.exe" exit /b 1
if not exist "%toolbin%\cpack.exe" exit /b 1
set "PATH=%toolbin%;%PATH%"
for /f "delims=" %%P in ('where.exe cmake') do if not defined cmakepath set "cmakepath=%%P"
for /f "delims=" %%P in ('where.exe cpack') do if not defined cpackpath set "cpackpath=%%P"
if /i not "!cmakepath!"=="%toolbin%\cmake.exe" exit /b 1
if /i not "!cpackpath!"=="%toolbin%\cpack.exe" exit /b 1
"%toolbin%\cmake.exe" --version || exit /b 1
"%toolbin%\cpack.exe" --version || exit /b 1
"%toolbin%\cmake.exe" --version | findstr /x /c:"cmake version 4.3.4" >nul || exit /b 1
"%toolbin%\cpack.exe" --version | findstr /x /c:"cpack version 4.3.4" >nul || exit /b 1
echo Verified cmake=%toolbin%\cmake.exe
echo Verified cpack=%toolbin%\cpack.exe
echo %toolbin%>>"%GITHUB_PATH%" || exit /b 1
endlocal
exit /b 0
