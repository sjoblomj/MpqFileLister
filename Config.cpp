/*
    Config.cpp - Configuration management for MpqFileLister plugin
*/

#include "Config.h"
#include "Utils.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

// === Configuration variables ===
const char* const DEFAULT_LOG_FORMAT = "%f";

bool g_logUniqueOnly = true;
std::string g_logFormat = DEFAULT_LOG_FORMAT;
TargetGame g_targetGame = TargetGame::LATER;
std::string g_logFileName = "MpqFileLister_FileLog.txt";

// Path to the config file (next to the plugin DLL)
static std::string g_configFilePath;

void InitConfigPath(HMODULE hModule)
{
    std::string dllPath = GetModulePathSafe(hModule);
    if (!dllPath.empty())
    {
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

    // Skip a UTF-8 byte-order mark if present (e.g. added by Notepad when saving),
    // so the first key=value line's prefix match below doesn't silently fail.
    if (file.peek() == 0xEF)
    {
        char bom[3];
        file.read(bom, 3);
        if (static_cast<unsigned char>(bom[1]) != 0xBB || static_cast<unsigned char>(bom[2]) != 0xBF)
            file.seekg(0);
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (line.rfind("LogUniqueOnly=", 0) == 0)
        {
            g_logUniqueOnly = (line.substr(14) == "1");
        }
        else if (line.rfind("LogFormat=", 0) == 0)
        {
            g_logFormat = line.substr(10);
        }
        else if (line.rfind("TargetGame=", 0) == 0)
        {
            try
            {
                int gameValue = std::stoi(line.substr(11));
                if (gameValue >= 0 && gameValue <= 1)
                    g_targetGame = static_cast<TargetGame>(gameValue);
            }
            catch (const std::exception&)
            {
                // Malformed value in the ini file - keep the current/default TargetGame
            }
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
    file << "LogFormat=" << g_logFormat << "\n";
    file << "TargetGame=" << static_cast<int>(g_targetGame) << "\n";
    file << "LogFileName=" << g_logFileName << "\n";
}
