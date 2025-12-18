# Changelog

All notable changes to this project will be documented in this file.

## [1.1] - 2025-12-18

### Added
- Can now intercept calls to `SFileOpenFile`.
- Support for Diablo I (untested).
- Support for logging timestamps, in the form of milliseconds since epoch.
- Some UI improvements:
  * More logically grouped controls.
  * Descriptive labels.
  * Keyboard shortcuts.



## [1.0] - 2025-12-07

### Added
- Initial release. Support for:
  * Intercepting file calls to `SFileOpenFileEx`.
  * Outputting only file name, or file archive + file name.
  * Only logging file names once.
  * Absolute or relative path to output file.
