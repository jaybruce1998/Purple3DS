#!/bin/bash

# 3DS Build Script for Pokemon Purple
# This script builds the 3DS version of the game

set -e  # Exit on any error

echo "🔨 Building Pokemon Purple 3DS..."
echo "=================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: CMakeLists.txt not found. Please run from the nds directory."
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "📁 Creating build directory..."
    mkdir build
fi

# Navigate to build directory
cd build

# Configure with CMake
echo "⚙️  Configuring project..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
echo "🔨 Building project..."
cmake --build . --config Release

# Check if build was successful
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo "📦 Output files:"
    echo "   - main_3ds.elf (executable)"
    echo "   - main_3ds.3dsx (3DSX file for Citra)"
    if [ -f "main_3ds.smdh" ]; then
        echo "   - main_3ds.smdh (SMDH icon)"
    fi
    echo ""
    echo "🚀 Ready to run on 3DS or Citra!"
else
    echo ""
    echo "❌ Build failed!"
    exit 1
fi
