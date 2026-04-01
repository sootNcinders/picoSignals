#!/usr/bin/env python3
"""
Cross-platform build script for PicoSignals Configuration Editor
Works on both macOS and Windows
Usage: python3 build.py [--platform macos|windows|both]
"""

import os
import sys
import subprocess
import shutil
import platform

def get_platform():
    """Get the current platform"""
    system = platform.system()
    if system == "Darwin":
        return "macos"
    elif system == "Windows":
        return "windows"
    else:
        return "linux"

def check_pyinstaller():
    """Check if PyInstaller is installed"""
    try:
        import PyInstaller
        return True
    except ImportError:
        return False

def build_macos():
    """Build for macOS"""
    print("\n" + "="*50)
    print("Building for macOS...")
    print("="*50)
    
    if not os.path.exists("config_editor_macos.spec"):
        print("Error: config_editor_macos.spec not found")
        return False
    
    try:
        result = subprocess.run(
            ["pyinstaller", "config_editor_macos.spec"],
            check=True
        )
        
        if os.path.exists("dist/PicoSignals Config Editor.app"):
            print("\n✓ macOS build successful!")
            print("Output: dist/PicoSignals Config Editor.app")
            return True
        else:
            print("\n✗ Build completed but output not found")
            return False
            
    except subprocess.CalledProcessError as e:
        print(f"\n✗ Build failed: {e}")
        return False

def build_windows():
    """Build for Windows"""
    print("\n" + "="*50)
    print("Building for Windows...")
    print("="*50)
    
    if not os.path.exists("config_editor_windows.spec"):
        print("Error: config_editor_windows.spec not found")
        return False
    
    try:
        result = subprocess.run(
            ["pyinstaller", "config_editor_windows.spec"],
            check=True
        )
        
        if os.path.exists("dist/config_editor/config_editor.exe"):
            print("\n✓ Windows build successful!")
            print("Output: dist/config_editor/config_editor.exe")
            return True
        else:
            print("\n✗ Build completed but output not found")
            return False
            
    except subprocess.CalledProcessError as e:
        print(f"\n✗ Build failed: {e}")
        return False

def cleanup():
    """Clean up build artifacts"""
    print("\nCleaning up build artifacts...")
    if os.path.exists("build"):
        shutil.rmtree("build")
    if os.path.exists("*.pyc"):
        os.remove("*.pyc")
    print("✓ Cleanup complete")

def main():
    print("="*50)
    print("PicoSignals Configuration Editor - Build Tool")
    print("="*50)
    
    # Check PyInstaller
    if not check_pyinstaller():
        print("\nError: PyInstaller is not installed")
        print("Install with: pip install pyinstaller")
        sys.exit(1)
    
    # Parse arguments
    platform_choice = "current"
    if len(sys.argv) > 1:
        if sys.argv[1] == "--platform" and len(sys.argv) > 2:
            platform_choice = sys.argv[2].lower()
        elif sys.argv[1] in ["macos", "windows", "both"]:
            platform_choice = sys.argv[1].lower()
    
    # Determine what to build
    current = get_platform()
    
    if platform_choice == "current":
        platform_choice = current
    
    # Create directories
    os.makedirs("dist", exist_ok=True)
    os.makedirs("build", exist_ok=True)
    
    success = True
    
    if platform_choice == "macos":
        if current == "macos":
            success = build_macos()
        else:
            print("\nWarning: Building for macOS on non-macOS system")
            print("The resulting executable may not work properly")
            response = input("Continue anyway? (y/N): ").lower()
            if response == "y":
                success = build_macos()
            else:
                print("Build cancelled")
                sys.exit(0)
    
    elif platform_choice == "windows":
        if current == "windows":
            success = build_windows()
        else:
            print("\nWarning: Building for Windows on non-Windows system")
            print("The resulting executable may not work properly")
            response = input("Continue anyway? (y/N): ").lower()
            if response == "y":
                success = build_windows()
            else:
                print("Build cancelled")
                sys.exit(0)
    
    elif platform_choice == "both":
        if current == "macos":
            success = build_macos()
            if success:
                print("\n⚠ Note: To build for Windows, run this script on Windows")
        elif current == "windows":
            success = build_windows()
            if success:
                print("\n⚠ Note: To build for macOS, run this script on macOS")
        else:
            print("Error: Cannot build for both on this platform")
            success = False
    
    else:
        print(f"Error: Unknown platform '{platform_choice}'")
        print("Use: macos, windows, or both")
        sys.exit(1)
    
    # Show next steps
    if success:
        print("\n" + "="*50)
        print("Next Steps:")
        print("="*50)
        if platform_choice == "macos" or (platform_choice == "current" and current == "macos"):
            print("• Open the app: open 'dist/PicoSignals Config Editor.app'")
            print("• Code-sign (optional): codesign -s - 'dist/PicoSignals Config Editor.app'")
        elif platform_choice == "windows" or (platform_choice == "current" and current == "windows"):
            print("• Run: dist\\config_editor\\config_editor.exe")
        
        print("\nFor more information, see BUILD.md")
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
