/*
    Hooks.cpp - Everything specific to the Storm.dll functions this plugin hooks:
    their ordinals (for Diablo I and later games), the storage for each one's
    original function pointer, the Hooked* functions that replace them in the
    patched import table, and GetHookEntries() (see Hooks.h), which is the only
    thing InitializePlugin() (MpqFileLister.cpp) knows about any of this.
*/

#include "Hooks.h"
#include "MpqFileLister.h"
#include "LogFormat.h"
#include "Utils.h"
#include <sstream>
#include <vector>

// Storm.dll ordinals
static constexpr uint32_t SFILEOPENFILE_D1_ORDINAL       = 0x4E;    // 78
static constexpr uint32_t SFILEOPENFILEEX_D1_ORDINAL     = 0x4F;    // 79
static constexpr uint32_t SVIDPLAYBEGIN_D1_ORDINAL       = 0x9D;    // 157
static constexpr uint32_t SFILEOPENFILE_ORDINAL          = 0x10B;   // 267
static constexpr uint32_t SFILEOPENFILEEX_ORDINAL        = 0x10C;   // 268
static constexpr uint32_t SVIDPLAYBEGIN_ORDINAL          = 0x1C6;   // 454

static constexpr uint32_t SFILELOADFILE_D1_ORDINAL       = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SFILELOADFILEEX_D1_ORDINAL     = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SBMPLOADIMAGE_D1_ORDINAL       = 0x8;     // 8
static constexpr uint32_t SBMPALLOCLOADIMAGE_D1_ORDINAL  = 0x0;     // not exported by D1's Storm.dll
static constexpr uint32_t SFILELOADFILE_ORDINAL          = 0x117;   // 279
static constexpr uint32_t SFILELOADFILEEX_ORDINAL        = 0x119;   // 281
static constexpr uint32_t SBMPLOADIMAGE_ORDINAL          = 0x143;   // 323
static constexpr uint32_t SBMPALLOCLOADIMAGE_ORDINAL     = 0x145;   // 325

/* Storm function signatures */
typedef BOOL (WINAPI *SFileOpenFilePtr)(LPCSTR lpFileName, HANDLE* hFile);
typedef BOOL (WINAPI *SFileOpenFileExPtr)(HANDLE hMpq, const char* szFileName, DWORD dwSearchScope, HANDLE* phFile);
typedef BOOL (WINAPI *SVidPlayBeginPtr)(char *filename, int a2, int* a3, int* a4, int* a5, int flags, HANDLE* video);
typedef BOOL (WINAPI *SFileLoadFilePtr)(LPCSTR lpFileName, LPVOID* lplpFileData, LPDWORD lpdwFileSize, DWORD dwFlags1, DWORD dwFlags2);
typedef BOOL (WINAPI *SFileLoadFileExPtr)(HANDLE hMpq, LPCSTR lpFileName, LPVOID* lplpFileData, LPDWORD lpdwFileSize, DWORD dwFlags1, DWORD dwFlags2, LPVOID lpOverlapped);
typedef BOOL (WINAPI *SBmpLoadImagePtr)(LPCSTR lpFileName, LPPALETTEENTRY lpPalette, LPBYTE lpBits, DWORD dwBitsSize, LPDWORD lpdwWidth, LPDWORD lpdwHeight, LPDWORD lpdwBpp);
// SBmpAllocLoadImage's alloc-callback parameter is passed through untouched, so
// it's typed as an opaque pointer here rather than a fully-specified callback signature.
typedef BOOL (WINAPI *SBmpAllocLoadImagePtr)(LPCSTR lpFileName, LPPALETTEENTRY lpPalette, LPBYTE* lplpBits, LPDWORD lpdwWidth, LPDWORD lpdwHeight, LPDWORD lpdwBpp, LPDWORD lpdwSize, LPVOID lpAllocProc);

