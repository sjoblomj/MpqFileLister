/*
    MPQDraftPlugin.h - MPQDraft Plugin Interface
    
    This is a simplified version of the MPQDraft plugin interface header
    for use by standalone plugins.
*/

#ifndef MPQDRAFTPLUGIN_H
#define MPQDRAFTPLUGIN_H

#include <windows.h>

// The maximum length of a plugin module's filename. INCLUDES final NULL.
#define MPQDRAFT_MAX_PATH 264

// The maximum length of a plugin's name. INCLUDES final NULL.
#define MPQDRAFT_MAX_PLUGIN_NAME 64

#pragma pack(push, 1)
struct MPQDRAFTPLUGINMODULE
{
    DWORD dwComponentID;
    DWORD dwModuleID;
    BOOL bExecute;
    char szModuleFileName[MPQDRAFT_MAX_PATH];
};
#pragma pack(pop)

// Server interface - allows plugin to locate its modules
struct IMPQDraftServer
{
    virtual BOOL WINAPI GetPluginModule(
        DWORD dwPluginID,
        DWORD dwModuleID,
        LPSTR lpszFileName) = 0;
};

// Plugin interface - must be implemented by all plugins
struct IMPQDraftPlugin
{
    virtual BOOL WINAPI Identify(LPDWORD lpdwPluginID) = 0;
    virtual BOOL WINAPI GetPluginName(LPSTR lpszPluginName, DWORD nNameBufferLength) = 0;
    virtual BOOL WINAPI CanPatchExecutable(LPCSTR lpszEXEFileName) = 0;
    virtual BOOL WINAPI Configure(HWND hParentWnd) = 0;
    virtual BOOL WINAPI ReadyForPatch() = 0;
    virtual BOOL WINAPI GetModules(MPQDRAFTPLUGINMODULE *lpPluginModules, LPDWORD lpnNumModules) = 0;
    virtual BOOL WINAPI InitializePlugin(IMPQDraftServer *lpMPQDraftServer) = 0;
    virtual BOOL WINAPI TerminatePlugin() = 0;
};

// Export function that plugins must provide
extern "C" BOOL WINAPI GetMPQDraftPlugin(IMPQDraftPlugin **lppMPQDraftPlugin);

#endif // MPQDRAFTPLUGIN_H
