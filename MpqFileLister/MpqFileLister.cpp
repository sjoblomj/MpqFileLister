/*
    MpqFileLister - An MPQDraft plugin that logs all SFileOpenFileEx calls
*/

#include "MpqFileLister.h"
#include "MPQDraftPlugin.h"
#include "QHookAPI.h"
#include "Config.h"
#include "ConfigDialog.h"
#include <filesystem>
#include <cstring>
#include <unordered_set>

// Storm.dll ordinals
static constexpr uint32_t SFILEOPENFILEEX_ORDINAL = 0x10C;       // 268
static constexpr uint32_t SFILEGETFILEARCHIVE_ORDINAL = 0x108;   // 264
static constexpr uint32_t SFILEGETARCHIVENAME_ORDINAL = 0x113;   // 275

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

    // Log the filename and archive
    if (szFileName && s_logFile.is_open())
    {
        std::lock_guard<std::mutex> lock(s_logMutex);

        // Build the log entry: "filename" or "archive.mpq: filename"
        std::string logEntry = szFileName;

        // Try to get the archive name if the file was found and the option is enabled
        if (g_printMpqArchive && result && phFile && *phFile && s_SFileGetFileArchive && s_SFileGetArchiveName)
        {
            // Get the archive handle from the file handle
            HANDLE hArchive = nullptr;
            if (s_SFileGetFileArchive(*phFile, &hArchive) && hArchive)
            {
                char archiveName[MAX_PATH] = {0};
                if (s_SFileGetArchiveName(hArchive, archiveName, MAX_PATH) && archiveName[0])
                {
                    // Extract just the filename from the full path
                    std::filesystem::path archivePath(archiveName);
                    logEntry = archivePath.filename().string() + ": " + logEntry;
                }
            }
        }

        bool shouldLog = true;
        if (g_logUniqueOnly)
        {
            // Only log if we haven't seen this entry before
            auto [it, inserted] = s_seenFiles.insert(logEntry);
            shouldLog = inserted;
        }

        if (shouldLog)
        {
            s_logFile << logEntry << "\n";
            s_logFile.flush();
        }
    }

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

    // Get the original function pointer using ordinal
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    s_OriginalSFileOpenFileEx = reinterpret_cast<SFileOpenFileExPtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)SFILEOPENFILEEX_ORDINAL)));

    if (!s_OriginalSFileOpenFileEx)
    {
        if (s_logFile.is_open())
        {
            s_logFile << "ERROR: SFileOpenFileEx not found in Storm.dll\n";
        }
        return TRUE;  // Return TRUE to not abort the patch
    }

    // Get SFileGetFileArchive and SFileGetArchiveName for logging which MPQ files come from (optional)
    s_SFileGetFileArchive = reinterpret_cast<SFileGetFileArchivePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)SFILEGETFILEARCHIVE_ORDINAL)));
    s_SFileGetArchiveName = reinterpret_cast<SFileGetArchiveNamePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)SFILEGETARCHIVENAME_ORDINAL)));

    // Patch the import table to redirect calls to our hook
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    HMODULE hHostProcess = GetModuleHandle(nullptr);
    PatchImportEntry(
        hHostProcess,
        "Storm.dll",
        reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSFileOpenFileEx)),
        reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileOpenFileEx)),
        TRUE  // Recursive - patch all loaded modules
    );

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
