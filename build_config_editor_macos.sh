#!/bin/bash
# Convenience script to build the Config Editor for macOS from the root picoSignals directory
# Usage: ./build_config_editor_macos.sh

cd "$(dirname "$0")/ConfigEditor"

if [ ! -f "build_macos.sh" ]; then
    echo "Error: ConfigEditor/build_macos.sh not found"
    exit 1
fi

chmod +x build_macos.sh
./build_macos.sh
