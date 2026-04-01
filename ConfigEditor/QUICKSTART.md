# Quick Start: Building Executables

## One-Command Build

You can build from either the `ConfigEditor/` directory or from the root `picoSignals/` directory.

### Build from ConfigEditor Directory (Recommended)

```bash
cd ConfigEditor
python3 build.py
```

### Build from Root picoSignals Directory

**macOS:**
```bash
python3 build_config_editor.py
# or using the shell script:
./build_config_editor_macos.sh
```

**Windows:**
```cmd
python build_config_editor.py
# or using the batch script:
build_config_editor_windows.bat
```

---

## Build for Specific Platform

### macOS
```bash
cd ConfigEditor
python3 build.py --platform macos
```

### Windows
```cmd
cd ConfigEditor
python build.py --platform windows
```

### Both Platforms (requires running on each OS)
```bash
python3 build.py --platform both  # Run on macOS
python build.py --platform both   # Run on Windows
```

---

## What You Get

### macOS
- **Location:** `dist/PicoSignals Config Editor.app`
- **Type:** App bundle (standard macOS application)
- **Size:** ~150-200 MB
- **Run:** Double-click the app or `open "dist/PicoSignals Config Editor.app"`

### Windows
- **Location:** `dist/config_editor/config_editor.exe`
- **Type:** Standalone executable with dependencies folder
- **Size:** ~200-250 MB
- **Run:** Double-click the .exe or run from command prompt

---

## Installation Steps

1. **Install PyInstaller** (one time):
   ```bash
   pip install pyinstaller
   ```

2. **Navigate to ConfigEditor directory** (optional - you can also build from root):
   ```bash
   cd ConfigEditor
   ```

3. **Build for your platform**:
   ```bash
   python3 build.py  # Builds for current platform
   # or from root: python3 build_config_editor.py
   ```

4. **Find the output** in the `dist/` folder

5. **Run the app**:
   - macOS: Open the `.app` bundle
   - Windows: Run the `.exe`

---

## Distributing to Others

### macOS
- Share the `.app` bundle (entire folder)
- Or create a DMG file: see BUILD.md for instructions

### Windows
- Share the entire `config_editor` folder (must include all dependencies)
- Or create an installer using NSIS/Inno Setup

---

## Troubleshooting

**"PyInstaller not found"**
```bash
pip install pyinstaller
```

**"My antivirus flags the .exe as suspicious"**
- This is common with PyInstaller executables
- Code-sign the executable (see BUILD.md)

**"App won't start on Windows"**
- Ensure Visual C++ Redistributable is installed on the target machine
- Run from command prompt to see errors

**"macOS: App is damaged"**
```bash
codesign -s - "dist/PicoSignals Config Editor.app"
```

---

## For More Information
See [BUILD.md](BUILD.md) for detailed instructions and advanced options.
