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


def run_pyinstaller(spec_file, arch=None, use_wine=False):
    """Run PyInstaller with optional arch wrapper or via Wine (experimental).

    - arch: None or 'x86_64' to force running under Rosetta on macOS (uses `arch -x86_64`).
    - use_wine: if True, attempt to run PyInstaller under Wine (Windows build on macOS/Linux).
    """
    cmd = None
    if use_wine:
        wine = shutil.which("wine")
        if not wine:
            raise RuntimeError("Wine not found on PATH")
        # Use --noconfirm/-y to avoid interactive confirmation and --clean to remove temp files
        cmd = [wine, "pyinstaller", "-y", "--clean", spec_file]
    else:
        # macOS x86 on Apple Silicon: use `arch -x86_64` if available
        if arch == "x86_64" and platform.system() == "Darwin":
            arch_bin = shutil.which("arch")
            if arch_bin:
                cmd = [arch_bin, "-x86_64", "pyinstaller", "-y", "--clean", spec_file]
            else:
                # Fallback to plain pyinstaller — will likely build for host arch
                cmd = ["pyinstaller", "-y", "--clean", spec_file]
        else:
            cmd = ["pyinstaller", "-y", "--clean", spec_file]

    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True)


def is_ci_environment():
    """Detect CI or non-interactive environments."""
    if os.environ.get("CI"):
        return True
    if os.environ.get("GITHUB_ACTIONS") == "true":
        return True
    return False


def ask_yes(prompt, default=False):
    """Prompt the user for a yes/no answer. In CI, return `default` without prompting.

    Returns True for yes, False for no.
    """
    if is_ci_environment():
        print(f"{prompt} [Auto-answered {'y' if default else 'n'} in CI]")
        return default
    try:
        resp = input(prompt).strip().lower()
        return resp == "y"
    except EOFError:
        return default

def build_macos(arch=None):
    """Build for macOS"""
    print("\n" + "="*50)
    print("Building for macOS...")
    print("="*50)
    
    if not os.path.exists("config_editor_macos.spec"):
        print("Error: config_editor_macos.spec not found")
        return False
    
    try:
        # Use helper which can run under Rosetta when requested
        run_pyinstaller("config_editor_macos.spec", arch=arch, use_wine=False)
        
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
        current = get_platform()
        if current == "windows":
            run_pyinstaller("config_editor_windows.spec")
        else:
            # Attempt experimental Wine-based build on non-Windows hosts
            wine = shutil.which("wine")
            if wine:
                print("\nWine detected — attempting experimental Windows build under Wine.")
                if ask_yes("Continue with Wine build? (y/N): ", default=True):
                    run_pyinstaller("config_editor_windows.spec", use_wine=True)
                else:
                    print("Build cancelled")
                    return False
            else:
                print("\nWarning: Building Windows executables on non-Windows hosts is not supported by PyInstaller.")
                print("Install Wine and re-run, or build on a Windows machine or CI.")
                if ask_yes("Continue anyway (attempt plain pyinstaller)? (y/N): ", default=True):
                    run_pyinstaller("config_editor_windows.spec")
                else:
                    print("Build cancelled")
                    return False

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


def check_icons(platform_choice):
    """Verify icon files exist for the requested platform(s) and prompt if missing."""
    missing = []
    if platform_choice in ("macos", "both"):
        if not os.path.exists(os.path.join("icons", "icon.icns")):
            missing.append("macOS: icons/icon.icns")
    if platform_choice in ("windows", "both"):
        if not os.path.exists(os.path.join("icons", "icon.ico")):
            missing.append("Windows: icons/icon.ico")

    if missing:
        print("\nWarning: The following icon files are missing:")
        for m in missing:
            print(" - " + m)
        if not ask_yes("Continue without custom icons? (y/N): ", default=True):
            print("Build cancelled")
            sys.exit(0)

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
    arch_choice = None
    # Support: --platform <macos|windows|both>  and optional --arch <x86_64|arm64>
    if len(sys.argv) > 1:
        args = sys.argv[1:]
        i = 0
        while i < len(args):
            a = args[i]
            if a == "--platform" and i+1 < len(args):
                platform_choice = args[i+1].lower()
                i += 2
            elif a in ["macos", "windows", "both"]:
                platform_choice = a
                i += 1
            elif a == "--arch" and i+1 < len(args):
                arch_choice = args[i+1]
                i += 2
            else:
                i += 1
    
    # Determine what to build
    current = get_platform()
    
    if platform_choice == "current":
        platform_choice = current
    
    # Create directories
    os.makedirs("dist", exist_ok=True)
    os.makedirs("build", exist_ok=True)
    # Check for expected icon files and prompt if missing
    check_icons(platform_choice)
    
    success = True
    
    if platform_choice == "macos":
        if current == "macos":
            success = build_macos(arch=arch_choice)
        else:
            print("\nWarning: Building for macOS on non-macOS system")
            print("The resulting executable may not work properly")
            if ask_yes("Continue anyway? (y/N): ", default=True):
                success = build_macos(arch=arch_choice)
            else:
                print("Build cancelled")
                sys.exit(0)
    
    elif platform_choice == "windows":
        if current == "windows":
            success = build_windows()
        else:
            print("\nWarning: Building for Windows on non-Windows system")
            print("The resulting executable may not work properly")
            if ask_yes("Continue anyway? (y/N): ", default=True):
                success = build_windows()
            else:
                print("Build cancelled")
                sys.exit(0)
    
    elif platform_choice == "both":
        if current == "macos":
            success = build_macos(arch=arch_choice)
            if success:
                print("\n⚠ Note: To build for Windows reliably, run this script on Windows or use CI.")
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
