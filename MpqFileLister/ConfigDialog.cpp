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
    "  By Ojan (Johan Sj\xf6" "blom)\r\n"
    "\r\n"
    "This plugin intercepts all file access attempts made by the game "
    "through Storm.dll's SFileOpenFileEx function. Every filename that "
    "the game tries to open from MPQ archives is logged to a text file."
    "\r\n\r\n"
    "This is useful for modding, debugging, or understanding which "
    "game assets are loaded during gameplay."
    "\r\n\r\n";

static const char* CHECKBOX_TEXT = "Log unique filenames only (no duplicates) ";
static const char* PATH_LABEL_TEXT = "Log file name (filename only to log to the game's directory):";

// Layout constants
static constexpr int MARGIN = 25;
static constexpr int SPACING = 25;
static constexpr int BUTTON_HEIGHT = 60;
static constexpr int EDIT_HEIGHT = 50;
static constexpr int MIN_DLG_WIDTH = 300;

// Calculated dialog dimensions (set during control creation)
static int g_dlgWidth = 0;
static int g_dlgHeight = 0;

// Helper to measure text size
static SIZE MeasureText(HDC hdc, const char* text, int maxWidth = 0)
{
    SIZE size = {0, 0};
    if (maxWidth > 0)
    {
        RECT rc = {0, 0, maxWidth, 0};
        DrawTextA(hdc, text, -1, &rc, DT_CALCRECT | DT_WORDBREAK);
        size.cx = rc.right;
        size.cy = rc.bottom;
    }
    else
    {
        GetTextExtentPoint32A(hdc, text, static_cast<int>(strlen(text)), &size);
    }
    return size;
}

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

static void CalculateDialogLayout(HDC hdc)
{
    // Measure all text elements to determine required sizes
    SIZE descSize = MeasureText(hdc, DESCRIPTION_TEXT, 400);
    SIZE checkboxSize = MeasureText(hdc, CHECKBOX_TEXT);
    SIZE labelSize = MeasureText(hdc, PATH_LABEL_TEXT);
    SIZE browseSize = MeasureText(hdc, "Browse...");
    SIZE okSize = MeasureText(hdc, "OK");
    SIZE cancelSize = MeasureText(hdc, "Cancel");

    // Add padding for checkbox (checkbox square + spacing)
    checkboxSize.cx += 20;
    checkboxSize.cy += 4;

    // Add padding for buttons
    browseSize.cx += 16;
    browseSize.cy = BUTTON_HEIGHT;
    okSize.cx += 24;
    okSize.cy = BUTTON_HEIGHT;
    cancelSize.cx += 24;
    cancelSize.cy = BUTTON_HEIGHT;

    // Calculate required width (widest element + margins)
    int contentWidth = descSize.cx;
    if (checkboxSize.cx > contentWidth) contentWidth = checkboxSize.cx;
    if (labelSize.cx > contentWidth) contentWidth = labelSize.cx;

    g_dlgWidth = contentWidth + (MARGIN * 2);
    if (g_dlgWidth < MIN_DLG_WIDTH) g_dlgWidth = MIN_DLG_WIDTH;

    // Calculate required height
    int y = MARGIN;
    y += descSize.cy + SPACING;      // Description
    y += checkboxSize.cy + SPACING;  // Checkbox
    y += labelSize.cy + SPACING;     // Path label
    y += EDIT_HEIGHT + SPACING;      // Edit + Browse row
    y += SPACING;                    // Extra spacing before buttons
    y += BUTTON_HEIGHT + MARGIN;     // OK/Cancel buttons

    g_dlgHeight = y;
}

