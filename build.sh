#!/bin/bash

# VideoMaster Build Script
# Requires: Qt6, CMake, FFmpeg development libraries

set -e

echo "Building VideoMaster..."

# Check if build directory exists
if [ -d "build" ]; then
    echo "Removing existing build directory..."
    rm -rf build
fi

# Create build directory
mkdir build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build the project
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "Executable: ./VideoMaster"
echo ""
echo "To run the application:"
echo "cd build && ./VideoMaster"