/*
    LogFormat.h - Renders one logged Storm.dll call into a line of text, according
    to the user-configurable log format template. See FormatLogEntry() below.
*/

#ifndef LOGFORMAT_H
#define LOGFORMAT_H

#include <windows.h>
#include <string>

// Formats a raw pointer value for the log, e.g. "0x28fe1c", or "(null)".
std::string PointerToString(const void* ptr);

// For a pointer parameter whose target either isn't a simple scalar (a raw byte/pixel
// buffer, a 256-entry palette array, an opaque structure) or whose validity we can't
// establish (an undocumented parameter of unknown lifetime, a callback/function pointer):
// report the address only. Dereferencing any of these would either be meaningless, risk
// crashing the hooked game, or require dumping arbitrary binary data into a text log.
std::string PointerOnly(const char* name, const void* ptr);

// For a pointer-to-DWORD output parameter that a successful call is known to populate
// (e.g. SBmpLoadImage's lpdwWidth): report both the address and the value it points to.
std::string PointerWithDword(const char* name, const DWORD* ptr);

// For a pointer-to-HANDLE output parameter (the common Storm "give me a handle back"
// idiom, e.g. SFileOpenFile's hFile): report both the address and the resulting handle.
std::string PointerWithHandle(const char* name, HANDLE* ptr);

// For a pointer-to-pointer output buffer (e.g. SFileLoadFile's lplpFileData): report the
// address plus the resulting buffer's address - one level of dereference to reveal where
// the data ended up, without ever dumping the buffer's (arbitrary-length, binary) contents.
std::string PointerWithPointer(const char* name, void* const* ptr);

// Expands a user-supplied log format string, replacing placeholders with the
// values for one logged call:
//   %t       - timestamp, milliseconds since epoch
//   %T       - timestamp, ISO-8601 UTC, millisecond precision
//   %t{...}  - timestamp, custom strftime-style format; %L inside the braces is
//              replaced with milliseconds; falls back to the raw "%t{...}" text
//              if the format is invalid (see FormatLogEntry.cpp for why)
//   %a       - MPQ archive name
//   %f       - filename
//   %c       - the Storm.dll call, e.g. SFileOpenFileEx
//   %p       - that call's non-pointer parameters
//   %P       - that call's pointer parameters
//   %%, %{, %} - a literal '%', '{' or '}' character
// Any other character (including an unrecognized "%x" sequence, which is kept
// as-is) is copied through unchanged. Operating purely on std::string with
// index-checked access means there is no fixed-size buffer to overflow,
// regardless of how long the format string or the substituted values are.
std::string FormatLogEntry(const std::string& format, const std::string& archiveName,
                            const std::string& fileName, const std::string& callName,
                            const std::string& nonPointerParams, const std::string& pointerParams);

#endif // LOGFORMAT_H
