/*
    Hooks.cpp - The CMpqFileListerPlugin::Hooked* functions that replace Storm.dll's
    real SFileOpenFile, SFileOpenFileEx, SVidPlayBegin, SFileLoadFile, SFileLoadFileEx,
    SBmpLoadImage and SBmpAllocLoadImage in the patched import table. Declared in
    MpqFileLister.h; installed by InitializePlugin() in MpqFileLister.cpp.
*/

#include "MpqFileLister.h"
#include "LogFormat.h"
#include "Utils.h"
#include <sstream>
#include <vector>

// Called instead of the original SFileOpenFile
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

// Called instead of the original SFileOpenFileEx
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

// Called instead of the original SVidPlayBegin
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

// Called instead of the original SFileLoadFile
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

// Called instead of the original SFileLoadFileEx
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

// Called instead of the original SBmpLoadImage
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

// Called instead of the original SBmpAllocLoadImage
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
