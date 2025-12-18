/*
    MpqFileLister - An MPQDraft plugin that logs all SFileOpenFile and SFileOpenFileEx calls
*/

#include "MpqFileLister.h"
#include "MPQDraftPlugin.h"
#include "QHookAPI.h"
#include "Config.h"
#include "ConfigDialog.h"
#include <filesystem>
#include <cstring>
#include <unordered_set>
#include <chrono>
#include <sstream>
#include <iomanip>

// Storm.dll ordinals
static constexpr uint32_t SFILEOPENFILE_D1_ORDINAL       = 0x4E;    // 78
static constexpr uint32_t SFILEOPENFILEEX_D1_ORDINAL     = 0x4F;    // 79
static constexpr uint32_t SFILEGETFILEARCHIVE_D1_ORDINAL = 0x4B;    // 75
static constexpr uint32_t SFILEGETARCHIVENAME_D1_ORDINAL = 0x56;    // 86
static constexpr uint32_t SFILEOPENFILE_ORDINAL          = 0x10B;   // 267
static constexpr uint32_t SFILEOPENFILEEX_ORDINAL        = 0x10C;   // 268
static constexpr uint32_t SFILEGETFILEARCHIVE_ORDINAL    = 0x108;   // 264
static constexpr uint32_t SFILEGETARCHIVENAME_ORDINAL    = 0x113;   // 275

// Function pointer types for archive name lookup
// BOOL SFileGetFileArchive(HANDLE hFile, HANDLE* phArchive)
using SFileGetFileArchivePtr = BOOL (WINAPI*)(HANDLE, HANDLE*);
// BOOL SFileGetArchiveName(HANDLE hArchive, char* szArchiveName, DWORD dwBufferSize)
using SFileGetArchiveNamePtr = BOOL (WINAPI*)(HANDLE, char*, DWORD);

static SFileGetFileArchivePtr s_SFileGetFileArchive = nullptr;
static SFileGetArchiveNamePtr s_SFileGetArchiveName = nullptr;

// Global plugin instance
CMpqFileListerPlugin g_MpqFileLister;

// Static member initialization
SFileOpenFilePtr CMpqFileListerPlugin::s_OriginalSFileOpenFile = nullptr;
SFileOpenFileExPtr CMpqFileListerPlugin::s_OriginalSFileOpenFileEx = nullptr;
std::ofstream CMpqFileListerPlugin::s_logFile;
std::mutex CMpqFileListerPlugin::s_logMutex;
std::string CMpqFileListerPlugin::s_logFilePath;

// Set to track seen filenames (used when g_logUniqueOnly is true)
static std::unordered_set<std::string> s_seenFiles;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    (void)lpReserved;

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            g_MpqFileLister.SetThisModule(hModule);
            DisableThreadLibraryCalls(hModule);
            InitConfigPath(hModule);
            LoadConfig();
            break;
        case DLL_PROCESS_DETACH:
            // Ensure cleanup happens
            g_MpqFileLister.TerminatePlugin();
            break;
    }
    return TRUE;
}

// IMPQDraftPlugin wrapper class to provide the vtable interface
class CMpqFileListerPluginInterface : public IMPQDraftPlugin
{
public:
    BOOL WINAPI Identify(DWORD* lpdwPluginID) override
        { return g_MpqFileLister.Identify(lpdwPluginID); }
    BOOL WINAPI GetPluginName(char* lpszPluginName, DWORD nNameBufferLength) override
        { return g_MpqFileLister.GetPluginName(lpszPluginName, nNameBufferLength); }
    BOOL WINAPI CanPatchExecutable(const char* lpszEXEFileName) override
        { return g_MpqFileLister.CanPatchExecutable(lpszEXEFileName); }
    BOOL WINAPI Configure(HWND hParentWnd) override
        { return g_MpqFileLister.Configure(hParentWnd); }
    BOOL WINAPI ReadyForPatch() override
        { return g_MpqFileLister.ReadyForPatch(); }
    BOOL WINAPI GetModules(MPQDRAFTPLUGINMODULE* lpPluginModules, DWORD* lpnNumModules) override
        { return g_MpqFileLister.GetModules(lpPluginModules, lpnNumModules); }
    BOOL WINAPI InitializePlugin(IMPQDraftServer* lpMPQDraftServer) override
        { return g_MpqFileLister.InitializePlugin(lpMPQDraftServer); }
    BOOL WINAPI TerminatePlugin() override
        { return g_MpqFileLister.TerminatePlugin(); }
};

static CMpqFileListerPluginInterface g_PluginInterface;

// Required export - this is how MPQDraft discovers the plugin
extern "C" __declspec(dllexport) BOOL WINAPI GetMPQDraftPlugin(IMPQDraftPlugin** lppMPQDraftPlugin)
{
    if (!lppMPQDraftPlugin)
        return FALSE;

    *lppMPQDraftPlugin = &g_PluginInterface;
    return TRUE;
}

