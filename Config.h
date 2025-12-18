/*
    Config.h - Configuration management for MpqFileLister plugin
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <string>

// === Configuration variables ===

// Log format options
enum class LogFormat
{
    TIMESTAMP_ARCHIVE_FILENAME = 0,   // Print '<timestamp> <MPQ archive>: <filename>'
    ARCHIVE_FILENAME = 1,             // Print '<MPQ archive>: <filename>'
    TIMESTAMP_FILENAME = 2,           // Print '<timestamp> <filename>'
    FILENAME_ONLY = 3                 // Print '<filename>'
};

// Target game options (determines which Storm.dll ordinals to use)
enum class TargetGame
{
    DIABLO_1 = 0,  // Diablo I (uses different ordinals)
    LATER = 1      // StarCraft, Diablo II, Warcraft II, etc.
};

extern bool g_logUniqueOnly;
extern LogFormat g_logFormat;
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

