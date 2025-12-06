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

#ifndef QHOOKAPI_H
#define QHOOKAPI_H

#include <windows.h>
#include <set>

// We're going to keep track of all the modules we've patched so that we don't
// do the same one and its descendants twice. This will significantly speed up
// recursive patching operations.
typedef std::set<HMODULE> ModuleSet;

/*
    * PatchImportEntry *

    Patches a module's import table to redirect one imported function to a
    version you supply. PatchImportEntry returns the number of patches made,
    or -1 on failure.

    The import table is a list of functions a module will call in other
    modules, such as the Windows system DLLs. Calls made normally in the source
    will go through the import table to locate the function in a different
    module; this import table will be set up by Windows when the module in
    question is loaded. GetProcAddress, however, does not use the import table.
    Rather, it goes directly to the module the desired function is in, and
    looks at its export table, which lists functions that module makes
    available for other modules to import. For this reason, this function will
    not alter what is returned by GetProcAddress for a function, even if the
    function had been patched by PatchImportEntry.
*/
DWORD WINAPI PatchImportEntry(
    // The (root) module whose import table is to be patched.
    IN HMODULE hHostProgram,
    // The name of the module which is exporting the function
    IN LPCSTR lpszModuleName,
    // The function which is to be replaced in import tables
    IN FARPROC pfnOldFunction,
    // The function which is to replace the old function in import tables
    IN FARPROC pfnNewFunction,
    // A set of modules that have already been patched
    IN OUT ModuleSet *lpModuleSet,
    // Whether the function should execute recursively
    IN OPTIONAL BOOL bRecurse = FALSE
);

// This version allocates the ModuleSet internally
inline DWORD WINAPI PatchImportEntry(
    IN HMODULE hHostProgram,
    IN LPCSTR lpszModuleName,
    IN FARPROC pfnOldFunction,
    IN FARPROC pfnNewFunction,
    IN OPTIONAL BOOL bRecurse = FALSE
)
{
    try
    {
        ModuleSet modules;
        return PatchImportEntry(hHostProgram, lpszModuleName,
            pfnOldFunction, pfnNewFunction, &modules, bRecurse);
    }
    catch (...)
    { return (DWORD)-1; }
}

#endif // QHOOKAPI_H