CMpqFileListerPlugin::CMpqFileListerPlugin()
    : m_hThisModule(nullptr)
    , m_hStorm(nullptr)
    , m_bInitialized(false)
{
}

CMpqFileListerPlugin::~CMpqFileListerPlugin()
{
    TerminatePlugin();
}

BOOL WINAPI CMpqFileListerPlugin::Identify(DWORD* lpdwPluginID)
{
    if (!lpdwPluginID)
        return FALSE;

    *lpdwPluginID = PLUGIN_ID;
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::GetPluginName(char* lpszPluginName, DWORD nNameBufferLength)
{
    if (!lpszPluginName)
        return FALSE;

    const char* name = PLUGIN_NAME;
    if (nNameBufferLength < strlen(name) + 1)
        return FALSE;

    strcpy(lpszPluginName, name);
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::CanPatchExecutable(const char* lpszEXEFileName)
{
    (void)lpszEXEFileName;
    // This plugin can work with any executable that uses Storm.dll
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::Configure(HWND hParentWnd)
{
    ShowConfigDialog(hParentWnd, m_hThisModule);
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::ReadyForPatch()
{
    // Always ready - configuration is available but never required, since we use defaults
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::GetModules(void* lpPluginModules, DWORD* lpnNumModules)
{
    (void)lpPluginModules;
    if (!lpnNumModules)
        return FALSE;

    // No additional modules needed
    *lpnNumModules = 0;
    return TRUE;
}

// Helper function to get Unix epoch timestamp in milliseconds
static std::string GetTimestampMs()
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(ms);
}

// Helper function to log file access (shared by both hook functions)
void CMpqFileListerPlugin::LogFileAccess(const char* fileName, HANDLE fileHandle)
{
    if (!fileName || !s_logFile.is_open())
        return;

    std::lock_guard<std::mutex> lock(s_logMutex);

    // Build the log entry based on the selected format
    std::string logEntry;
    std::string archiveName;

    // Get archive name if needed for the format
    bool needArchive = (g_logFormat == LogFormat::TIMESTAMP_ARCHIVE_FILENAME ||
                        g_logFormat == LogFormat::ARCHIVE_FILENAME);

    if (needArchive && fileHandle && s_SFileGetFileArchive && s_SFileGetArchiveName)
    {
        HANDLE hArchive = nullptr;
        if (s_SFileGetFileArchive(fileHandle, &hArchive) && hArchive)
        {
            char archiveNameBuf[MAX_PATH] = {0};
            if (s_SFileGetArchiveName(hArchive, archiveNameBuf, MAX_PATH) && archiveNameBuf[0])
            {
                // Extract just the filename from the full path
                std::filesystem::path archivePath(archiveNameBuf);
                archiveName = archivePath.filename().string();
            }
        }
    }

    // Build the uniqueness key (without timestamp) for duplicate detection
    std::string uniqueKey;
    if (!archiveName.empty())
        uniqueKey = archiveName + ": " + fileName;
    else
        uniqueKey = fileName;

    // Check if we should log this entry
    bool shouldLog = true;
    if (g_logUniqueOnly)
    {
        // Only log if we haven't seen this entry before (based on uniqueKey, not timestamp)
        auto [it, inserted] = s_seenFiles.insert(uniqueKey);
        shouldLog = inserted;
    }

    if (!shouldLog)
        return;

    // Build the log entry according to the selected format
    switch (g_logFormat)
    {
        case LogFormat::TIMESTAMP_ARCHIVE_FILENAME:
            logEntry = GetTimestampMs() + " ";
            if (!archiveName.empty())
                logEntry += archiveName + ": ";
            logEntry += fileName;
            break;

        case LogFormat::ARCHIVE_FILENAME:
            if (!archiveName.empty())
                logEntry = archiveName + ": " + fileName;
            else
                logEntry = fileName;
            break;

        case LogFormat::TIMESTAMP_FILENAME:
            logEntry = GetTimestampMs() + " " + fileName;
            break;

        case LogFormat::FILENAME_ONLY:
            logEntry = fileName;
            break;
    }

    s_logFile << logEntry << "\n";
    s_logFile.flush();
}

// The hook function - this is called instead of the original SFileOpenFile
BOOL WINAPI CMpqFileListerPlugin::HookedSFileOpenFile(
    LPCSTR lpFileName,
    HANDLE* hFile)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSFileOpenFile)
        result = s_OriginalSFileOpenFile(lpFileName, hFile);

    // Log the file access
    if (result && hFile && *hFile)
        LogFileAccess(lpFileName, *hFile);

    return result;
}

// The hook function - this is called instead of the original SFileOpenFileEx
BOOL WINAPI CMpqFileListerPlugin::HookedSFileOpenFileEx(
    HANDLE hMpq,
    const char* szFileName,
    DWORD dwSearchScope,
    HANDLE* phFile)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSFileOpenFileEx)
        result = s_OriginalSFileOpenFileEx(hMpq, szFileName, dwSearchScope, phFile);

    // Log the file access
    if (result && phFile && *phFile)
        LogFileAccess(szFileName, *phFile);

    return result;
}

