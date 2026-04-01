# Building PicoSignals Configuration Editor Executables

This directory contains PyInstaller spec files and build scripts to create standalone executables for macOS and Windows.

## Quick Start

### Build from This Directory
```bash
cd ConfigEditor
python3 build.py
```

### Build from Root picoSignals Directory
Yes, you can use convenience scripts from the root directory:

**macOS:**
```bash
python3 build_config_editor.py
# or
./build_config_editor_macos.sh
```

**Windows:**
```cmd
python build_config_editor.py
# or
build_config_editor_windows.bat
```

---

## Prerequisites

1. **Python 3.8+** installed
2. **PyInstaller** installed:
   ```bash
   pip install pyinstaller
   ```

## Building for macOS

### Option 1: Using the build script (recommended)

```bash
chmod +x build_macos.sh
./build_macos.sh
```

### Option 2: Manual build

```bash
pyinstaller config_editor_macos.spec
```

**Output:** `dist/PicoSignals Config Editor.app`

### macOS Code Signing (Optional)

To sign the app for distribution:

```bash
# Self-signed (for testing)
codesign -s - "dist/PicoSignals Config Editor.app"

# With Developer ID (for distribution)
codesign -s "Developer ID Application: Your Name" "dist/PicoSignals Config Editor.app"
```

### Run the macOS app

```bash
open "dist/PicoSignals Config Editor.app"
```

---

## Building for Windows

### Option 1: Using the build script (recommended)

```cmd
build_windows.bat
```

### Option 2: Manual build

```cmd
pyinstaller config_editor_windows.spec
```

**Output:** `dist\config_editor\config_editor.exe`

### Run the Windows app

```cmd
dist\config_editor\config_editor.exe
```

---

## Cross-Platform Building

### Building on macOS for Windows

You can build the Windows executable on macOS, but it's not recommended. The resulting .exe will work, but testing is limited.

**Note:** For production builds, use the native OS whenever possible.

---

## Troubleshooting

### PyInstaller not found
```bash
pip install --upgrade pyinstaller
```

### Missing module errors
Edit the spec files and add the missing module to the `hiddenimports` list:
```python
hiddenimports=[
    'PyQt6.QtCore',
    'PyQt6.QtGui',
    'PyQt6.QtWidgets',
    'module_name',  # Add here
]
```

### App won't start on Windows
- Ensure all dependencies are installed: `pip install PyQt6`
- Check that Visual C++ Redistributable is installed on the target Windows machine
- Try running from command prompt to see error messages: `dist\config_editor\config_editor.exe`

### macOS: "App is damaged" error
The app needs to be code-signed. Run:
```bash
codesign -s - "dist/PicoSignals Config Editor.app"
```

---

## Distribution

### macOS Distribution

1. Build the app: `./build_macos.sh`
2. Code-sign it (see above)
3. Create a DMG file (optional):
   ```bash
   hdiutil create -volname "PicoSignals Config Editor" \
     -srcfolder dist \
     -ov -format UDZO config_editor.dmg
   ```
4. Distribute the `.app` bundle or `.dmg` file

### Windows Distribution

1. Build the app: `build_windows.bat`
2. The `dist\config_editor` folder contains all files needed to run
3. You can:
   - Distribute the entire `config_editor` folder
   - Create an installer using NSIS or Inno Setup
   - Create a ZIP file of the folder

---

## File Structure

```
dist/
├── PicoSignals Config Editor.app/  (macOS - the full app bundle)
└── config_editor/                   (Windows - the folder with exe and dependencies)
    ├── config_editor.exe
    ├── PyQt6/
    └── ...other dependencies
```

---

## Version Number

To add a version number to the executable:

1. **macOS:** Edit `config_editor_macos.spec` and add to `info_plist`:
   ```python
   info_plist={
       'CFBundleShortVersionString': '1.0.0',
       'CFBundleVersion': '1.0.0',
   }
   ```

2. **Windows:** Create a version.txt file and reference it in the spec file (see PyInstaller docs)

---

## Performance Tips

- The first run will be slower as the application unpacks resources
- Subsequent runs will be faster
- The `--onefile` option in the spec creates a single executable file instead of a folder
- You can modify the spec files to change this behavior

---

## Support

For issues with PyInstaller, see: https://pyinstaller.readthedocs.io/
For PyQt6 issues, see: https://www.riverbankcomputing.com/software/pyqt/
