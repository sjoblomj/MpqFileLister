/*
    The contents of this file are subject to the Common Development and Distribution
    License Version 1.0 (the "License"); you may not use this file except in compliance
    with the License. You may obtain a copy of the License at
    http://www.sun.com/cddl/cddl.html.

    Software distributed under the License is distributed on an "AS IS" basis,
    WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for the
    specific language governing rights and limitations under the License.

    The Initial Developer of the Original Code is Justin Olbrantz.
    The Original Code Copyright (C) 2007 Justin Olbrantz. All Rights Reserved.
*/

#include "QHookAPI.h"
#include <winnt.h>
#include <cassert>

// Get a given data directory entry from an HMODULE
static BOOL FindDataDirectoryEntry(
    IN HMODULE hModule,
    IN DWORD iEntryIndex,
    OUT PVOID *lplpDirEntry,
    OUT OPTIONAL LPDWORD lpnEntrySize)
{
    assert(hModule);
    assert(lplpDirEntry);

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;

    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE
        || pDosHeader->e_lfanew == 0)
        return FALSE;

    PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)
        ((LPBYTE)pDosHeader + pDosHeader->e_lfanew);

    if (pNTHeader->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
        return FALSE;

    if (iEntryIndex >= pNTHeader->OptionalHeader.NumberOfRvaAndSizes)
        return FALSE;

    IMAGE_DATA_DIRECTORY &dirEntry =
        pNTHeader->OptionalHeader.DataDirectory[iEntryIndex];
    if (!dirEntry.Size || !dirEntry.VirtualAddress)
        return FALSE;

    *lplpDirEntry = (PVOID)((LPBYTE)hModule + dirEntry.VirtualAddress);
    if (lpnEntrySize)
        *lpnEntrySize = dirEntry.Size;

    return TRUE;
}

// Core patching function
static DWORD PatchImportCore(
    IN HMODULE hHostProgram,
    IN HMODULE hExportModule,
    IN FARPROC pfnOldFunction,
    IN FARPROC pfnNewFunction,
    IN BOOL bRecurse,
    IN OUT OPTIONAL ModuleSet *pModules)
{
    assert(hHostProgram);
    assert(hExportModule);
    assert(pfnOldFunction);
    assert(pfnNewFunction);

    if (pModules)
    {
        ModuleSet::iterator itModule = pModules->find(hHostProgram);
        if (itModule != pModules->end())
            return 0;

        try
        { pModules->insert(hHostProgram); }
        catch (...)
        { return (DWORD)-1; }
    }

    PIMAGE_IMPORT_DESCRIPTOR pImportDesc;
    DWORD nImportSize;
    if (!FindDataDirectoryEntry(hHostProgram, IMAGE_DIRECTORY_ENTRY_IMPORT,
        (PVOID *)&pImportDesc, &nImportSize))
        return 0;

    DWORD nPatchCount = 0;

    for (DWORD iDescIndex = 0; pImportDesc[iDescIndex].Name; iDescIndex++)
    {
        LPCSTR lpszImportName = (LPCSTR)
            ((LPBYTE)hHostProgram + pImportDesc[iDescIndex].Name);
        HMODULE hChildModule = GetModuleHandleA(lpszImportName);

        if (hChildModule != NULL && hChildModule != hExportModule)
        {
            if (bRecurse)
            {
                DWORD nPatchesMade = PatchImportCore(hChildModule, hExportModule,
                    pfnOldFunction, pfnNewFunction, bRecurse, pModules);
                if (nPatchesMade == (DWORD)-1)
                    return (DWORD)-1;

                nPatchCount += nPatchesMade;
            }
            continue;
        }

        FARPROC *pThunk = (FARPROC *)
            ((LPBYTE)hHostProgram + pImportDesc[iDescIndex].FirstThunk);

        for (DWORD iThunkIndex = 0; pThunk[iThunkIndex]; iThunkIndex++)
        {
            if (pThunk[iThunkIndex] != pfnOldFunction)
                continue;

            DWORD dwOldProtection, dwUnused;

            if (!VirtualProtect(&pThunk[iThunkIndex], sizeof(pThunk[iThunkIndex]),
                PAGE_READWRITE, &dwOldProtection))
                return (DWORD)-1;

            pThunk[iThunkIndex] = pfnNewFunction;

            VirtualProtect(&pThunk[iThunkIndex], sizeof(pThunk[iThunkIndex]),
                dwOldProtection, &dwUnused);

            nPatchCount++;
        }
    }

    return nPatchCount;
}

// Public wrapper function
DWORD WINAPI PatchImportEntry(
    IN HMODULE hHostProgram,
    IN LPCSTR lpszModuleName,
    IN FARPROC pfnOldFunction,
    IN FARPROC pfnNewFunction,
    IN OUT ModuleSet *lpModuleSet,
    IN BOOL bRecurse)
{
    assert(lpszModuleName);
    assert(lpModuleSet);

    if (!hHostProgram || !lpszModuleName || !pfnOldFunction ||
        !pfnNewFunction || !lpModuleSet)
        return (DWORD)-1;

#ifdef _MSC_VER
    __try
    {
#endif
        HMODULE hExportModule = GetModuleHandleA(lpszModuleName);
        if (!hExportModule)
            return (DWORD)-1;

        return PatchImportCore(hHostProgram, hExportModule,
            pfnOldFunction, pfnNewFunction, bRecurse, lpModuleSet);
#ifdef _MSC_VER
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return (DWORD)-1;
    }
#endif
}
