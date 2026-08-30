/*
    MpqFileLister - An MPQDraft plugin that logs all file access attempts to Storm.dll
*/

#ifndef MPQFILELISTER_H
#define MPQFILELISTER_H

#include <windows.h>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

// Unique plugin ID
constexpr uint32_t PLUGIN_ID = 0x4d51464c;  // "MQFL" in hex

// Plugin name - defined as macro to allow string literal concatenation
#define PLUGIN_NAME "MpqFileLister v1.1"

// Forward declaration of the interface
struct IMPQDraftPlugin;
struct IMPQDraftServer;

// The plugin class
class CMpqFileListerPlugin
{
private:
    HMODULE m_hThisModule;
    HMODULE m_hStorm;
    bool m_bInitialized;

    // Logging (using standard C++)
    static std::ofstream s_logFile;
    static std::mutex s_logMutex;
    static std::string s_logFilePath;

public:
    CMpqFileListerPlugin();
    ~CMpqFileListerPlugin();

    void SetThisModule(HMODULE hModule) { m_hThisModule = hModule; }

    // Logs one file access. Called by the Hooked* functions in Hooks.cpp - this
    // class knows nothing about which Storm.dll functions exist; see Hooks.h.
    // callName is the Storm.dll function that was called (e.g. "SFileOpenFileEx");
    // nonPointerParams and pointerParams are pre-formatted, human-readable summaries
    // of that call's other arguments - the former for parameters that aren't pointers
    // (e.g. "dwSearchScope=0x0"), the latter for parameters that are (e.g.
    // "phFile=0x28fe1c (deref=0x1f4)") - used to satisfy the %p and %P format
    // placeholders respectively. See FormatLogEntry() in LogFormat.cpp.
    static void LogFileAccess(const char* fileName, HANDLE fileHandle, const char* callName,
                               const std::string& nonPointerParams, const std::string& pointerParams);

    // IMPQDraftPlugin interface methods
    BOOL WINAPI Identify(DWORD* lpdwPluginID);
    BOOL WINAPI GetPluginName(char* lpszPluginName, DWORD nNameBufferLength);
    BOOL WINAPI CanPatchExecutable(const char* lpszEXEFileName);
    BOOL WINAPI Configure(HWND hParentWnd);
    BOOL WINAPI ReadyForPatch();
    BOOL WINAPI GetModules(void* lpPluginModules, DWORD* lpnNumModules);
    BOOL WINAPI InitializePlugin(IMPQDraftServer* lpMPQDraftServer);
    BOOL WINAPI TerminatePlugin();
};

// Global plugin instance
extern CMpqFileListerPlugin g_MpqFileLister;

#endif // MPQFILELISTER_H
