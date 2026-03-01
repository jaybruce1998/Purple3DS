@echo off
REM 3DS Build Script for Pokemon Purple (Windows)
REM This script builds the 3DS version of the game

echo 🔨 Building Pokemon Purple 3DS...
echo ==================================

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ❌ Error: CMakeLists.txt not found. Please run from the nds directory.
    pause
    exit /b 1
)

REM Create build directory if it doesn't exist
if not exist "build" (
    echo 📁 Creating build directory...
    mkdir build
)

REM Navigate to build directory
cd build

REM Configure with CMake
echo ⚙️  Configuring project...
cmake .. -DCMAKE_BUILD_TYPE=Release

REM Build the project
echo 🔨 Building project...
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✅ Build successful!
    echo 📦 Output files:
    echo    - main_3ds.elf ^(executable^)
    echo    - main_3ds.3dsx ^(3DSX file for Citra^)
    if exist "main_3ds.smdh" (
        echo    - main_3ds.smdh ^(SMDH icon^)
    )
    echo.
    echo 🚀 Ready to run on 3DS or Citra!
) else (
    echo.
    echo ❌ Build failed!
    pause
    exit /b 1
)