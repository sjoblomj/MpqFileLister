/*
    MpqFileLister - An MPQDraft plugin that logs all SFileOpenFile,
    SFileOpenFileEx and SVidPlayBegin calls

    This plugin hooks the Storm.dll SFileOpenFile, SFileOpenFileEx and
    SVidPlayBegin functions and logs every filename that the game attempts to
    open from MPQ archives.
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

/* Storm function signatures */
// SFileOpenFile
typedef BOOL (WINAPI *SFileOpenFilePtr)(
    LPCSTR lpFileName,
    HANDLE* hFile
);

// SFileOpenFileEx
typedef BOOL (WINAPI *SFileOpenFileExPtr)(
    HANDLE hMpq,
    const char* szFileName,
    DWORD dwSearchScope,
    HANDLE* phFile
);

// SVidPlayBegin
typedef BOOL (WINAPI *SVidPlayBeginPtr)(
    char *filename,
    int a2,
    int* a3,
    int* a4,
    int* a5,
    int flags,
    HANDLE* video
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
    static SVidPlayBeginPtr s_OriginalSVidPlayBegin;

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

    static BOOL WINAPI HookedSVidPlayBegin(
        char *filename,
        int a2,
        int* a3,
        int* a4,
        int* a5,
        int flags,
        HANDLE* video
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
