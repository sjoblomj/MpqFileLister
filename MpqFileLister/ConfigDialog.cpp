/*
    ConfigDialog.cpp - Configuration dialog for MpqFileLister plugin
*/

#include "ConfigDialog.h"
#include "Config.h"
#include "MpqFileLister.h"
#include <commdlg.h>
#include <string>

// Dialog control IDs
static constexpr int IDC_DESCRIPTION = 101;
static constexpr int IDC_UNIQUE_CHECKBOX = 102;
static constexpr int IDC_PATH_LABEL = 103;
static constexpr int IDC_PATH_EDIT = 104;
static constexpr int IDC_BROWSE_BUTTON = 105;
static constexpr int IDC_OK_BUTTON = IDOK;
static constexpr int IDC_CANCEL_BUTTON = IDCANCEL;

static const char* DESCRIPTION_TEXT =
    PLUGIN_NAME "\r\n"
    "  By Ojan (Johan Sjöblom)\r\n"
    "\r\n"
    "This plugin intercepts all file access attempts made by the game\r\n"
    "through Storm.dll's SFileOpenFileEx function. Every filename that\r\n"
    "the game tries to open from MPQ archives is logged to a text file.\r\n"
    "\r\n"
    "This is useful for modding, debugging, or understanding which\r\n"
    "game assets are loaded during gameplay.";

// Dialog dimensions
static constexpr int DLG_WIDTH = 320;
static constexpr int DLG_HEIGHT = 230;

static void HandleBrowseButton(HWND hDlg)
{
    char filename[MAX_PATH] = "";
    GetDlgItemTextA(hDlg, IDC_PATH_EDIT, filename, MAX_PATH);

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select Log File Location";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "txt";

    if (GetSaveFileNameA(&ofn))
    {
        SetDlgItemTextA(hDlg, IDC_PATH_EDIT, filename);
    }
}

static void HandleOkButton(HWND hDlg)
{
    // Save checkbox state
    g_logUniqueOnly = (IsDlgButtonChecked(hDlg, IDC_UNIQUE_CHECKBOX) == BST_CHECKED);

    // Save path
    char path[MAX_PATH];
    GetDlgItemTextA(hDlg, IDC_PATH_EDIT, path, MAX_PATH);
    g_logFileName = path;

    // Save to the config file
    SaveConfig();
}

static HWND CreateDialogControls(HWND hDlg, HMODULE hModule)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Description (multiline static text)
    HWND hDesc = CreateWindowExA(
        0, "STATIC", DESCRIPTION_TEXT,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 10, DLG_WIDTH - 30, 90,
        hDlg, (HMENU)IDC_DESCRIPTION, hModule, nullptr
    );
    SendMessage(hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Checkbox
    HWND hCheck = CreateWindowExA(
        0, "BUTTON", "Log unique filenames only (no duplicates)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, 105, DLG_WIDTH - 30, 20,
        hDlg, (HMENU)IDC_UNIQUE_CHECKBOX, hModule, nullptr
    );
    SendMessage(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    CheckDlgButton(hDlg, IDC_UNIQUE_CHECKBOX, g_logUniqueOnly ? BST_CHECKED : BST_UNCHECKED);

    // Path label
    HWND hLabel = CreateWindowExA(
        0, "STATIC", "Log file name (filename only to log to the game's directory):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 135, DLG_WIDTH - 30, 20,
        hDlg, (HMENU)IDC_PATH_LABEL, hModule, nullptr
    );
    SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Path edit
    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", g_logFileName.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 160, DLG_WIDTH - 90, 22,
        hDlg, (HMENU)IDC_PATH_EDIT, hModule, nullptr
    );
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Browse button
    HWND hBrowse = CreateWindowExA(
        0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DLG_WIDTH - 75, 159, 60, 24,
        hDlg, (HMENU)IDC_BROWSE_BUTTON, hModule, nullptr
    );
    SendMessage(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

    // OK button
    HWND hOK = CreateWindowExA(
        0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        DLG_WIDTH - 160, DLG_HEIGHT - 60, 70, 25,
        hDlg, (HMENU)IDC_OK_BUTTON, hModule, nullptr
    );
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Cancel button
    HWND hCancel = CreateWindowExA(
        0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DLG_WIDTH - 80, DLG_HEIGHT - 60, 70, 25,
        hDlg, (HMENU)IDC_CANCEL_BUTTON, hModule, nullptr
    );
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    return hDlg;
}

static void CenterDialog(HWND hDlg, HWND hParentWnd)
{
    RECT rcParent, rcDlg;
    if (hParentWnd && GetWindowRect(hParentWnd, &rcParent))
    {
        GetWindowRect(hDlg, &rcDlg);
        int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcParent.top + ((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

static void RunDialogMessageLoop(HWND hDlg, HWND /*hParentWnd*/)
{
    MSG msg;
    bool running = true;

    while (running && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))
        {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
            {
                running = false;
                continue;
            }

            if (msg.message == WM_COMMAND)
            {
                WORD cmd = LOWORD(msg.wParam);
                if (cmd == IDC_OK_BUTTON)
                {
                    HandleOkButton(hDlg);
                    running = false;
                    continue;
                }
                else if (cmd == IDC_CANCEL_BUTTON)
                {
                    running = false;
                    continue;
                }
                else if (cmd == IDC_BROWSE_BUTTON)
                {
                    HandleBrowseButton(hDlg);
                    continue;
                }
            }
        }

        if (!IsWindow(hDlg))
        {
            running = false;
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void ShowConfigDialog(HWND hParentWnd, HMODULE hModule)
{
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        WC_DIALOG,
        "MPQFileLister - Configuration",
        WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DLG_WIDTH, DLG_HEIGHT,
        hParentWnd,
        nullptr,
        hModule,
        nullptr
    );

    if (!hDlg)
        return;

    CreateDialogControls(hDlg, hModule);
    CenterDialog(hDlg, hParentWnd);

    // Disable the parent window for modal behavior
    if (hParentWnd)
        EnableWindow(hParentWnd, FALSE);

    RunDialogMessageLoop(hDlg, hParentWnd);

    // Re-enable the parent window
    if (hParentWnd)
        EnableWindow(hParentWnd, TRUE);

    if (IsWindow(hDlg))
        DestroyWindow(hDlg);
}
