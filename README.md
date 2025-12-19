# MpqFileLister

An [MPQDraft](https://github.com/sjoblomj/MPQDraft) plugin that logs all file access attempts made through Storm.dll's `SFileOpenFile` and `SFileOpenFileEx` functions.

## Overview

This plugin intercepts calls to `SFileOpenFile` and `SFileOpenFileEx` in Storm.dll - the functions Blizzard games use to open files from MPQ archives. Every filename the game attempts to open is logged to a text file.

This is useful for:
- **Modding**: Discover which game assets are loaded and when.
- **Namebreaking**: MPQs sometimes don't contain the names of the files it contains. Use this to list all file names that are accessed.
- **Understanding**: Learn how the game loads its resources.

## Usage

1. Open [MPQDraft](https://github.com/sjoblomj/MPQDraft)
2. Load `MpqFileLister.qdp` as a plugin
3. Configure the plugin settings as desired if desired - it will use default settings otherwise
4. Run your game - the log file will be created automatically or as configured

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

## Technical Details

- Uses import table patching via `PatchImportEntry()` to hook Storm.dll
- Thread-safe logging with `std::mutex`
- Standard C++17 where possible (`std::filesystem`, `std::ofstream`, `std::string`)
- No MFC dependencies - pure Win32 API and standard C++

## Files

| File                 | Description                     |
|----------------------|---------------------------------|
| `MpqFileLister.cpp`  | Main plugin implementation      |
| `MpqFileLister.h`    | Plugin class declaration        |
| `Config.cpp/h`       | Configuration loading/saving    |
| `ConfigDialog.cpp/h` | Win32 configuration dialog      |
| `QHookAPI.cpp/h`     | Import table patching utilities |
| `MPQDraftPlugin.h`   | MPQDraft plugin interface       |
