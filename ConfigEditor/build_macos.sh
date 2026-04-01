#!/bin/bash
# Build script for macOS executable
# Usage: ./build_macos.sh

set -e

echo "======================================"
echo "PicoSignals Config Editor - macOS Build"
echo "======================================"
echo ""

# Check if PyInstaller is installed
if ! command -v pyinstaller &> /dev/null; then
    echo "Error: PyInstaller is not installed"
    echo "Install it with: pip install pyinstaller"
    exit 1
fi

# Create build directory
mkdir -p dist build

# Build the macOS app bundle
echo "Building macOS app bundle..."
pyinstaller config_editor_macos.spec

# Check if build was successful
if [ -d "dist/PicoSignals Config Editor.app" ]; then
    echo ""
    echo "✓ Build successful!"
    echo ""
    echo "Output location:"
    echo "  dist/PicoSignals Config Editor.app"
    echo ""
    echo "To code-sign the app (optional):"
    echo "  codesign -s - 'dist/PicoSignals Config Editor.app'"
    echo ""
    echo "To run the app:"
    echo "  open 'dist/PicoSignals Config Editor.app'"
else
    echo "✗ Build failed"
    exit 1
fi