BOOL WINAPI CMpqFileListerPlugin::InitializePlugin(IMPQDraftServer* lpMPQDraftServer)
{
    (void)lpMPQDraftServer;

    if (m_bInitialized)
        return TRUE;

    // Build log file path
    // If g_logFileName is an absolute path, use it directly
    // Otherwise, place it in the game's directory
    std::filesystem::path logPath(g_logFileName);
    if (logPath.is_absolute())
    {
        s_logFilePath = g_logFileName;
    }
    else
    {
        std::string exePath(MAX_PATH, '\0');
        DWORD len = GetModuleFileNameA(nullptr, exePath.data(), MAX_PATH);
        if (len > 0)
        {
            exePath.resize(len);
            std::filesystem::path gamePath(exePath);
            s_logFilePath = (gamePath.parent_path() / g_logFileName).string();
        }
        else
        {
            // Fallback to just the filename in the current directory
            s_logFilePath = g_logFileName;
        }
    }

    // Open the log file
    s_logFile.open(s_logFilePath, std::ios::out | std::ios::trunc);

    // Find Storm.dll
    m_hStorm = GetModuleHandleA("Storm");
    if (!m_hStorm)
        m_hStorm = GetModuleHandleA("storm.dll");
    if (!m_hStorm)
        m_hStorm = GetModuleHandleA("Storm.dll");

    if (!m_hStorm)
    {
        // Storm is not loaded - can't hook
        if (s_logFile.is_open())
        {
            s_logFile << "ERROR: Storm.dll not found\n";
        }
        return TRUE;  // Return TRUE to not abort the patch
    }

    // Select ordinals based on target game
    uint32_t sFileOpenFileOrdinal;
    uint32_t sFileOpenFileExOrdinal;
    uint32_t sFileGetFileArchiveOrdinal;
    uint32_t sFileGetArchiveNameOrdinal;

    if (g_targetGame == TargetGame::DIABLO_1)
    {
        sFileOpenFileOrdinal = SFILEOPENFILE_D1_ORDINAL;
        sFileOpenFileExOrdinal = SFILEOPENFILEEX_D1_ORDINAL;
        sFileGetFileArchiveOrdinal = SFILEGETFILEARCHIVE_D1_ORDINAL;
        sFileGetArchiveNameOrdinal = SFILEGETARCHIVENAME_D1_ORDINAL;
    }
    else // TargetGame::LATER
    {
        sFileOpenFileOrdinal = SFILEOPENFILE_ORDINAL;
        sFileOpenFileExOrdinal = SFILEOPENFILEEX_ORDINAL;
        sFileGetFileArchiveOrdinal = SFILEGETFILEARCHIVE_ORDINAL;
        sFileGetArchiveNameOrdinal = SFILEGETARCHIVENAME_ORDINAL;
    }

    // Get the original function pointers using ordinals
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    s_OriginalSFileOpenFile = reinterpret_cast<SFileOpenFilePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileOpenFileOrdinal)));

    s_OriginalSFileOpenFileEx = reinterpret_cast<SFileOpenFileExPtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileOpenFileExOrdinal)));

    if (!s_OriginalSFileOpenFile && !s_OriginalSFileOpenFileEx)
    {
        if (s_logFile.is_open())
        {
            s_logFile << "ERROR: Neither SFileOpenFile nor SFileOpenFileEx found in Storm.dll\n";
        }
        return TRUE;  // Return TRUE to not abort the patch
    }

    // Get SFileGetFileArchive and SFileGetArchiveName for logging which MPQ files come from (optional)
    s_SFileGetFileArchive = reinterpret_cast<SFileGetFileArchivePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetFileArchiveOrdinal)));
    s_SFileGetArchiveName = reinterpret_cast<SFileGetArchiveNamePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetArchiveNameOrdinal)));

    // Patch the import table to redirect calls to our hooks
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    HMODULE hHostProcess = GetModuleHandle(nullptr);

    if (s_OriginalSFileOpenFile)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSFileOpenFile)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileOpenFile)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    if (s_OriginalSFileOpenFileEx)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSFileOpenFileEx)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileOpenFileEx)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    m_bInitialized = true;
    return TRUE;
}

BOOL WINAPI CMpqFileListerPlugin::TerminatePlugin()
{
    if (!m_bInitialized)
        return TRUE;

    // Clear the seen files set
    s_seenFiles.clear();

    m_bInitialized = false;
    return TRUE;
}
