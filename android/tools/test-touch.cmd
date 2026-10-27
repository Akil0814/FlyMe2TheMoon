@echo off
call "%~1\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /I"%~dp0..\.." "%~dp0..\tests\touch_pointer_tests.cpp" /Fo"%~2\touch_pointer_tests.obj" /Fe"%~2\touch_pointer_tests.exe"
if errorlevel 1 exit /b 1
"%~2\touch_pointer_tests.exe"
