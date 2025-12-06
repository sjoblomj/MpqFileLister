/*
    Config.cpp - Configuration management for MpqFileLister plugin
*/

#include "Config.h"
#include <filesystem>
#include <fstream>

// === Configuration variables ===
bool g_logUniqueOnly = true;
std::string g_logFileName = "MPQDraft_FileLog.txt";

// Path to config file (next to the plugin DLL)
static std::string g_configFilePath;

void InitConfigPath(HMODULE hModule)
{
    std::string dllPath(MAX_PATH, '\0');
    DWORD len = GetModuleFileNameA(hModule, dllPath.data(), MAX_PATH);
    if (len > 0)
    {
        dllPath.resize(len);
        std::filesystem::path p(dllPath);
        g_configFilePath = (p.parent_path() / "MpqFileLister.ini").string();
    }
}

void LoadConfig()
{
    if (g_configFilePath.empty())
        return;

    std::ifstream file(g_configFilePath);
    if (!file.is_open())
        return;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.rfind("LogUniqueOnly=", 0) == 0)
        {
            g_logUniqueOnly = (line.substr(14) == "1");
        }
        else if (line.rfind("LogFileName=", 0) == 0)
        {
            g_logFileName = line.substr(12);
        }
    }
}

void SaveConfig()
{
    if (g_configFilePath.empty())
        return;

    std::ofstream file(g_configFilePath);
    if (!file.is_open())
        return;

    file << "LogUniqueOnly=" << (g_logUniqueOnly ? "1" : "0") << "\n";
    file << "LogFileName=" << g_logFileName << "\n";
}

