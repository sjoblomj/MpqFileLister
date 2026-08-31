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

- **Log unique filenames only**: When enabled, each filename is logged only once (no duplicates). Uniqueness is based on the filename, plus the archive name if your log format includes `%a` - if it doesn't, identically named files from different archives are treated as the same entry, matching what the log actually shows you. Either way, this is unaffected by `%t`, `%T`, `%t{...}`, `%c`, `%p` or `%P` varying between accesses. When disabled, every access is logged, even repeated ones.
- **Log format**: A free-form text template for each logged line, including how timestamps are rendered - see [Log format](#log-format) below.
- **Log file name**: The name of the log file. If you enter just a filename (e.g., `FileLog.txt`), it will be created in the game's directory. You can also specify an absolute path.
- **Target game**: Whether to target Diablo I, or later games.

Settings are saved to `MpqFileLister.ini` next to the plugin.

## Log format

The log format is a template applied to every logged call. Any character is printed as-is, except for the following placeholders:

| Placeholder | Expands to |
|-------------|------------|
| `%t` | Timestamp, milliseconds since epoch (1970-01-01), e.g. `1735689600123` |
| `%T` | Timestamp, ISO-8601, UTC, millisecond precision, e.g. `2026-08-27T14:03:21.123Z` |
| `%t{...}` | Timestamp, custom `strftime`-style format - see [Custom timestamp formats](#custom-timestamp-formats-t) below |
| `%a` | The MPQ archive name. Empty if unavailable - see caveats below |
| `%f` | The filename that was passed to the hooked call |
| `%c` | The name of the Storm.dll call that was made, e.g. `SFileOpenFileEx` |
| `%p` | That call's **non-pointer** parameters - see [Call parameters](#call-parameters-p-and-p) below |
| `%P` | That call's **pointer** parameters - see [Call parameters](#call-parameters-p-and-p) below |
| `%%`, `%{`, `%}` | A literal `%`, `{` or `}` character |

The default format is `%f` (filename only).

Any `%` followed by a character other than one of the above (including a lone `%` at the very end of the format string) is left in the output unchanged, so a typo like `%x` shows up as `%x` in the log rather than silently eating a character. To print a literal `%t`, `%a`, etc. rather than have it expanded, escape the `%` as `%%`, e.g. a format of `%%t = %t` logs a line like `%t = 1735689600000`. This mirrors the common `printf`/`strftime`-style convention of doubling `%` to escape it, which is why that convention was used here - and it's why `{` and `}` get the same treatment (`%{`/`%}`) rather than a different escape mechanism of their own.

### Custom timestamp formats (`%t{...}`)

`%t` and `%T` cover the two most common needs (a compact sortable number, and a human-readable standard timestamp) without any setup. For anything else, `%t{...}` runs the text between the braces through the C runtime's `strftime`, with one addition: `%L` inside the braces expands to the millisecond fraction, zero-padded to 3 digits (`strftime` itself has no notion of sub-second precision). For example:

```
%t{%H:%M:%S.%L}             ->  14:03:21.123
%t{%A, %B %d}               ->  Thursday, August 27
%t{%Y-%m-%dT%H:%M:%S.%L}Z   ->  2026-08-27T14:03:21.123Z   (equivalent to %T)
```

A few things worth knowing before relying on this:

- **Braces only mean anything right after `%t`.** A `{` or `}` anywhere else in your format is already just a literal character - no escaping needed. Escaping (`%{`/`%}`) is only needed for a literal brace immediately after `%t` (to stop it being read as the start of a sub-format), or for a literal `}` *inside* a sub-format's braces (to stop it being read as the closing brace) - `%t{%Y-%m-%d%}}` renders as `2026-08-27}`.
- **Only whatever `strftime` your C runtime actually implements is available**. On the toolchain this plugin is built with, the supported specifiers are `%a %A %b %B %c %d %H %I %j %m %M %p %S %U %w %W %x %X %y %Y %z %Z`. Stick to the tested set above, or the fixed `%t`/`%T` placeholders.
- **`%z`/`%Z` are not UTC-aware.** They print the *host machine's local* timezone name, regardless of the fact that the rest of the values (`%H`, `%M`, `%S`, etc.) are computed in UTC. Using them in a custom format will produce misleading output (e.g. a UTC hour next to a non-UTC zone label). Avoid `%z`/`%Z` in `%t{...}`.
- **Any unsupported or invalid specifier fails the entire sub-format, not just that token.** A single unrecognized specifier anywhere in the string makes the whole call return nothing. Rather than silently produce a blank timestamp, an invalid `%t{...}` renders as its own unresolved source text (e.g. `%t{%Q}` if `%Q` isn't a real specifier) - the mistake stays visible in the log instead of disappearing.
- **`%t{...}` only has access to time.** The outer placeholders (`%f`, `%a`, `%c`, `%p`, `%P`) aren't available inside the braces - that scope is handed entirely to `strftime`, which only knows about dates and times.
- **Locale matters here in a way it doesn't for `%t`/`%T`.** Specifiers like `%a`/`%A`/`%b`/`%B`/`%c`/`%x`/`%X` render using whatever locale the game process happens to be running under, so weekday/month names may not always come out in English. `%t` and `%T` are unaffected, since both are built by hand rather than through `strftime`.

### Archive name availability

`%a` requires two things: the hooked call must yield a file handle (`SFileOpenFile`/`SFileOpenFileEx` do; `SFileLoadFile`, `SFileLoadFileEx`, `SBmpLoadImage`, `SBmpAllocLoadImage` and `SVidPlayBegin` do not, since none of them return an `HSFILE`), and Storm.dll must export `SFileGetArchiveName` - which it does **not** when targeting Diablo I. If either condition isn't met, `%a` simply expands to an empty string rather than causing an error.

### Call parameters (`%p` and `%P`)

The split is purely mechanical, by C parameter type: every argument to the hooked call that **isn't** a pointer goes into `%p`; every argument that **is** a pointer (including Windows `HANDLE`s, which are pointer-typed) goes into `%P`. Nothing is left out and nothing is curated for "interestingness" - if you only want part of this, combine `%p`/`%P` with your own format text, or omit whichever one you don't need.

For a pointer parameter, `%P` always logs its raw address (or `(null)`). For some of those pointers it also logs the value found at that address, shown as `(deref=...)`, when doing so is both safe and meaningful:

- **Safe** means the call already succeeded and the pointer is a well-understood, fixed-size output slot (a `DWORD*` or `HANDLE*` "give me a value back" parameter) - not a buffer of unknown/arbitrary length, not a parameter whose validity we can't establish, and not something that isn't actually data (a callback/function pointer).
- **Meaningful** means the dereferenced value is itself a plain, short, printable thing (a number or a handle) rather than something that would need to be dumped as raw/binary data (pixel buffers, a 256-entry palette, an `OVERLAPPED` structure).

Where a pointer-to-pointer output parameter is involved (e.g. `SFileLoadFile`'s `lplpFileData`), exactly one level of dereference is shown - the resulting buffer's address - never the buffer's contents.

| Call | `%p` (non-pointer parameters) | `%P` (pointer parameters) |
|------|-------------------------------|----------------------------|
| `SFileOpenFile` | *(none - every parameter is a pointer)* | `lpFileName`; `hFile` (deref: the resulting handle) |
| `SFileOpenFileEx` | `dwSearchScope` | `hMpq`; `szFileName`; `phFile` (deref: the resulting handle) |
| `SFileLoadFile` | `dwFlags1`, `dwFlags2` | `lpFileName`; `lplpFileData` (deref: the loaded buffer's address); `lpdwFileSize` (deref: the loaded size) |
| `SFileLoadFileEx` | `dwFlags1`, `dwFlags2` | `hMpq`; `lpFileName`; `lplpFileData` (deref: the loaded buffer's address); `lpdwFileSize` (deref: the loaded size); `lpOverlapped` (address only - opaque structure) |
| `SBmpLoadImage` | `dwBitsSize` (the caller's destination-buffer capacity) | `lpFileName`; `lpPalette` (address only - a 256-entry array, not a scalar); `lpBits` (address only - raw pixel buffer); `lpdwWidth`, `lpdwHeight`, `lpdwBpp` (each deref'd only if the caller passed a non-`NULL` pointer for it) |
| `SBmpAllocLoadImage` | *(none - every parameter is a pointer)* | `lpFileName`; `lpPalette` (address only); `lplpBits` (deref: the allocated buffer's address); `lpdwWidth`, `lpdwHeight`, `lpdwBpp`, `lpdwSize` (each deref'd only if the caller passed a non-`NULL` pointer for it); `lpAllocProc` (address only - a callback function pointer) |
| `SVidPlayBegin` | `a2`, `flags` | `filename`; `a3`, `a4`, `a5` (address only - `a2`-`a5` are undocumented/unnamed in this reverse-engineered signature, and `a3`-`a5` are pointers of unknown validity/lifetime, so they're never dereferenced); `video` (deref: the resulting handle, following the same "give me a handle back" idiom as `hFile`/`phFile`) |

### Example

A format of:

```
%t %a: %f (%c, %p | %P)
```

produces lines like:

```
1735689600123 Broodat.mpq: unit\protoss\lshield.los (SFileOpenFileEx, dwSearchScope=0x0 | hMpq=(null), szFileName=0x28fe30, phFile=0x28fe1c (deref=0x1f4))
1735689600456 patch_rt.mpq: rez\stat_txt.tbl (SFileOpenFile,  | lpFileName=0x28fe30, hFile=0x28fe1c (deref=0x1a8))
1735689601001 : glue\palette.pcx (SBmpLoadImage, dwBitsSize=64000 | lpFileName=0x28fe30, lpPalette=0x28fe40, lpBits=0xa10000, lpdwWidth=0x28fe10 (deref=320), lpdwHeight=0x28fe14 (deref=200), lpdwBpp=0x28fe18 (deref=8))
```

(Note the empty `%a` for Diablo I or for calls that don't provide an archive name, and the empty `%p` for `SFileOpenFile` since all of its parameters are pointers. Addresses shown above are illustrative, not real.)

## Output

With the default log format (`%f`), the log file contains one filename per line:

```
unit\protoss\lshield.los
scripts\iscript.bin
arr\sprites.dat
rez\stat_txt.tbl
scripts\aiscript.bin
arr\units.dat
arr\flingy.dat
arr\weapons.dat
unit\cmdbtns\cmdicons.grp
tileset\badlands-nc.wpe
...
```

Using a format of `%a: %f` instead, output looks like:

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

| File                 | Description                                                        |
|----------------------|---------------------------------------------------------------------|
| `MpqFileLister.cpp/h`| DLL entry point, `IMPQDraftPlugin` glue, and plugin lifecycle (`InitializePlugin`/`TerminatePlugin`). Knows nothing about which Storm.dll functions exist - see `Hooks.cpp/h` |
| `Hooks.cpp/h`        | Everything Storm.dll-specific: hook ordinals (Diablo I and later games), the `Hooked*` functions installed in the patched import table, and the archive-name lookup |
| `LogFormat.cpp/h`    | The log-line templating engine - expands `%t`/`%a`/`%f`/`%c`/`%p`/`%P`/etc. into a logged line |
| `Config.cpp/h`       | Configuration loading/saving                                        |
| `ConfigDialog.cpp/h` | Win32 configuration dialog                                          |
| `Utils.cpp/h`        | Small, generic, reusable helpers (safe module-path lookup, string joining) |
| `QHookAPI.cpp/h`     | Import table patching utilities                                     |
| `MPQDraftPlugin.h`   | MPQDraft plugin interface                                           |
