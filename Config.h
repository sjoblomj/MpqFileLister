/*
    Config.h - Configuration management for MpqFileLister plugin
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <string>

// === Configuration variables ===

// Target game options (determines which Storm.dll ordinals to use)
enum class TargetGame
{
    DIABLO_1 = 0,  // Diablo I (uses different ordinals)
    LATER = 1      // StarCraft, Diablo II, Warcraft II, etc.
};

// Default log line format - see FormatLogEntry() in MpqFileLister.cpp for the
// placeholder syntax (%t, %T, %t{...}, %a, %f, %c, %p, %P, %%, %{, %}).
extern const char* const DEFAULT_LOG_FORMAT;

extern bool g_logUniqueOnly;
extern std::string g_logFormat;
extern TargetGame g_targetGame;
extern std::string g_logFileName;

// === Configuration functions ===

// Initialize the config file path based on the DLL location
void InitConfigPath(HMODULE hModule);

// Load configuration from the INI file
void LoadConfig();

// Save configuration to the INI file
void SaveConfig();

#endif // CONFIG_H
