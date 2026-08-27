#!/bin/bash
# Build script with icon embedding for Windows

set -e

echo "Building RodLegacy with embedded icon..."

# Check if we're cross-compiling for Windows
if [ "$1" = "windows" ]; then
    echo "Cross-compiling for Windows with icon embedding..."

    # Convert PNG to ICO
    echo "Converting icon.png to icon.ico..."
    python3 scripts/convert_icon.py data/res/icon.png data/res/icon.ico

    # Build with meson
    meson setup builddir --cross-file /project/cross-mingw.txt
    cd builddir
    ninja
    echo "Build complete! Check builddir/src/open_sr.exe"
else
    echo "Building for current platform..."
    meson setup builddir
    cd builddir
    ninja
    echo "Build complete!"
fi
