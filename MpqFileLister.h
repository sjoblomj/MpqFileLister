/*
    MpqFileLister - An MPQDraft plugin that logs all SFileOpenFile and SFileOpenFileEx calls

    This plugin hooks the Storm.dll SFileOpenFile and SFileOpenFileEx functions
    and logs every filename that the game attempts to open from MPQ archives.
*/

#ifndef MPQFILELISTER_H
#define MPQFILELISTER_H

#include <windows.h>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

// Unique plugin ID - randomly generated
constexpr uint32_t PLUGIN_ID = 0x4d51464c;  // "MQFL" in hex

// Plugin name - defined as macro to allow string literal concatenation
#define PLUGIN_NAME "MpqFileLister v1.1"

// Forward declaration of the interface
struct IMPQDraftPlugin;
struct IMPQDraftServer;

// Storm function signatures
// SFileOpenFile (ordinal 0x10B)
typedef BOOL (WINAPI *SFileOpenFilePtr)(
    LPCSTR lpFileName,
    HANDLE* hFile
);

// SFileOpenFileEx (ordinal 0x10C)
typedef BOOL (WINAPI *SFileOpenFileExPtr)(
    HANDLE hMpq,
    const char* szFileName,
    DWORD dwSearchScope,
    HANDLE* phFile
);

// The plugin class
class CMpqFileListerPlugin
{
private:
    HMODULE m_hThisModule;
    HMODULE m_hStorm;
    bool m_bInitialized;

    // Original function pointers (static for use in static hook functions)
    static SFileOpenFilePtr s_OriginalSFileOpenFile;
    static SFileOpenFileExPtr s_OriginalSFileOpenFileEx;

    // Logging (using standard C++)
    static std::ofstream s_logFile;
    static std::mutex s_logMutex;
    static std::string s_logFilePath;

    // Helper function for logging file access
    static void LogFileAccess(const char* fileName, HANDLE fileHandle);

    // Our hook functions
    static BOOL WINAPI HookedSFileOpenFile(
        LPCSTR lpFileName,
        HANDLE* hFile
    );

    static BOOL WINAPI HookedSFileOpenFileEx(
        HANDLE hMpq,
        const char* szFileName,
        DWORD dwSearchScope,
        HANDLE* phFile
    );

public:
    CMpqFileListerPlugin();
    ~CMpqFileListerPlugin();

    void SetThisModule(HMODULE hModule) { m_hThisModule = hModule; }

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
