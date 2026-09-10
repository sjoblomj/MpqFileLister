# Changelog

All notable changes to this project will be documented in this file.

## [1.2.0] - 2026-09-07

### Added
- Can now intercept calls to `SVidPlayBegin`, `SFileLoadFile`, `SFileLoadFileEx`, `SBmpLoadImage` and `SBmpAllocLoadImage`.
- Log format is now a customizable template instead of fixed choices, with placeholders for timestamp, archive name, filename, call name and call parameters.
- Timestamps can be rendered as epoch-milliseconds, ISO-8601, or a custom `strftime`-style format.

### Fixed
- The Diablo I ordinal for `SFileGetArchiveName` was resolving to the wrong function.
- A malformed `TargetGame` value in the ini file could crash the game on load.
- The log file path could be silently truncated when saving settings.
- Install paths at or beyond `MAX_PATH` could silently resolve to the wrong config/log file location.
- The ini file's first setting could silently be ignored if the file had a UTF-8 byte-order mark.

### Changed
- Split the Storm.dll-specific hook code, the log-line templating engine, and small shared utilities out of `MpqFileLister.cpp` into their own files (`StormHooks.cpp/h`, `LogFormat.cpp/h`, `Utils.cpp/h`) for maintainability. No behavior change.



## [1.1.0] - 2025-12-18

### Added
- Can now intercept calls to `SFileOpenFile`.
- Support for Diablo I (untested).
- Support for logging timestamps, in the form of milliseconds since epoch.
- Some UI improvements:
  * More logically grouped controls.
  * Descriptive labels.
  * Keyboard shortcuts.



## [1.0.0] - 2025-12-07

### Added
- Initial release. Support for:
  * Intercepting file calls to `SFileOpenFileEx`.
  * Outputting only file name, or file archive + file name.
  * Only logging file names once.
  * Absolute or relative path to output file.
