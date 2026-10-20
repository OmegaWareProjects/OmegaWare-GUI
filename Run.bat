@echo off
setlocal
cd /d "%~dp0"
set BACKEND=%~1
set MODE=%~2
set ARCH=%~3
if "%BACKEND%"=="" set BACKEND=dx11
if "%MODE%"=="" set MODE=release
if "%ARCH%"=="" set ARCH=x64
set EXE=Build\%MODE%\%ARCH%\OmegaGUI-Demo.exe
if not exist "%EXE%" call "%~dp0Build.bat" %MODE% %ARCH% || exit /b 1
start "" "%EXE%" --backend=%BACKEND%