// Original function pointers - set by InitializePlugin() through the originalPtrSlot
// of each GetHookEntries() row, read by the Hooked* functions below.
static SFileOpenFilePtr s_OriginalSFileOpenFile = nullptr;
static SFileOpenFileExPtr s_OriginalSFileOpenFileEx = nullptr;
static SVidPlayBeginPtr s_OriginalSVidPlayBegin = nullptr;
static SFileLoadFilePtr s_OriginalSFileLoadFile = nullptr;
static SFileLoadFileExPtr s_OriginalSFileLoadFileEx = nullptr;
static SBmpLoadImagePtr s_OriginalSBmpLoadImage = nullptr;
static SBmpAllocLoadImagePtr s_OriginalSBmpAllocLoadImage = nullptr;

// Called instead of the original SFileOpenFile
static BOOL WINAPI HookedSFileOpenFile(
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
        CMpqFileListerPlugin::LogFileAccess(lpFileName, *hFile, "SFileOpenFile", "", JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SFileOpenFileEx
static BOOL WINAPI HookedSFileOpenFileEx(
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

        CMpqFileListerPlugin::LogFileAccess(szFileName, *phFile, "SFileOpenFileEx", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SVidPlayBegin
static BOOL WINAPI HookedSVidPlayBegin(
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

        CMpqFileListerPlugin::LogFileAccess(filename, nullptr, "SVidPlayBegin", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SFileLoadFile
static BOOL WINAPI HookedSFileLoadFile(
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

        CMpqFileListerPlugin::LogFileAccess(lpFileName, nullptr, "SFileLoadFile", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SFileLoadFileEx
static BOOL WINAPI HookedSFileLoadFileEx(
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

        CMpqFileListerPlugin::LogFileAccess(lpFileName, nullptr, "SFileLoadFileEx", nonPtr.str(), JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SBmpLoadImage
static BOOL WINAPI HookedSBmpLoadImage(
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

        CMpqFileListerPlugin::LogFileAccess(lpFileName, nullptr, "SBmpLoadImage", nonPtr, JoinParts(ptrParts));
    }

    return result;
}

// Called instead of the original SBmpAllocLoadImage
static BOOL WINAPI HookedSBmpAllocLoadImage(
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

        CMpqFileListerPlugin::LogFileAccess(lpFileName, nullptr, "SBmpAllocLoadImage", "", JoinParts(ptrParts));
    }

    return result;
}

const std::vector<HookEntry>& GetHookEntries()
{
    // Use reinterpret_cast via void* to avoid -Wcast-function-type warning
    static const std::vector<HookEntry> hooks = {
        { "SFileOpenFile", SFILEOPENFILE_D1_ORDINAL, SFILEOPENFILE_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSFileOpenFile),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileOpenFile)) },
        { "SFileOpenFileEx", SFILEOPENFILEEX_D1_ORDINAL, SFILEOPENFILEEX_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSFileOpenFileEx),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileOpenFileEx)) },
        { "SVidPlayBegin", SVIDPLAYBEGIN_D1_ORDINAL, SVIDPLAYBEGIN_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSVidPlayBegin),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSVidPlayBegin)) },
        { "SFileLoadFile", SFILELOADFILE_D1_ORDINAL, SFILELOADFILE_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSFileLoadFile),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileLoadFile)) },
        { "SFileLoadFileEx", SFILELOADFILEEX_D1_ORDINAL, SFILELOADFILEEX_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSFileLoadFileEx),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSFileLoadFileEx)) },
        { "SBmpLoadImage", SBMPLOADIMAGE_D1_ORDINAL, SBMPLOADIMAGE_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSBmpLoadImage),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSBmpLoadImage)) },
        { "SBmpAllocLoadImage", SBMPALLOCLOADIMAGE_D1_ORDINAL, SBMPALLOCLOADIMAGE_ORDINAL,
          reinterpret_cast<void**>(&s_OriginalSBmpAllocLoadImage),
          reinterpret_cast<FARPROC>(reinterpret_cast<void*>(HookedSBmpAllocLoadImage)) },
    };
    return hooks;
}
