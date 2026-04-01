#!/usr/bin/env python3
"""
Convenience script to build the Config Editor from the root picoSignals directory.
This script changes to the ConfigEditor directory and runs the build script there.

Usage:
    python3 build_config_editor.py [--platform macos|windows|both]
"""

import os
import sys
import subprocess

def main():
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_editor_dir = os.path.join(script_dir, 'ConfigEditor')
    
    # Check if ConfigEditor directory exists
    if not os.path.isdir(config_editor_dir):
        print(f"Error: ConfigEditor directory not found at {config_editor_dir}")
        sys.exit(1)
    
    # Check if build.py exists
    build_script = os.path.join(config_editor_dir, 'build.py')
    if not os.path.exists(build_script):
        print(f"Error: build.py not found at {build_script}")
        sys.exit(1)
    
    # Change to ConfigEditor directory
    os.chdir(config_editor_dir)
    
    # Run build.py with the same arguments
    try:
        cmd = [sys.executable, 'build.py'] + sys.argv[1:]
        result = subprocess.run(cmd)
        sys.exit(result.returncode)
    except Exception as e:
        print(f"Error running build script: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
