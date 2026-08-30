/*
    Utils.h - Small, generic, reusable helpers shared across MpqFileLister
*/

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>

// Returns the full path to the given module (nullptr for the host executable).
// hModule is an opaque module handle (an HMODULE, passed as void* so this header
// doesn't need <windows.h>), as returned by the platform's module-loading APIs.
// The underlying lookup silently truncates the path if it doesn't fit in a fixed
// buffer, with no other indication - this retries with a larger buffer instead of
// risking a silently-wrong path for install locations at or beyond 260 characters.
// Returns "" on outright failure.
std::string GetModulePathSafe(void* hModule);

// Joins strings into a single ", "-separated string, e.g. for building "key=value"
// parameter lists.
std::string JoinParts(const std::vector<std::string>& parts);

#endif // UTILS_H
