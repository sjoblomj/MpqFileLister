# MpqFileLister

An [MPQDraft](https://github.com/sjoblomj/MPQDraft) plugin that logs all file access attempts made through Storm.dll's filename-based asset-loading functions.

## Overview

This plugin intercepts every Storm.dll function that fetches an asset from an MPQ archive by filename - `SFileOpenFile`, `SFileOpenFileEx`, `SFileLoadFile`, `SFileLoadFileEx`, `SBmpLoadImage`, `SBmpAllocLoadImage` and `SVidPlayBegin`. Every filename the game attempts to open is logged to a text file. See [Hooked functions](#hooked-functions) below for what each one does and which target games export it.

This is useful for:
- **Modding**: Discover which game assets are loaded and when.
- **Namebreaking**: MPQs sometimes don't contain the names of the files it contains. Use this to list all file names that are accessed.
- **Understanding**: Learn how the game loads its resources.

## Hooked functions

| Function | What it does | Diablo I | Later games |
|----------|---------------|:--------:|:------------:|
| `SFileOpenFile` | Opens a file by name, searching the default set of mounted archives, and returns a handle for later `SFileReadFile` calls. | ✅ | ✅ |
| `SFileOpenFileEx` | Same as `SFileOpenFile`, but lets the caller target a specific archive handle and/or search-scope flags. | ✅ | ✅ |
| `SFileLoadFile` | Loads an entire file by name in one call, with Storm allocating the buffer itself (no separate open/read/close). | ❌ not exported | ✅ |
| `SFileLoadFileEx` | Same as `SFileLoadFile`, but targets a specific archive handle/search scope. | ❌ not exported | ✅ |
| `SBmpLoadImage` | Fetches an image file by name and decodes it directly into a caller-supplied buffer. This is how menus load PCX backgrounds and other UI art - it bypasses `SFileOpenFile` entirely, combining the file lookup and image decode into one call. | ✅ | ✅ |
| `SBmpAllocLoadImage` | Same as `SBmpLoadImage`, but Storm allocates the pixel buffer itself via a caller-supplied allocation callback. Also used for PCX/UI art. | ❌ not exported | ✅ |
| `SVidPlayBegin` | Fetches and begins streaming a video (`.smk`) by filename, locating and opening the file internally rather than requiring the caller to first obtain a file handle. | ✅ | ✅ |

"Diablo I" and "Later games" refer to the **Target game** setting described under [Configuration](#configuration) below. The plugin selects Storm.dll export ordinals based on this setting; where a function is marked "not exported", Diablo I's Storm.dll simply doesn't have that entry point, so the plugin silently skips hooking it for that target.

The plugin also resolves (but does not hook) `SFileGetFileArchive` and `SFileGetArchiveName`, used only to look up which MPQ archive a given file was opened from when a log format that includes the archive name is selected. `SFileGetArchiveName` is not exported by Diablo I's Storm.dll, so archive names are unavailable for that target regardless of the chosen log format.

## Usage

1. Open [MPQDraft](https://github.com/sjoblomj/MPQDraft).
2. Load `MpqFileLister.qdp` as a plugin.
3. Configure the plugin settings as desired if desired - it will use default settings otherwise.
4. Run your game - the log file will be created automatically or as configured.

## Configuration

Click "Configure" in MPQDraft to open the settings dialog:

- **Log unique filenames only**: When enabled, each filename is logged only once (no duplicates). When disabled, every access is logged, even repeated ones.
- **Log format**: Decides the logging format. Choose whether to log timestamp (in milliseconds since epoch, 1970-01-07), the name of the archive and the file name.
- **Log file name**: The name of the log file. If you enter just a filename (e.g., `FileLog.txt`), it will be created in the game's directory. You can also specify an absolute path.
- **Target game**: Whether to target Diablo I, or later games.

Settings are saved to `MpqFileLister.ini` next to the plugin.

## Output

The log file contains one filename per line:

```
Broodat.mpq: unit\protoss\lshield.los
patch_rt.mpq: scripts\iscript.bin
Broodat.mpq: arr\sprites.dat
patch_rt.mpq: rez\stat_txt.tbl
patch_rt.mpq: scripts\aiscript.bin
patch_rt.mpq: arr\units.dat
patch_rt.mpq: arr\flingy.dat
patch_rt.mpq: arr\weapons.dat
patch_rt.mpq: unit\cmdbtns\cmdicons.grp
Stardat.mpq: tileset\badlands-nc.wpe
...
```

## Building

### Requirements

- CMake 3.15+
- C++17 compiler (MSVC or MinGW-w64)
- Windows SDK (for MSVC) or MinGW-w64 runtime

### Building with MSVC (Windows)

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Building with MinGW-w64 (Windows)

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

### Cross-compiling from Linux

Install MinGW-w64:
```bash
# Debian/Ubuntu
sudo apt install mingw-w64

# Fedora
sudo dnf install mingw64-gcc-c++

# Arch
sudo pacman -S mingw-w64-gcc
```

Build:
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-toolchain.cmake ..
cmake --build .
```

The output is `MpqFileLister.qdp` (a DLL with the MPQDraft plugin extension). Load this in MPQDraft.

## Files

| File                 | Description                     |
|----------------------|---------------------------------|
| `MpqFileLister.cpp`  | Main plugin implementation      |
| `MpqFileLister.h`    | Plugin class declaration        |
| `Config.cpp/h`       | Configuration loading/saving    |
| `ConfigDialog.cpp/h` | Win32 configuration dialog      |
| `QHookAPI.cpp/h`     | Import table patching utilities |
| `MPQDraftPlugin.h`   | MPQDraft plugin interface       |
