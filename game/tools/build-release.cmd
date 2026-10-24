@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0\..\.."
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DELYSIA_ENABLE_IMGUI=OFF
if errorlevel 1 exit /b 1
cmake --build out/build/release --target FlyMe2TheMoon --parallel 8
exit /b %errorlevel%
