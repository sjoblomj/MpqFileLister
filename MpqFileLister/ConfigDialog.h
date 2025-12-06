/*
    ConfigDialog.h - Configuration dialog for MpqFileLister plugin
*/

#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <windows.h>

// Show the configuration dialog
// hParentWnd - Parent window handle (can be NULL)
// hModule - Module handle of the plugin DLL
void ShowConfigDialog(HWND hParentWnd, HMODULE hModule);

#endif // CONFIGDIALOG_H

