/*
    StormHooks.h - The generic interface InitializePlugin()/LogFileAccess()
    (MpqFileLister.cpp) use to resolve and patch every Storm.dll function this
    plugin touches, without needing to know what any of them are. All of that
    knowledge - which functions exist, their ordinals per target game, and (for
    the ones that are actually hooked) their replacement functions - lives
    entirely in StormHooks.cpp.
*/

#ifndef STORMHOOKS_H
#define STORMHOOKS_H

#include <windows.h>
#include <cstdint>
#include <string>
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

// Resolves SFileGetFileArchive/SFileGetArchiveName against hStorm, for looking up
// which MPQ archive a file came from. Not hooks - nothing gets patched, so these
// aren't part of GetHookEntries() - just resolved for LookupArchiveName() to use.
// Safe to call even when the lookup isn't available for this target (e.g.
// SFileGetArchiveName on Diablo I); LookupArchiveName() then just always returns "".
void ResolveArchiveNameLookup(HMODULE hStorm, bool isDiabloOne);

// Returns the name of the MPQ archive that fileHandle's underlying file was opened
// from (just the archive's filename, not its full path), or "" if unavailable -
// either the lookup isn't available for this target, fileHandle is null (not every
// hooked call yields a real HSFILE), or the lookup itself failed for this handle.
std::string LookupArchiveName(HANDLE fileHandle);

#endif // STORMHOOKS_H
