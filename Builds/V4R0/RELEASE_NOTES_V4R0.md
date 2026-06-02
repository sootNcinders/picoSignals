# PicoSignals Release Notes
## Version 4.0 (V4R0)

**Current Version:** 0215d21  
**Commits Since CTC Update Bug Fix:** 53  
**Release Date:** June 1, 2026

## New Features

- **GUI Wrapper for OTA Updates** – Added a graphical interface for the OTA updater tool
- **Config Editor Tool** – New GUI-based configuration editor with a clean interface
- **PCA9554 I/O Chip Support** – Extended support for multiple types of I/O expansion chips (PCA9554 & PCA9674), replacing obsolete hardware

## Major Updates

**Bootloader Implementation** – Complete bootloader system developed and tested, ready for field deployment
- Supports remote OTA firmware updates
- Multi-board address support for simultaneous updates across multiple boards
- Successfully tested and validated

**Configurable Logging** – Added ability to toggle different groups of print statements for better debugging control

## Bug Fixes & Improvements

- **Input Handling Refactor** – Restructured input system to use point structure with inheritance for cleaner I/O chip management
- **Help Statement Cleanup** – Fixed and cleaned up help command output
- **Release Folder Organization** – Moved release artifacts into separate version folders (V4R0 and earlier versions)

## Affected Components

- Bootloader system (new)
- Configuration tools (new GUI)
- Input/I/O handling (PCA9554/9674 support)
- LED control and status management
- Radio communication (address support)
- Menu system
- OTA update infrastructure
- Build system

## Commit History

- 0215d21 2026-06-01 YML fixes
- b7605e9 2026-06-01 Update ota_build.yml
- d7e48ae 2026-06-01 Updater Build
- f91b3e8 2026-06-01 File move
- 39f2686 2026-06-01 yml
- d897bd1 2026-06-01 Zip
- 3dfdc2c 2026-06-01 Update config_editor_windows.spec
- daffb46 2026-06-01 Update build.py
- 55446c6 2026-06-01 Update build.py
- 72f0a55 2026-06-01 Update build.py
- c0d7d13 2026-06-01 Update build.yml
- 12e40b0 2026-06-01 Update build.py
- 0c5391a 2026-06-01 Update build.yml
- 66d07c0 2026-06-01 Configurer App
- 5233f03 2026-05-31 Config and Notes
- c6b514d 2026-05-24 Log options
- 43b15a5 2026-04-05 GUI wrapper for OTA
- 9f4fe36 2026-03-31 Bug fixes and config editor
- 19b0633 2026-03-27 Added support for pca9554
- 74f69dd 2026-03-23 Bootloader Tested
- 8d9237a 2025-12-08 Added address for bootloader
- 00e0d19 2025-11-30 Bootloader
- 122c786 2025-11-21 Bootloader
- 9e81e50 2025-11-21 Moved Releases into separate folders
- bd841e9 2025-10-12 Rev bump
- 408d7dc 2025-10-12 CTC Update bug fix
- 3ee8722 2025-09-29 Bug fixes
- 93d3727 2025-09-07 Manual Update
- b6b6363 2025-09-07 V3R3 Release
- 3a1ae78 2025-09-06 New Configs
- b6772e5 2025-09-06 Dwarf Menu
- 6fe6dbc 2025-09-05 Merge pull request #1 from sootNcinders/LocalDwarfControl
- 3766080 2025-09-05 Local Dwarf
- a90d3a1 2025-09-05 Print nodes
- ca73db8 2025-09-01 SD safe state
- 053ea2c 2025-08-07 Git Ignore
- a8cf0b2 2025-08-02 Batt menu change
- 4ad9d5c 2025-08-02 CRC fix
- cacce0c 2025-07-29 RCLI Check in
- ef1e821 2025-07-23 Rebuild
- 33f914f 2025-07-23 Merge branch '24hr-Battery-Averaging' into Remote-Command-Line
- c864f25 2025-07-23 CRC
- 4cce482 2025-07-22 Radio Bug
- 9eb65b6 2025-07-22 Struct
- c38db12 2025-07-20 Add
- 22ec117 2025-05-29 Backspace fix
- e3cc0be 2025-05-29 I adjustment clean up
- 4c642a9 2025-05-26 V3R2
- eec6ea0 2025-05-21 Comment clean up
- b8acc72 2025-05-21 Adjustment Numbers
- 73ae78c 2025-05-14 Clean up
- a626189 2025-02-14 V3R1 Release
