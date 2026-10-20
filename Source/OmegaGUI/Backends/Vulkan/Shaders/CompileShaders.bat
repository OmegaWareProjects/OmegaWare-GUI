@echo off
setlocal
cd /d "%~dp0"
set GLSLANG=%VULKAN_SDK%\Bin\glslangValidator.exe
"%GLSLANG%" -V --vn g_aUIVertexSPIRV -o UIVertex.hpp UI.vert || exit /b 1
"%GLSLANG%" -V --vn g_aWorldVertexSPIRV -o WorldVertex.hpp World.vert || exit /b 1
"%GLSLANG%" -V --vn g_aUIFragmentSPIRV -o UIFragment.hpp UI.frag || exit /b 1
