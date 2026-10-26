@echo off
REM ---------------------------------------------------------------------------
REM  Unisoc Dump Tool - Windows build helper
REM
REM  Usage:
REM     build.bat            Configure + build Release with Visual Studio 2022
REM     build.bat vs2019     Use Visual Studio 2019
REM     build.bat ninja      Use Ninja (needs ninja + a C compiler on PATH)
REM     build.bat tests      Build and run the core tests only
REM ---------------------------------------------------------------------------
setlocal

set BUILD_DIR=build
set MODE=%1

if "%MODE%"=="tests" (
    cmake -S . -B %BUILD_DIR% || goto :err
    cmake --build %BUILD_DIR% --target udt_tests || goto :err
    ctest --test-dir %BUILD_DIR% --output-on-failure
    goto :eof
)

if "%MODE%"=="ninja" (
    cmake -S . -B %BUILD_DIR% -G "Ninja" -DCMAKE_BUILD_TYPE=Release || goto :err
    cmake --build %BUILD_DIR% || goto :err
    goto :done
)

set GEN=Visual Studio 17 2022
if "%MODE%"=="vs2019" set GEN=Visual Studio 16 2019

cmake -S . -B %BUILD_DIR% -G "%GEN%" -A x64 || goto :err
cmake --build %BUILD_DIR% --config Release || goto :err

:done
echo.
echo Build complete. Executable:
echo    %BUILD_DIR%\Release\UnisocDumpTool.exe  (VS generators)
echo    %BUILD_DIR%\UnisocDumpTool.exe          (Ninja/Make generators)
goto :eof

:err
echo.
echo Build failed. Ensure CMake and a C toolchain are installed and on PATH.
exit /b 1
