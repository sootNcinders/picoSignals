# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec file for PicoSignals Configuration Editor (macOS)
Build with: pyinstaller config_editor_macos.spec
"""

block_cipher = None

a = Analysis(
    ['config_editor.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=[
        'PyQt6.QtCore',
        'PyQt6.QtGui',
        'PyQt6.QtWidgets',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludedimports=[],
    noarchive=False,
)

_zst = getattr(a, 'zst', getattr(a, 'zipped_data', None))
pyz = PYZ(a.pure, _zst, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='config_editor',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

app = BUNDLE(
    exe,
    name='PicoSignals Config Editor.app',
    icon='icons/icon.icns',
    bundle_identifier='com.picoSignals.configEditor',
    info_plist={
        'NSPrincipalClass': 'NSApplication',
        'NSHighResolutionCapable': 'True',
    },
)
