/*
    Utils.h - Small, generic, reusable helpers shared across MpqFileLister
*/

#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <string>
#include <vector>

// Returns the full path to the given module (nullptr for the host executable).
// GetModuleFileNameA silently truncates the path if it doesn't fit in the buffer
// (returning a length equal to the buffer size, with no other indication) - this
// retries with a larger buffer instead of risking a silently-wrong path for install
// locations at or beyond MAX_PATH characters. Returns "" on outright failure.
std::string GetModulePathSafe(HMODULE hModule);

// Joins strings into a single ", "-separated string, e.g. for building "key=value"
// parameter lists.
std::string JoinParts(const std::vector<std::string>& parts);

#endif // UTILS_H
