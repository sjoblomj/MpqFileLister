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

// SFileLoadFile - loads an entire file by name into a Storm-allocated buffer
typedef BOOL (WINAPI *SFileLoadFilePtr)(
    LPCSTR lpFileName,
    LPVOID* lplpFileData,
    LPDWORD lpdwFileSize,
    DWORD dwFlags1,
    DWORD dwFlags2
);

// SFileLoadFileEx - same as SFileLoadFile, but targets a specific archive/search scope
typedef BOOL (WINAPI *SFileLoadFileExPtr)(
    HANDLE hMpq,
    LPCSTR lpFileName,
    LPVOID* lplpFileData,
    LPDWORD lpdwFileSize,
    DWORD dwFlags1,
    DWORD dwFlags2,
    LPVOID lpOverlapped
);

// SBmpLoadImage - fetches an image by name, decoding it into a caller-supplied buffer
typedef BOOL (WINAPI *SBmpLoadImagePtr)(
    LPCSTR lpFileName,
    LPPALETTEENTRY lpPalette,
    LPBYTE lpBits,
    DWORD dwBitsSize,
    LPDWORD lpdwWidth,
    LPDWORD lpdwHeight,
    LPDWORD lpdwBpp
);

// SBmpAllocLoadImage - same as SBmpLoadImage, but Storm allocates the pixel buffer.
// The alloc-callback parameter is passed through untouched, so it's typed as an
// opaque pointer here rather than a fully-specified callback signature.
typedef BOOL (WINAPI *SBmpAllocLoadImagePtr)(
    LPCSTR lpFileName,
    LPPALETTEENTRY lpPalette,
    LPBYTE* lplpBits,
    LPDWORD lpdwWidth,
    LPDWORD lpdwHeight,
    LPDWORD lpdwBpp,
    LPDWORD lpdwSize,
    LPVOID lpAllocProc
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
    static SFileLoadFilePtr s_OriginalSFileLoadFile;
    static SFileLoadFileExPtr s_OriginalSFileLoadFileEx;
    static SBmpLoadImagePtr s_OriginalSBmpLoadImage;
    static SBmpAllocLoadImagePtr s_OriginalSBmpAllocLoadImage;

    // Logging (using standard C++)
    static std::ofstream s_logFile;
    static std::mutex s_logMutex;
    static std::string s_logFilePath;

    // Helper function for logging file access.
    // callName is the Storm.dll function that was called (e.g. "SFileOpenFileEx");
    // nonPointerParams and pointerParams are pre-formatted, human-readable summaries
    // of that call's other arguments - the former for parameters that aren't pointers
    // (e.g. "dwSearchScope=0x0"), the latter for parameters that are (e.g.
    // "phFile=0x28fe1c (deref=0x1f4)") - used to satisfy the %p and %P format
    // placeholders respectively. See FormatLogEntry() in MpqFileLister.cpp.
    static void LogFileAccess(const char* fileName, HANDLE fileHandle, const char* callName,
                               const std::string& nonPointerParams, const std::string& pointerParams);

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

    static BOOL WINAPI HookedSFileLoadFile(
        LPCSTR lpFileName,
        LPVOID* lplpFileData,
        LPDWORD lpdwFileSize,
        DWORD dwFlags1,
        DWORD dwFlags2
    );

    static BOOL WINAPI HookedSFileLoadFileEx(
        HANDLE hMpq,
        LPCSTR lpFileName,
        LPVOID* lplpFileData,
        LPDWORD lpdwFileSize,
        DWORD dwFlags1,
        DWORD dwFlags2,
        LPVOID lpOverlapped
    );

    static BOOL WINAPI HookedSBmpLoadImage(
        LPCSTR lpFileName,
        LPPALETTEENTRY lpPalette,
        LPBYTE lpBits,
        DWORD dwBitsSize,
        LPDWORD lpdwWidth,
        LPDWORD lpdwHeight,
        LPDWORD lpdwBpp
    );

    static BOOL WINAPI HookedSBmpAllocLoadImage(
        LPCSTR lpFileName,
        LPPALETTEENTRY lpPalette,
        LPBYTE* lplpBits,
        LPDWORD lpdwWidth,
        LPDWORD lpdwHeight,
        LPDWORD lpdwBpp,
        LPDWORD lpdwSize,
        LPVOID lpAllocProc
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
