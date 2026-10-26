@echo off
setlocal
set "FLY_VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%FLY_VCVARS%" set "FLY_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
call "%FLY_VCVARS%"
if errorlevel 1 exit /b 1
cd /d "%~dp0\..\.."
set "FLY_CMAKE=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "FLY_NINJA=%VSINSTALLDIR%Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
"%FLY_CMAKE%" -S . -B out/build/release -G Ninja "-DCMAKE_MAKE_PROGRAM=%FLY_NINJA%" -DCMAKE_BUILD_TYPE=Release -DELYSIA_ENABLE_IMGUI=ON -DELYSIA_ENABLE_ASAN=OFF -DBUILD_TESTING=ON
if errorlevel 1 exit /b 1
"%FLY_CMAKE%" --build out/build/release --target FlyMe2TheMoon fly_development_overlay_tests fly_visual_tests fly_selection_tests fly_game_tests fly_story_tests fly_mechanism_tests fly_music_tests --parallel 8
exit /b %errorlevel%
