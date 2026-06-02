# -*- mode: python ; coding: utf-8 -*-
"""
PyInstaller spec file for OTA Updater (macOS)
"""

block_cipher = None

a = Analysis(
    ['OTAupdater_GUI.py'],
    pathex=[],
    binaries=[],
    datas=[],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
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
    name='ota_updater',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
)

app = BUNDLE(
    exe,
    name='OTAUpdater.app',
    icon='icons/icon.icns',
    bundle_identifier='com.picosignals.otaupdater',
)
