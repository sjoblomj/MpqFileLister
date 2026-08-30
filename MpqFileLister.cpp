/*
    MpqFileLister - An MPQDraft plugin that logs all file access attempts to Storm.dll
*/

#include "MpqFileLister.h"
#include "MPQDraftPlugin.h"
#include "QHookAPI.h"
#include "Config.h"
#include "ConfigDialog.h"
#include "Utils.h"
#include "LogFormat.h"
#include "Hooks.h"
#include <filesystem>
#include <cstring>
#include <unordered_set>
#include <vector>

// SFileGetFileArchive/SFileGetArchiveName aren't hooked (nothing gets patched) -
// they're only resolved so LogFileAccess() can look up which archive a file came
// from, so they stay here rather than in Hooks.h/cpp alongside the actual hooks.
static constexpr uint32_t SFILEGETFILEARCHIVE_D1_ORDINAL = 0x4B;    // 75
static constexpr uint32_t SFILEGETARCHIVENAME_D1_ORDINAL = 0x0;     // not exported by D1's Storm.dll
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

// Helper function to log file access (shared by all hook functions)
void CMpqFileListerPlugin::LogFileAccess(const char* fileName, HANDLE fileHandle,
                                         const char* callName, const std::string& nonPointerParams,
                                         const std::string& pointerParams)
{
    if (!fileName || !s_logFile.is_open())
        return;

    std::lock_guard<std::mutex> lock(s_logMutex);

    // Look up the archive name, if the format asks for it and it can be resolved.
    // Not every call yields an HSFILE (e.g. SVidPlayBegin, SFileLoadFile), and
    // SFileGetArchiveName is not exported by Diablo I's Storm.dll at all - in
    // both cases %a simply expands to an empty string.
    std::string archiveName;
    bool needArchive = g_logFormat.find("%a") != std::string::npos;

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

    // Build the uniqueness key for duplicate detection. This mirrors what the log
    // format actually distinguishes: the key always excludes the timestamp (so
    // %t/%T/%t{...} varying between accesses doesn't defeat deduplication), and
    // includes the archive name only when the format requests it via %a - if the
    // user isn't asking to see which archive a file came from, two identically
    // named files from different archives are correctly treated as the same entry.
    std::string uniqueKey;
    if (!archiveName.empty())
        uniqueKey = archiveName + ": " + fileName;
    else
        uniqueKey = fileName;

    // Check if we should log this entry
    if (g_logUniqueOnly)
    {
        // Only log if we haven't seen this entry before (based on uniqueKey, not timestamp)
        auto [it, inserted] = s_seenFiles.insert(uniqueKey);
        if (!inserted)
            return;
    }

    std::string logEntry = FormatLogEntry(g_logFormat, archiveName, fileName,
                                           callName ? callName : "",
                                           nonPointerParams, pointerParams);

    s_logFile << logEntry << "\n";
    s_logFile.flush();
}

// MPQDraft only ever calls this once per patched process (from LoadPlugins() in its
// injected DLL) - it never re-initializes a plugin after calling TerminatePlugin(),
// so s_logFile.open() below doesn't need to guard against being called on an
// already-open stream. See TerminatePlugin() for why.
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
        std::string exePath = GetModulePathSafe(nullptr);
        if (!exePath.empty())
        {
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

    // All knowledge of which Storm.dll functions get hooked - their names, ordinals
    // and replacement functions - lives in Hooks.cpp; this function only knows how
    // to resolve and patch whatever entries it's handed.
    const std::vector<HookEntry>& hooks = GetHookEntries();

    // Resolve the original function pointers using ordinals. An ordinal of 0 means
    // the function is known not to exist for the selected target (e.g. SFileLoadFile
    // on Diablo I) - resolution and warnings are skipped for those rather than
    // treated as an unexpected lookup failure.
    bool anyHooked = false;
    for (const HookEntry& hook : hooks)
    {
        uint32_t ordinal = (g_targetGame == TargetGame::DIABLO_1) ? hook.d1Ordinal : hook.laterOrdinal;
        if (!ordinal)
            continue;

        // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
        void* original = reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)ordinal));
        *hook.originalPtrSlot = original;

        if (!original)
        {
            if (s_logFile.is_open())
                s_logFile << "WARNING: " << hook.name << " not found in Storm.dll - not hooked\n";
            continue;
        }

        anyHooked = true;
    }

    if (!anyHooked)
    {
        if (s_logFile.is_open())
            s_logFile << "ERROR: None of the " << hooks.size() << " configured Storm.dll functions were found\n";
        return TRUE;  // Return TRUE to not abort the patch
    }

    // Get SFileGetFileArchive and SFileGetArchiveName for logging which MPQ files come from
    // (optional - e.g. SFileGetArchiveName does not exist on Diablo I's Storm.dll, ordinal 0)
    bool isD1 = (g_targetGame == TargetGame::DIABLO_1);
    uint32_t sFileGetFileArchiveOrdinal = isD1 ? SFILEGETFILEARCHIVE_D1_ORDINAL : SFILEGETFILEARCHIVE_ORDINAL;
    uint32_t sFileGetArchiveNameOrdinal = isD1 ? SFILEGETARCHIVENAME_D1_ORDINAL : SFILEGETARCHIVENAME_ORDINAL;

    s_SFileGetFileArchive = reinterpret_cast<SFileGetFileArchivePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetFileArchiveOrdinal)));
    if (sFileGetArchiveNameOrdinal)
    {
        s_SFileGetArchiveName = reinterpret_cast<SFileGetArchiveNamePtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetArchiveNameOrdinal)));
    }

    // Patch the import table to redirect calls to our hooks
    HMODULE hHostProcess = GetModuleHandle(nullptr);
    for (const HookEntry& hook : hooks)
    {
        void* original = *hook.originalPtrSlot;
        if (!original)
            continue;

        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(original),
            hook.hookFn,
            TRUE  // Recursive - patch all loaded modules
        );
    }

    m_bInitialized = true;
    return TRUE;
}

// Does not revert the PatchImportEntry hooks installed in InitializePlugin(), and
// doesn't need to: MPQDraft's own source (src/dll/MPQDraftDLL.cpp) documents that
// it deliberately never unloads plugin DLLs mid-session, since unpatching a hooked
// function while another thread might still call it is unsafe - the call that
// would do so (UnloadPlugins(), invoked from PatchExitProcess) is commented out
// there on purpose. This plugin's TerminatePlugin() only ever runs once, from its
// own DllMain's DLL_PROCESS_DETACH, as the process is already exiting.
BOOL WINAPI CMpqFileListerPlugin::TerminatePlugin()
{
    if (!m_bInitialized)
        return TRUE;

    // Clear the seen files set
    s_seenFiles.clear();

    m_bInitialized = false;
    return TRUE;
}
