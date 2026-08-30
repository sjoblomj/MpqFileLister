/*
    Hooks.h - The generic interface InitializePlugin() (MpqFileLister.cpp) uses to
    resolve and patch every Storm.dll function this plugin hooks, without needing
    to know what any of them are. All of that knowledge - which functions exist,
    their ordinals per target game, and their replacement functions - lives
    entirely in Hooks.cpp.
*/

#ifndef HOOKS_H
#define HOOKS_H

#include <windows.h>
#include <cstdint>
#include <vector>

// One row per hooked Storm.dll function: both ordinals (Diablo I and later games -
// kept together so the two targets can't silently drift out of sync with each
// other), where to store the resolved original function pointer, and which
// function replaces it in the import table. An ordinal of 0 means the function is
// known not to exist for that target.
struct HookEntry
{
    const char* name;
    uint32_t d1Ordinal;
    uint32_t laterOrdinal;
    void** originalPtrSlot;  // where to write the resolved original function pointer
    FARPROC hookFn;          // the function that replaces it in the import table
};

// Returns the full list of Storm.dll functions this plugin can hook. The caller
// only needs to resolve each entry's ordinal, store the result through
// originalPtrSlot, and patch it in via hookFn - it never needs to know what's
// actually being hooked.
const std::vector<HookEntry>& GetHookEntries();

#endif // HOOKS_H
