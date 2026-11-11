@echo off
setlocal
rem Explicit selected SDK, version, cache, notice output and workflow output.
python "%~dp0collect-qt-notices.py" --sdk-root "%~1" --version "%~2" --cache "%~3" --output "%~4" --github-output "%~5"
exit /b %errorlevel%
