/*
    Config.h - Configuration management for MpqFileLister plugin
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>
#include <string>

// === Configuration variables ===
extern bool g_logUniqueOnly;
extern bool g_printMpqArchive;
extern std::string g_logFileName;

// === Configuration functions ===

// Initialize the config file path based on the DLL location
void InitConfigPath(HMODULE hModule);

// Load configuration from the INI file
void LoadConfig();

// Save configuration to the INI file
void SaveConfig();

#endif // CONFIG_H

