/*
    MpqFileLister - An MPQDraft plugin that logs all file access attempts to Storm.dll
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
#include <vector>
#include <ctime>
#include <cstdio>

// Storm.dll ordinals
static constexpr uint32_t SFILEOPENFILE_D1_ORDINAL       = 0x4E;    // 78
static constexpr uint32_t SFILEOPENFILEEX_D1_ORDINAL     = 0x4F;    // 79
static constexpr uint32_t SFILEGETFILEARCHIVE_D1_ORDINAL = 0x4B;    // 75
static constexpr uint32_t SFILEGETARCHIVENAME_D1_ORDINAL = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SVIDPLAYBEGIN_D1_ORDINAL       = 0x9D;    // 157
static constexpr uint32_t SFILEOPENFILE_ORDINAL          = 0x10B;   // 267
static constexpr uint32_t SFILEOPENFILEEX_ORDINAL        = 0x10C;   // 268
static constexpr uint32_t SFILEGETFILEARCHIVE_ORDINAL    = 0x108;   // 264
static constexpr uint32_t SFILEGETARCHIVENAME_ORDINAL    = 0x113;   // 275
static constexpr uint32_t SVIDPLAYBEGIN_ORDINAL          = 0x1C6;   // 454

static constexpr uint32_t SFILELOADFILE_D1_ORDINAL       = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SFILELOADFILEEX_D1_ORDINAL     = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SBMPLOADIMAGE_D1_ORDINAL       = 0x8;     // 8
static constexpr uint32_t SBMPALLOCLOADIMAGE_D1_ORDINAL  = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SFILELOADFILE_ORDINAL          = 0x117;   // 279
static constexpr uint32_t SFILELOADFILEEX_ORDINAL        = 0x119;   // 281
static constexpr uint32_t SBMPLOADIMAGE_ORDINAL          = 0x143;   // 323
static constexpr uint32_t SBMPALLOCLOADIMAGE_ORDINAL     = 0x145;   // 325

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
SVidPlayBeginPtr CMpqFileListerPlugin::s_OriginalSVidPlayBegin = nullptr;
SFileLoadFilePtr CMpqFileListerPlugin::s_OriginalSFileLoadFile = nullptr;
SFileLoadFileExPtr CMpqFileListerPlugin::s_OriginalSFileLoadFileEx = nullptr;
SBmpLoadImagePtr CMpqFileListerPlugin::s_OriginalSBmpLoadImage = nullptr;
SBmpAllocLoadImagePtr CMpqFileListerPlugin::s_OriginalSBmpAllocLoadImage = nullptr;
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

// Joins "key=value" style strings into a single ", "-separated string, e.g. for %p/%P
static std::string JoinParts(const std::vector<std::string>& parts)
{
    std::string result;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
            result += ", ";
        result += parts[i];
    }
    return result;
}

// Formats a raw pointer value for the log, e.g. "0x28fe1c", or "(null)".
static std::string PointerToString(const void* ptr)
{
    if (!ptr)
        return "(null)";
    std::ostringstream oss;
    oss << ptr;
    return oss.str();
}

// For a pointer parameter whose target either isn't a simple scalar (a raw byte/pixel
// buffer, a 256-entry palette array, an opaque structure) or whose validity we can't
// establish (an undocumented parameter of unknown lifetime, a callback/function pointer):
// report the address only. Dereferencing any of these would either be meaningless, risk
// crashing the hooked game, or require dumping arbitrary binary data into a text log.
static std::string PointerOnly(const char* name, const void* ptr)
{
    return std::string(name) + "=" + PointerToString(ptr);
}

// For a pointer-to-DWORD output parameter that a successful call is known to populate
// (e.g. SBmpLoadImage's lpdwWidth): report both the address and the value it points to.
static std::string PointerWithDword(const char* name, const DWORD* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr)
        s += " (deref=" + std::to_string(*ptr) + ")";
    return s;
}

// For a pointer-to-HANDLE output parameter (the common Storm "give me a handle back"
// idiom, e.g. SFileOpenFile's hFile): report both the address and the resulting handle.
static std::string PointerWithHandle(const char* name, HANDLE* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr && *ptr)
        s += " (deref=" + PointerToString(*ptr) + ")";
    return s;
}

// For a pointer-to-pointer output buffer (e.g. SFileLoadFile's lplpFileData): report the
// address plus the resulting buffer's address - one level of dereference to reveal where
// the data ended up, without ever dumping the buffer's (arbitrary-length, binary) contents.
static std::string PointerWithPointer(const char* name, void* const* ptr)
{
    std::string s = PointerOnly(name, ptr);
    if (ptr)
        s += " (deref=" + PointerToString(*ptr) + ")";
    return s;
}

// Renders the ISO-8601 UTC form of a timestamp, e.g. "2026-08-27T14:03:21.123Z"
static std::string FormatIso8601(const struct tm& utc, int millis)
{
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << millis << 'Z';
    return oss.str();
}

// Finds the index of the first unescaped '}' at or after `start` in `s`, treating
// an escaped "%}" pair as literal content rather than the sub-format's terminator.
// Returns std::string::npos if no unescaped '}' is found before the end of the string.
static size_t FindSubFormatEnd(const std::string& s, size_t start)
{
    for (size_t i = start; i < s.size(); ++i)
    {
        if (s[i] == '%' && i + 1 < s.size() && s[i + 1] == '}')
        {
            ++i; // skip the escaped pair - the loop's ++i moves past the '}' too
            continue;
        }
        if (s[i] == '}')
            return i;
    }
    return std::string::npos;
}

// Expands our own tokens inside a %t{...} sub-format before handing the result to
// strftime: %L becomes the zero-padded millisecond value, and %} becomes a literal
// '}' (needed since an escaped "%}" reaches here still in its escaped form - the
// terminator scan above only used it to know the span didn't end there). %% and
// every real strftime specifier are left completely untouched for strftime's own
// pass - this function never introduces a '%' character of its own, so it can't
// create a sequence strftime would misinterpret.
static std::string ExpandTimestampTokens(const std::string& subFormat, int millis)
{
    std::string result;
    result.reserve(subFormat.size());

    size_t i = 0;
    while (i < subFormat.size())
    {
        if (subFormat[i] == '%' && i + 1 < subFormat.size())
        {
            char next = subFormat[i + 1];
            if (next == 'L')
            {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%03d", millis);
                result += buf;
                i += 2;
                continue;
            }
            if (next == '}')
            {
                result += '}';
                i += 2;
                continue;
            }
        }
        result += subFormat[i];
        ++i;
    }

    return result;
}

// Renders a user-authored %t{...} sub-format via strftime, after expanding our own
// %L (milliseconds) and %} (literal brace) tokens. This CRT's strftime returns 0 for
// the *entire* call the moment the format contains any specifier it doesn't
// recognize or support - not just that one token - so a typo or an unsupported
// specifier (e.g. %T, which this CRT doesn't implement) would otherwise produce a
// silently blank timestamp. Instead, on failure the original, unresolved "%t{...}"
// text is returned, so a misconfigured format stays visible in the log rather than
// disappearing.
static std::string FormatCustomTimestamp(const std::string& subFormat, const struct tm& utc, int millis)
{
    std::string expanded = ExpandTimestampTokens(subFormat, millis);
    if (expanded.empty())
        return "";

    char buf[256];
    size_t n = strftime(buf, sizeof(buf), expanded.c_str(), &utc);
    if (n == 0)
        return "%t{" + subFormat + "}";

    return std::string(buf, n);
}

// Expands a user-supplied log format string, replacing placeholders with the
// values for one logged call:
//   %t       - timestamp, milliseconds since epoch
//   %T       - timestamp, ISO-8601 UTC, millisecond precision
//   %t{...}  - timestamp, custom strftime-style format; %L inside the braces is
//              replaced with milliseconds; see FormatCustomTimestamp() for what
//              happens if the format is invalid
//   %a       - MPQ archive name
//   %f       - filename
//   %c       - the Storm.dll call, e.g. SFileOpenFileEx
//   %p       - that call's non-pointer parameters
//   %P       - that call's pointer parameters
//   %%, %{, %} - a literal '%', '{' or '}' character
// Any other character (including an unrecognized "%x" sequence, which is kept
// as-is) is copied through unchanged. Operating purely on std::string with
// index-checked access means there is no fixed-size buffer to overflow,
// regardless of how long the format string or the substituted values are.
static std::string FormatLogEntry(const std::string& format, const std::string& archiveName,
                                  const std::string& fileName, const std::string& callName,
                                  const std::string& nonPointerParams, const std::string& pointerParams)
{
    // Computed once so every timestamp placeholder in this line reflects the same instant
    auto now = std::chrono::system_clock::now();
    auto msSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    time_t seconds = static_cast<time_t>(msSinceEpoch / 1000);
    int millis = static_cast<int>(msSinceEpoch % 1000);
    struct tm utc{};
    gmtime_s(&utc, &seconds);

    std::string result;
    result.reserve(format.size());

    size_t i = 0;
    while (i < format.size())
    {
        char c = format[i];
        if (c != '%' || i + 1 >= format.size())
        {
            result += c;
            ++i;
            continue;
        }

        char specifier = format[i + 1];

        // %t{...} - custom strftime-based timestamp sub-format
        if (specifier == 't' && i + 2 < format.size() && format[i + 2] == '{')
        {
            size_t contentStart = i + 3;
            size_t closePos = FindSubFormatEnd(format, contentStart);
            if (closePos != std::string::npos)
            {
                std::string subFormat = format.substr(contentStart, closePos - contentStart);
                result += FormatCustomTimestamp(subFormat, utc, millis);
                i = closePos + 1;
                continue;
            }
            // No closing '}' found: fall through and treat this as a bare %t: the
            // stray '{' is then just an ordinary character on the next iteration.
        }

        switch (specifier)
        {
            case 't': result += std::to_string(msSinceEpoch); break;
            case 'T': result += FormatIso8601(utc, millis);   break;
            case 'a': result += archiveName;                  break;
            case 'f': result += fileName;                     break;
            case 'c': result += callName;                     break;
            case 'p': result += nonPointerParams;             break;
            case 'P': result += pointerParams;                break;
            case '{': result += '{';                          break;
            case '}': result += '}';                          break;
            case '%': result += '%';                          break;
            default:
                // Unrecognized specifier: keep both characters literally
                result += '%';
                result += specifier;
                break;
        }
        i += 2;
    }

    return result;
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

    // Build the uniqueness key (independent of the chosen format/timestamp) for duplicate detection
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
    {
        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("lpFileName", lpFileName));
        ptrParts.push_back(PointerWithHandle("hFile", hFile));
        LogFileAccess(lpFileName, *hFile, "SFileOpenFile", "", JoinParts(ptrParts));
    }

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
    {
        std::ostringstream nonPtr;
        nonPtr << "dwSearchScope=0x" << std::hex << dwSearchScope;

        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("hMpq", hMpq));
        ptrParts.push_back(PointerOnly("szFileName", szFileName));
        ptrParts.push_back(PointerWithHandle("phFile", phFile));

        LogFileAccess(szFileName, *phFile, "SFileOpenFileEx", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// The hook function - this is called instead of the original SVidPlayBegin
BOOL WINAPI CMpqFileListerPlugin::HookedSVidPlayBegin(
    char *filename,
    int a2,
    int* a3,
    int* a4,
    int* a5,
    int flags,
    HANDLE* video)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSVidPlayBegin)
        result = s_OriginalSVidPlayBegin(filename, a2, a3, a4, a5, flags, video);

    // Log the file access. a2-a5 are undocumented/unnamed in this reverse-engineered
    // signature; a2 is a plain int, so it's reported by value like any other non-pointer
    // parameter. a3-a5 are pointers of unknown validity/lifetime, so they're reported as
    // raw addresses only, never dereferenced (dereferencing a pointer we don't understand
    // risks crashing the hooked game). video is reported the same way as the hFile/phFile
    // output-handle parameters elsewhere, since it follows the same "give me a handle
    // back" idiom.
    if (result)
    {
        std::ostringstream nonPtr;
        nonPtr << "a2=" << a2 << ", flags=0x" << std::hex << flags;

        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("filename", filename));
        ptrParts.push_back(PointerOnly("a3", a3));
        ptrParts.push_back(PointerOnly("a4", a4));
        ptrParts.push_back(PointerOnly("a5", a5));
        ptrParts.push_back(PointerWithHandle("video", video));

        LogFileAccess(filename, nullptr, "SVidPlayBegin", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// The hook function - this is called instead of the original SFileLoadFile
BOOL WINAPI CMpqFileListerPlugin::HookedSFileLoadFile(
    LPCSTR lpFileName,
    LPVOID* lplpFileData,
    LPDWORD lpdwFileSize,
    DWORD dwFlags1,
    DWORD dwFlags2)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSFileLoadFile)
        result = s_OriginalSFileLoadFile(lpFileName, lplpFileData, lpdwFileSize, dwFlags1, dwFlags2);

    // Log the file access (no HSFILE is returned by this API, so archive lookup is skipped)
    if (result)
    {
        std::ostringstream nonPtr;
        nonPtr << "dwFlags1=0x" << std::hex << dwFlags1 << ", dwFlags2=0x" << dwFlags2;

        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("lpFileName", lpFileName));
        ptrParts.push_back(PointerWithPointer("lplpFileData", reinterpret_cast<void* const*>(lplpFileData)));
        ptrParts.push_back(PointerWithDword("lpdwFileSize", lpdwFileSize));

        LogFileAccess(lpFileName, nullptr, "SFileLoadFile", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// The hook function - this is called instead of the original SFileLoadFileEx
BOOL WINAPI CMpqFileListerPlugin::HookedSFileLoadFileEx(
    HANDLE hMpq,
    LPCSTR lpFileName,
    LPVOID* lplpFileData,
    LPDWORD lpdwFileSize,
    DWORD dwFlags1,
    DWORD dwFlags2,
    LPVOID lpOverlapped)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSFileLoadFileEx)
        result = s_OriginalSFileLoadFileEx(hMpq, lpFileName, lplpFileData, lpdwFileSize, dwFlags1, dwFlags2, lpOverlapped);

    // Log the file access (no HSFILE is returned by this API, so archive lookup is skipped)
    if (result)
    {
        std::ostringstream nonPtr;
        nonPtr << "dwFlags1=0x" << std::hex << dwFlags1 << ", dwFlags2=0x" << dwFlags2;

        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("hMpq", hMpq));
        ptrParts.push_back(PointerOnly("lpFileName", lpFileName));
        ptrParts.push_back(PointerWithPointer("lplpFileData", reinterpret_cast<void* const*>(lplpFileData)));
        ptrParts.push_back(PointerWithDword("lpdwFileSize", lpdwFileSize));
        ptrParts.push_back(PointerOnly("lpOverlapped", lpOverlapped));

        LogFileAccess(lpFileName, nullptr, "SFileLoadFileEx", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// The hook function - this is called instead of the original SBmpLoadImage
BOOL WINAPI CMpqFileListerPlugin::HookedSBmpLoadImage(
    LPCSTR lpFileName,
    LPPALETTEENTRY lpPalette,
    LPBYTE lpBits,
    DWORD dwBitsSize,
    LPDWORD lpdwWidth,
    LPDWORD lpdwHeight,
    LPDWORD lpdwBpp)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSBmpLoadImage)
        result = s_OriginalSBmpLoadImage(lpFileName, lpPalette, lpBits, dwBitsSize, lpdwWidth, lpdwHeight, lpdwBpp);

    // Log the file access (no HSFILE is returned by this API, so archive lookup is skipped)
    if (result)
    {
        std::string nonPtr = "dwBitsSize=" + std::to_string(dwBitsSize);

        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("lpFileName", lpFileName));
        ptrParts.push_back(PointerOnly("lpPalette", lpPalette));
        ptrParts.push_back(PointerOnly("lpBits", lpBits));
        ptrParts.push_back(PointerWithDword("lpdwWidth", lpdwWidth));
        ptrParts.push_back(PointerWithDword("lpdwHeight", lpdwHeight));
        ptrParts.push_back(PointerWithDword("lpdwBpp", lpdwBpp));

        LogFileAccess(lpFileName, nullptr, "SBmpLoadImage", nonPtr, JoinParts(ptrParts));
    }

    return result;
}

// The hook function - this is called instead of the original SBmpAllocLoadImage
BOOL WINAPI CMpqFileListerPlugin::HookedSBmpAllocLoadImage(
    LPCSTR lpFileName,
    LPPALETTEENTRY lpPalette,
    LPBYTE* lplpBits,
    LPDWORD lpdwWidth,
    LPDWORD lpdwHeight,
    LPDWORD lpdwBpp,
    LPDWORD lpdwSize,
    LPVOID lpAllocProc)
{
    // Call the original function first to see if the file was found
    BOOL result = FALSE;
    if (s_OriginalSBmpAllocLoadImage)
        result = s_OriginalSBmpAllocLoadImage(lpFileName, lpPalette, lplpBits, lpdwWidth, lpdwHeight, lpdwBpp, lpdwSize, lpAllocProc);

    // Log the file access (no HSFILE is returned by this API, so archive lookup is skipped).
    // Every parameter here is a pointer, so %p is always empty for this call - use %P.
    // Callers may pass NULL for lpdwBpp/lpdwSize (as gamedata.cpp's alloc_load_bmp does),
    // so PointerWithDword only adds a "(deref=...)" value when the caller asked for one.
    if (result)
    {
        std::vector<std::string> ptrParts;
        ptrParts.push_back(PointerOnly("lpFileName", lpFileName));
        ptrParts.push_back(PointerOnly("lpPalette", lpPalette));
        ptrParts.push_back(PointerWithPointer("lplpBits", reinterpret_cast<void* const*>(lplpBits)));
        ptrParts.push_back(PointerWithDword("lpdwWidth", lpdwWidth));
        ptrParts.push_back(PointerWithDword("lpdwHeight", lpdwHeight));
        ptrParts.push_back(PointerWithDword("lpdwBpp", lpdwBpp));
        ptrParts.push_back(PointerWithDword("lpdwSize", lpdwSize));
        ptrParts.push_back(PointerOnly("lpAllocProc", lpAllocProc));

        LogFileAccess(lpFileName, nullptr, "SBmpAllocLoadImage", "", JoinParts(ptrParts));
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

    // Select ordinals based on target game
    uint32_t sFileOpenFileOrdinal;
    uint32_t sFileOpenFileExOrdinal;
    uint32_t sVidPlayBeginOrdinal;
    uint32_t sFileGetFileArchiveOrdinal;
    uint32_t sFileGetArchiveNameOrdinal;
    uint32_t sFileLoadFileOrdinal;
    uint32_t sFileLoadFileExOrdinal;
    uint32_t sBmpLoadImageOrdinal;
    uint32_t sBmpAllocLoadImageOrdinal;

    if (g_targetGame == TargetGame::DIABLO_1)
    {
        sFileOpenFileOrdinal = SFILEOPENFILE_D1_ORDINAL;
        sFileOpenFileExOrdinal = SFILEOPENFILEEX_D1_ORDINAL;
        sVidPlayBeginOrdinal = SVIDPLAYBEGIN_D1_ORDINAL;
        sFileGetFileArchiveOrdinal = SFILEGETFILEARCHIVE_D1_ORDINAL;
        sFileGetArchiveNameOrdinal = SFILEGETARCHIVENAME_D1_ORDINAL;
        sFileLoadFileOrdinal = SFILELOADFILE_D1_ORDINAL;
        sFileLoadFileExOrdinal = SFILELOADFILEEX_D1_ORDINAL;
        sBmpLoadImageOrdinal = SBMPLOADIMAGE_D1_ORDINAL;
        sBmpAllocLoadImageOrdinal = SBMPALLOCLOADIMAGE_D1_ORDINAL;
    }
    else // TargetGame::LATER
    {
        sFileOpenFileOrdinal = SFILEOPENFILE_ORDINAL;
        sFileOpenFileExOrdinal = SFILEOPENFILEEX_ORDINAL;
        sVidPlayBeginOrdinal = SVIDPLAYBEGIN_ORDINAL;
        sFileGetFileArchiveOrdinal = SFILEGETFILEARCHIVE_ORDINAL;
        sFileGetArchiveNameOrdinal = SFILEGETARCHIVENAME_ORDINAL;
        sFileLoadFileOrdinal = SFILELOADFILE_ORDINAL;
        sFileLoadFileExOrdinal = SFILELOADFILEEX_ORDINAL;
        sBmpLoadImageOrdinal = SBMPLOADIMAGE_ORDINAL;
        sBmpAllocLoadImageOrdinal = SBMPALLOCLOADIMAGE_ORDINAL;
    }

    // Get the original function pointers using ordinals
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    s_OriginalSFileOpenFile = reinterpret_cast<SFileOpenFilePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileOpenFileOrdinal)));

    s_OriginalSFileOpenFileEx = reinterpret_cast<SFileOpenFileExPtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileOpenFileExOrdinal)));

    s_OriginalSVidPlayBegin = reinterpret_cast<SVidPlayBeginPtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sVidPlayBeginOrdinal)));

    // An ordinal of 0 means the function is known not to exist for the selected
    // target (e.g. SFileLoadFile/SFileLoadFileEx/SBmpAllocLoadImage on Diablo I) -
    // skip resolving and warning about those rather than treating them as an
    // unexpected lookup failure.
    if (sFileLoadFileOrdinal)
    {
        s_OriginalSFileLoadFile = reinterpret_cast<SFileLoadFilePtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileLoadFileOrdinal)));
        if (!s_OriginalSFileLoadFile && s_logFile.is_open())
            s_logFile << "WARNING: SFileLoadFile not found in Storm.dll - not hooked\n";
    }

    if (sFileLoadFileExOrdinal)
    {
        s_OriginalSFileLoadFileEx = reinterpret_cast<SFileLoadFileExPtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileLoadFileExOrdinal)));
        if (!s_OriginalSFileLoadFileEx && s_logFile.is_open())
            s_logFile << "WARNING: SFileLoadFileEx not found in Storm.dll - not hooked\n";
    }

    if (sBmpLoadImageOrdinal)
    {
        s_OriginalSBmpLoadImage = reinterpret_cast<SBmpLoadImagePtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sBmpLoadImageOrdinal)));
        if (!s_OriginalSBmpLoadImage && s_logFile.is_open())
            s_logFile << "WARNING: SBmpLoadImage not found in Storm.dll - not hooked\n";
    }

    if (sBmpAllocLoadImageOrdinal)
    {
        s_OriginalSBmpAllocLoadImage = reinterpret_cast<SBmpAllocLoadImagePtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sBmpAllocLoadImageOrdinal)));
        if (!s_OriginalSBmpAllocLoadImage && s_logFile.is_open())
            s_logFile << "WARNING: SBmpAllocLoadImage not found in Storm.dll - not hooked\n";
    }

    if (!s_OriginalSFileOpenFile && !s_OriginalSFileOpenFileEx && !s_OriginalSVidPlayBegin &&
        !s_OriginalSFileLoadFile && !s_OriginalSFileLoadFileEx &&
        !s_OriginalSBmpLoadImage && !s_OriginalSBmpAllocLoadImage)
    {
        if (s_logFile.is_open())
        {
            s_logFile << "ERROR: None of the SFileOpenFile/SFileOpenFileEx/SVidPlayBegin/"
                         "SFileLoadFile/SFileLoadFileEx/SBmpLoadImage/SBmpAllocLoadImage "
                         "functions were found in Storm.dll\n";
        }
        return TRUE;  // Return TRUE to not abort the patch
    }

    // Get SFileGetFileArchive and SFileGetArchiveName for logging which MPQ files come from
    // (optional - e.g. SFileGetArchiveName does not exist on Diablo I's Storm.dll, ordinal 0)
    s_SFileGetFileArchive = reinterpret_cast<SFileGetFileArchivePtr>(
        reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetFileArchiveOrdinal)));
    if (sFileGetArchiveNameOrdinal)
    {
        s_SFileGetArchiveName = reinterpret_cast<SFileGetArchiveNamePtr>(
            reinterpret_cast<void*>(GetProcAddress(m_hStorm, (LPCSTR)sFileGetArchiveNameOrdinal)));
    }

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

    if (s_OriginalSVidPlayBegin)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSVidPlayBegin)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSVidPlayBegin)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    if (s_OriginalSFileLoadFile)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSFileLoadFile)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileLoadFile)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    if (s_OriginalSFileLoadFileEx)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSFileLoadFileEx)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileLoadFileEx)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    if (s_OriginalSBmpLoadImage)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSBmpLoadImage)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSBmpLoadImage)),
            TRUE  // Recursive - patch all loaded modules
        );
    }

    if (s_OriginalSBmpAllocLoadImage)
    {
        PatchImportEntry(
            hHostProcess,
            "Storm.dll",
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(s_OriginalSBmpAllocLoadImage)),
            reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSBmpAllocLoadImage)),
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
