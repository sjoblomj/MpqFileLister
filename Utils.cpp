/*
    Utils.cpp - Small, generic, reusable helpers shared across MpqFileLister
*/

#include "Utils.h"

std::string GetModulePathSafe(HMODULE hModule)
{
    std::string path(MAX_PATH, '\0');
    for (;;)
    {
        DWORD len = GetModuleFileNameA(hModule, path.data(), static_cast<DWORD>(path.size()));
        if (len == 0)
            return "";
        if (len < path.size())
        {
            path.resize(len);
            return path;
        }
        if (path.size() >= 32768)
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
