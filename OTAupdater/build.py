#!/usr/bin/env python3
"""
Cross-platform build script for OTAUpdater GUI
Usage: python3 build.py [--platform macos|windows|both] [--arch x86_64|arm64]
"""

import os
import sys
import subprocess
import shutil
import platform


def get_platform():
    system = platform.system()
    if system == "Darwin":
        return "macos"
    elif system == "Windows":
        return "windows"
    else:
        return "linux"


def check_pyinstaller():
    try:
        import PyInstaller  # noqa: F401
        return True
    except Exception:
        return False


def run_pyinstaller(spec_file, arch=None, use_wine=False):
    cmd = None
    if use_wine:
        wine = shutil.which("wine")
        if not wine:
            raise RuntimeError("Wine not found on PATH")
        cmd = [wine, "pyinstaller", "-y", "--clean", spec_file]
    else:
        if arch == "x86_64" and platform.system() == "Darwin":
            arch_bin = shutil.which("arch")
            if arch_bin:
                cmd = [arch_bin, "-x86_64", "pyinstaller", "-y", "--clean", spec_file]
            else:
                cmd = ["pyinstaller", "-y", "--clean", spec_file]
        else:
            cmd = ["pyinstaller", "-y", "--clean", spec_file]

    print("Running:", " ".join(cmd))
    try:
        completed = subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if completed.stdout:
            print(completed.stdout)
        if completed.stderr:
            print(completed.stderr)
    except subprocess.CalledProcessError as e:
        print("PyInstaller stdout:")
        if getattr(e, 'stdout', None):
            print(e.stdout)
        print("PyInstaller stderr:")
        if getattr(e, 'stderr', None):
            print(e.stderr)
        raise


def is_ci_environment():
    if os.environ.get("CI"):
        return True
    if os.environ.get("GITHUB_ACTIONS") == "true":
        return True
    return False


def ask_yes(prompt, default=False):
    if is_ci_environment():
        print(f"{prompt} [Auto-answered {'y' if default else 'n'} in CI]")
        return default
    try:
        resp = input(prompt).strip().lower()
        return resp == "y"
    except EOFError:
        return default


def build_macos(arch=None):
    print("\n" + "=" * 50)
    print("Building OTAUpdater for macOS...")
    print("=" * 50)

    if not os.path.exists("ota_macos.spec"):
        print("Error: ota_macos.spec not found")
        return False

    try:
        run_pyinstaller("ota_macos.spec", arch=arch, use_wine=False)

        if os.path.exists("dist/OTAUpdater.app"):
            print("\n[OK] macOS build successful!")
            print("Output: dist/OTAUpdater.app")
            return True
        else:
            print("\n[FAIL] Build completed but output not found")
            return False

    except subprocess.CalledProcessError as e:
        print(f"\n[FAIL] Build failed: {e}")
        return False


def build_windows():
    print("\n" + "=" * 50)
    print("Building OTAUpdater for Windows...")
    print("=" * 50)

    if not os.path.exists("ota_windows.spec"):
        print("Error: ota_windows.spec not found")
        return False

    try:
        current = get_platform()
        if current == "windows":
            run_pyinstaller("ota_windows.spec")
        else:
            wine = shutil.which("wine")
            if wine:
                print("\nWine detected — attempting experimental Windows build under Wine.")
                if ask_yes("Continue with Wine build? (y/N): ", default=True):
                    run_pyinstaller("ota_windows.spec", use_wine=True)
                else:
                    print("Build cancelled")
                    return False
            else:
                print("\nWarning: Building Windows executables on non-Windows hosts is not supported by PyInstaller.")
                print("Install Wine and re-run, or build on a Windows machine or CI.")
                if ask_yes("Continue anyway (attempt plain pyinstaller)? (y/N): ", default=True):
                    run_pyinstaller("ota_windows.spec")
                else:
                    print("Build cancelled")
                    return False

        if os.path.exists("dist/ota_updater/ota_updater.exe"):
            print("\n[OK] Windows build successful!")
            print("Output: dist/ota_updater/ota_updater.exe")
            return True
        else:
            print("\n[FAIL] Build completed but output not found")
            return False

    except subprocess.CalledProcessError as e:
        print(f"\n[FAIL] Build failed: {e}")
        return False


def main():
    print("=" * 50)
    print("OTAUpdater - Build Tool")
    print("=" * 50)

    if not check_pyinstaller():
        print("\nError: PyInstaller is not installed")
        print("Install with: pip install pyinstaller")
        sys.exit(1)

    platform_choice = "current"
    arch_choice = None

    if len(sys.argv) > 1:
        args = sys.argv[1:]
        i = 0
        while i < len(args):
            a = args[i]
            if a == "--platform" and i + 1 < len(args):
                platform_choice = args[i + 1].lower()
                i += 2
            elif a in ["macos", "windows", "both"]:
                platform_choice = a
                i += 1
            elif a == "--arch" and i + 1 < len(args):
                arch_choice = args[i + 1]
                i += 2
            else:
                i += 1

    current = get_platform()
    if platform_choice == "current":
        platform_choice = current

    os.makedirs("dist", exist_ok=True)
    os.makedirs("build", exist_ok=True)

    success = True

    if platform_choice == "macos":
        if current == "macos":
            success = build_macos(arch=arch_choice)
        else:
            print("\nWarning: Building for macOS on non-macOS system")
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
            if ask_yes("Continue anyway? (y/N): ", default=True):
                success = build_windows()
            else:
                print("Build cancelled")
                sys.exit(0)

    elif platform_choice == "both":
        if current == "macos":
            success = build_macos(arch=arch_choice)
            if success:
                print("\n[WARN] Note: To build for Windows reliably, run this script on Windows or use CI.")
        elif current == "windows":
            success = build_windows()
            if success:
                print("\n[WARN] Note: To build for macOS, run this script on macOS")
        else:
            print("Error: Cannot build for both on this platform")
            success = False

    else:
        print(f"Error: Unknown platform '{platform_choice}'")
        print("Use: macos, windows, or both")
        sys.exit(1)

    if success:
        print("\n" + "=" * 50)
        print("Next Steps:")
        print("=" * 50)
        if platform_choice == "macos" or (platform_choice == "current" and current == "macos"):
            print("- Open the app: open 'dist/OTAUpdater.app'")
        elif platform_choice == "windows" or (platform_choice == "current" and current == "windows"):
            print("- Run: dist\\ota_updater\\ota_updater.exe")

        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
