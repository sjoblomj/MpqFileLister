# MPQ File Lister

An MPQDraft plugin that logs all file access attempts made through Storm.dll's `SFileOpenFileEx` function.

## Overview

This plugin intercepts calls to `SFileOpenFileEx` (ordinal 0x10C) in Storm.dll - the function Blizzard games use to open files from MPQ archives. Every filename the game attempts to open is logged to a text file.

This is useful for:
- **Modding**: Discover which game assets are loaded and when
- **Debugging**: Track down missing or incorrectly named files
- **Understanding**: Learn how the game loads its resources

## Installation

1. Copy `MpqFileLister.qdp` to your MPQDraft plugins directory
2. Launch MPQDraft and enable the plugin
3. Run your game - the log file will be created automatically

## Configuration

Click "Configure" in MPQDraft to open the settings dialog:

- **Log unique filenames only**: When enabled, each filename is logged only once (no duplicates). When disabled, every access is logged, even repeated ones.
- **Log file name**: The name of the log file. If you enter just a filename (e.g., `MPQDraft_FileLog.txt`), it will be created in the game's directory. You can also specify an absolute path.

Settings are saved to `MpqFileLister.ini` next to the plugin.

## Output

The log file contains one filename per line:

```
=== MPQ File Access Log ===
rez\glucurs.bin
dlgs\popup640.grp
glue\title\title.pcx
arr\tfontgam.pcx
...
=== End of Log ===
```

## Building

### Requirements

- CMake 3.15+
- C++17 compiler (MSVC or MinGW)
- Windows SDK

### Build Commands

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The output is `MpqFileLister.qdp` (a DLL with the MPQDraft plugin extension).

## Technical Details

- Uses import table patching via `PatchImportEntry()` to hook Storm.dll
- Thread-safe logging with `std::mutex`
- Standard C++17 where possible (`std::filesystem`, `std::ofstream`, `std::string`)
- No MFC dependencies - pure Win32 API and standard C++

## Files

| File | Description |
|------|-------------|
| `MpqFileLister.cpp` | Main plugin implementation |
| `MpqFileLister.h` | Plugin class declaration |
| `Config.cpp/h` | Configuration loading/saving |
| `ConfigDialog.cpp/h` | Win32 configuration dialog |
| `QHookAPI.cpp/h` | Import table patching utilities |
| `MPQDraftPlugin.h` | MPQDraft plugin interface |

## License

This plugin is part of the MPQDraft project.

