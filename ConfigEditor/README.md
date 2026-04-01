# PicoSignals Configuration Editor

This directory contains the PicoSignals Configuration Editor - a PyQt6-based GUI application for creating and editing JSON configuration files for PicoSignals firmware.

## Quick Start

### Run the Editor
```bash
python3 config_editor.py
```

### Build Executables
```bash
# Build for current platform
python3 build.py

# Build for specific platform
python3 build.py --platform macos    # macOS
python3 build.py --platform windows  # Windows
```

## Files in This Directory

| File | Purpose |
|------|---------|
| `config_editor.py` | Main GUI application |
| `config_editor_macos.spec` | PyInstaller spec for macOS app bundle |
| `config_editor_windows.spec` | PyInstaller spec for Windows executable |
| `build.py` | Cross-platform build automation script |
| `build_macos.sh` | macOS shell script for building |
| `build_windows.bat` | Windows batch script for building |
| `BUILD.md` | Comprehensive build documentation |
| `QUICKSTART.md` | Quick reference for building |
| `CONFIG_EDITOR_README.md` | Features and usage documentation |
| `.buildignore` | Build artifacts exclusion list |

## Documentation

- **[QUICKSTART.md](QUICKSTART.md)** - Quick reference for building executables
- **[BUILD.md](BUILD.md)** - Comprehensive build guide with troubleshooting
- **[CONFIG_EDITOR_README.md](CONFIG_EDITOR_README.md)** - Features and usage guide

## Requirements

- Python 3.8+
- PyQt6: `pip install PyQt6`
- PyInstaller (for building executables): `pip install pyinstaller`

## Building Distributions

See [QUICKSTART.md](QUICKSTART.md) for quick instructions or [BUILD.md](BUILD.md) for detailed information.

After building, find your executable in the `dist/` folder:
- **macOS:** `dist/PicoSignals Config Editor.app`
- **Windows:** `dist/config_editor/config_editor.exe`

## Support

For issues or questions, see the [CONFIG_EDITOR_README.md](CONFIG_EDITOR_README.md) or run:
```bash
python3 config_editor.py --help
```