static HWND CreateDialogControls(HWND hDlg, HMODULE hModule)
{
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    // Get DC and select font for measurements
    HDC hdc = GetDC(hDlg);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Calculate layout
    CalculateDialogLayout(hdc);

    // Measure individual elements for positioning
    SIZE descSize = MeasureText(hdc, DESCRIPTION_TEXT, g_dlgWidth - (MARGIN * 2));
    SIZE checkboxSize = MeasureText(hdc, CHECKBOX_TEXT);
    checkboxSize.cx += 20;
    checkboxSize.cy += 4;
    SIZE labelSize = MeasureText(hdc, PATH_LABEL_TEXT);
    SIZE browseSize = MeasureText(hdc, "Browse...");
    browseSize.cx += 16;
    SIZE okSize = MeasureText(hdc, "OK");
    okSize.cx += 24;
    SIZE cancelSize = MeasureText(hdc, "Cancel");
    cancelSize.cx += 24;

    SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);

    int y = MARGIN;
    int contentWidth = g_dlgWidth - (MARGIN * 2);

    // Description (multiline static text)
    HWND hDesc = CreateWindowExA(
        0, "STATIC", DESCRIPTION_TEXT,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, contentWidth, descSize.cy,
        hDlg, (HMENU)IDC_DESCRIPTION, hModule, nullptr
    );
    SendMessage(hDesc, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += descSize.cy + SPACING;

    // Checkbox
    HWND hCheck = CreateWindowExA(
        0, "BUTTON", CHECKBOX_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        MARGIN, y, checkboxSize.cx, checkboxSize.cy,
        hDlg, (HMENU)IDC_UNIQUE_CHECKBOX, hModule, nullptr
    );
    SendMessage(hCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    CheckDlgButton(hDlg, IDC_UNIQUE_CHECKBOX, g_logUniqueOnly ? BST_CHECKED : BST_UNCHECKED);
    y += checkboxSize.cy + SPACING;

    // Path label
    HWND hLabel = CreateWindowExA(
        0, "STATIC", PATH_LABEL_TEXT,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        MARGIN, y, labelSize.cx, labelSize.cy,
        hDlg, (HMENU)IDC_PATH_LABEL, hModule, nullptr
    );
    SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += labelSize.cy + SPACING;

    // Path edit and Browse button on same row
    int browseWidth = browseSize.cx;
    int editWidth = contentWidth - browseWidth - SPACING;

    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", g_logFileName.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        MARGIN, y, editWidth, EDIT_HEIGHT,
        hDlg, (HMENU)IDC_PATH_EDIT, hModule, nullptr
    );
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hBrowse = CreateWindowExA(
        0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        MARGIN + editWidth + SPACING, y, browseWidth, EDIT_HEIGHT,
        hDlg, (HMENU)IDC_BROWSE_BUTTON, hModule, nullptr
    );
    SendMessage(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);
    y += EDIT_HEIGHT + SPACING + SPACING;

    // OK and Cancel buttons (right-aligned)
    int buttonY = y;
    int cancelX = g_dlgWidth - MARGIN - cancelSize.cx;
    int okX = cancelX - SPACING - okSize.cx;

    HWND hOK = CreateWindowExA(
        0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        okX, buttonY, okSize.cx, BUTTON_HEIGHT,
        hDlg, (HMENU)IDC_OK_BUTTON, hModule, nullptr
    );
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hCancel = CreateWindowExA(
        0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cancelX, buttonY, cancelSize.cx, BUTTON_HEIGHT,
        hDlg, (HMENU)IDC_CANCEL_BUTTON, hModule, nullptr
    );
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Resize the dialog to fit the content
    // Account for window chrome (title bar, borders)
    RECT rcClient = {0, 0, g_dlgWidth, g_dlgHeight};
    AdjustWindowRectEx(&rcClient, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int windowWidth = rcClient.right - rcClient.left;
    int windowHeight = rcClient.bottom - rcClient.top;
    SetWindowPos(hDlg, nullptr, 0, 0, windowWidth, windowHeight, SWP_NOMOVE | SWP_NOZORDER);

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

// Flag to signal dialog should close
static bool s_dialogRunning = false;

// Window procedure for the dialog
static LRESULT CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_COMMAND:
        {
            WORD cmd = LOWORD(wParam);
            if (cmd == IDC_OK_BUTTON)
            {
                HandleOkButton(hDlg);
                s_dialogRunning = false;
                return 0;
            }
            else if (cmd == IDC_CANCEL_BUTTON)
            {
                s_dialogRunning = false;
                return 0;
            }
            else if (cmd == IDC_BROWSE_BUTTON)
            {
                HandleBrowseButton(hDlg);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            s_dialogRunning = false;
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE)
            {
                s_dialogRunning = false;
                return 0;
            }
            break;
    }
    return DefWindowProcA(hDlg, uMsg, wParam, lParam);
}

static void RunDialogMessageLoop(HWND hDlg)
{
    MSG msg;
    s_dialogRunning = true;

    while (s_dialogRunning && GetMessage(&msg, nullptr, 0, 0))
    {
        if (!IsWindow(hDlg))
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static const char* DIALOG_CLASS_NAME = "MpqFileListerConfigDialog";

static void RegisterDialogClass(HMODULE hModule)
{
    static bool registered = false;
    if (registered)
        return;

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = hModule;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = DIALOG_CLASS_NAME;

    RegisterClassExA(&wc);
    registered = true;
}

void ShowConfigDialog(HWND hParentWnd, HMODULE hModule)
{
    // Register our custom window class
    RegisterDialogClass(hModule);

    // Create dialog with minimal initial size - it will be resized after controls are created
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DIALOG_CLASS_NAME,
        "MPQFileLister - Configuration",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,  // Not visible initially
        CW_USEDEFAULT, CW_USEDEFAULT,
        100, 100,  // Temporary size
        hParentWnd,
        nullptr,
        hModule,
        nullptr
    );

    if (!hDlg)
        return;

    // Create controls and resize dialog to fit
    CreateDialogControls(hDlg, hModule);
    CenterDialog(hDlg, hParentWnd);

    // Now show the dialog
    ShowWindow(hDlg, SW_SHOW);

    // Disable the parent window for modal behavior
    if (hParentWnd)
        EnableWindow(hParentWnd, FALSE);

    RunDialogMessageLoop(hDlg);

    // Re-enable the parent window
    if (hParentWnd)
        EnableWindow(hParentWnd, TRUE);

    if (IsWindow(hDlg))
        DestroyWindow(hDlg);
}
