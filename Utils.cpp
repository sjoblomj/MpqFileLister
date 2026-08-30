/*
    Utils.cpp - Small, generic, reusable helpers shared across MpqFileLister
*/

#include "Utils.h"

// Declared by hand instead of including <windows.h>, to keep that header's large
// surface area (and macros like min/max) out of this otherwise platform-agnostic
// file. This is an ordinary declaration for a function implemented in kernel32.dll;
// the compiler doesn't care that the declaration didn't come from windows.h, as
// long as it's ABI-correct - which this is (HMODULE is void*, LPSTR is char*, and
// DWORD is a 32-bit unsigned long on Windows, on both 32- and 64-bit builds).
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(
    void* hModule, char* lpFilename, unsigned long nSize);

namespace
{
    // Matches the historical Win32 MAX_PATH (260), without pulling in <windows.h> for it.
    constexpr size_t kInitialPathBufferSize = 260;
    constexpr size_t kMaxPathBufferSize = 32768;
}

std::string GetModulePathSafe(void* hModule)
{
    std::string path(kInitialPathBufferSize, '\0');
    for (;;)
    {
        unsigned long len = GetModuleFileNameA(hModule, path.data(), static_cast<unsigned long>(path.size()));
        if (len == 0)
            return "";
        if (len < path.size())
        {
            path.resize(len);
            return path;
        }
        if (path.size() >= kMaxPathBufferSize)
            return ""; // give up rather than growing without bound
        path.resize(path.size() * 2);
    }
}

std::string JoinParts(const std::vector<std::string>& parts)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
            result += ", ";
        result += parts[i];
    }
    return result;
}
