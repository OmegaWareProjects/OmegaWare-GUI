@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"
set MODE=%~1
set ARCH=%~2
if "%MODE%"=="" set MODE=debug
if "%ARCH%"=="" set ARCH=x64
set EXE=Build\%MODE%\%ARCH%\OmegaGUI-Demo.exe
if not exist "%EXE%" call "%~dp0Build.bat" %MODE% %ARCH% || exit /b 1
set FAILED=0
for %%B in (dx9 dx10 dx11 dx12 opengl vulkan) do (
	if not exist "Build\SelfTest\%%B" mkdir "Build\SelfTest\%%B"
	"%EXE%" --backend=%%B --selftest --output=Build\SelfTest\%%B
	if errorlevel 1 (
		echo [FAIL] %%B
		set FAILED=1
	) else (
		echo [pass] %%B
	)
)
exit /b !FAILED!
