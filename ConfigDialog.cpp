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
static constexpr int IDC_LOG_FORMAT_GROUPBOX = 103;
static constexpr int IDC_TIMESTAMP_INFO_LABEL = 104;
static constexpr int IDC_RADIO_TIMESTAMP_ARCHIVE_FILENAME = 105;
static constexpr int IDC_RADIO_ARCHIVE_FILENAME = 106;
static constexpr int IDC_RADIO_TIMESTAMP_FILENAME = 107;
static constexpr int IDC_RADIO_FILENAME_ONLY = 108;
static constexpr int IDC_LOG_FILENAME_GROUPBOX = 109;
static constexpr int IDC_PATH_LABEL = 110;
static constexpr int IDC_PATH_EDIT = 111;
static constexpr int IDC_BROWSE_BUTTON = 112;
static constexpr int IDC_TARGET_GAME_GROUPBOX = 113;
static constexpr int IDC_RADIO_DIABLO1 = 114;
static constexpr int IDC_RADIO_LATER = 115;
static constexpr int IDC_OK_BUTTON = IDOK;
static constexpr int IDC_CANCEL_BUTTON = IDCANCEL;

static const char* DESCRIPTION_TEXT =
    PLUGIN_NAME "\r\n"
    "  By Ojan (Johan Sj\xf6" "blom)\r\n"
    "\r\n"
    "This plugin intercepts all file access attempts made by the game "
    "through Storm.dll's SFileOpenFile and SFileOpenFileEx functions. "
    "Every filename that the game tries to open from MPQ archives is "
    "logged to a text file."
    "\r\n\r\n"
    "This is useful for modding, debugging, or understanding which "
    "game assets are loaded during gameplay."
    "\r\n\r\n";

static const char* UNIQUE_CHECKBOX_TEXT = "Log unique filenames only (no duplicates)";
static const char* LOG_FORMAT_GROUPBOX_TEXT = "Log format";
static const char* TIMESTAMP_INFO_TEXT = "Timestamp is in milliseconds since epoch";
static const char* RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT = "'<timestamp> <MPQ archive>: <filename>'";
static const char* RADIO_ARCHIVE_FILENAME_TEXT = "'<MPQ archive>: <filename>'";
static const char* RADIO_TIMESTAMP_FILENAME_TEXT = "'<timestamp> <filename>'";
static const char* RADIO_FILENAME_ONLY_TEXT = "'<filename>'";
static const char* LOG_FILENAME_GROUPBOX_TEXT = "Log file name";
static const char* PATH_LABEL_TEXT = "Put filename only (as opposed to full path) to create the file in the game's directory";
static const char* TARGET_GAME_GROUPBOX_TEXT = "Target game";
static const char* RADIO_DIABLO1_TEXT = "Diablo I";
static const char* RADIO_LATER_TEXT = "Later games (StarCraft, Diablo II, WarCraft II, etc.)";

// Layout constants
static constexpr int MARGIN = 25;
static constexpr int SPACING = 25;
static constexpr int SMALL_SPACING = 8;  // For related items like radio buttons
static constexpr int BUTTON_HEIGHT = 60;
static constexpr int EDIT_HEIGHT = 50;
static constexpr int MIN_DLG_WIDTH = 400;

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

    // Save log format radio button state
    if (IsDlgButtonChecked(hDlg, IDC_RADIO_TIMESTAMP_ARCHIVE_FILENAME) == BST_CHECKED)
        g_logFormat = LogFormat::TIMESTAMP_ARCHIVE_FILENAME;
    else if (IsDlgButtonChecked(hDlg, IDC_RADIO_ARCHIVE_FILENAME) == BST_CHECKED)
        g_logFormat = LogFormat::ARCHIVE_FILENAME;
    else if (IsDlgButtonChecked(hDlg, IDC_RADIO_TIMESTAMP_FILENAME) == BST_CHECKED)
        g_logFormat = LogFormat::TIMESTAMP_FILENAME;
    else if (IsDlgButtonChecked(hDlg, IDC_RADIO_FILENAME_ONLY) == BST_CHECKED)
        g_logFormat = LogFormat::FILENAME_ONLY;

    // Save target game radio button state
    if (IsDlgButtonChecked(hDlg, IDC_RADIO_DIABLO1) == BST_CHECKED)
        g_targetGame = TargetGame::DIABLO_1;
    else if (IsDlgButtonChecked(hDlg, IDC_RADIO_LATER) == BST_CHECKED)
        g_targetGame = TargetGame::LATER;

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
    SIZE uniqueCheckboxSize = MeasureText(hdc, UNIQUE_CHECKBOX_TEXT);
    SIZE timestampInfoSize = MeasureText(hdc, TIMESTAMP_INFO_TEXT);
    SIZE radio1Size = MeasureText(hdc, RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT);
    SIZE radio2Size = MeasureText(hdc, RADIO_ARCHIVE_FILENAME_TEXT);
    SIZE radio3Size = MeasureText(hdc, RADIO_TIMESTAMP_FILENAME_TEXT);
    SIZE radio4Size = MeasureText(hdc, RADIO_FILENAME_ONLY_TEXT);
    SIZE radioDiablo1Size = MeasureText(hdc, RADIO_DIABLO1_TEXT);
    SIZE radioLaterSize = MeasureText(hdc, RADIO_LATER_TEXT);
    SIZE labelSize = MeasureText(hdc, PATH_LABEL_TEXT);
    SIZE browseSize = MeasureText(hdc, "Browse...");
    SIZE okSize = MeasureText(hdc, "OK");
    SIZE cancelSize = MeasureText(hdc, "Cancel");

    // Add padding for checkbox/radio buttons (button circle/square + spacing)
    uniqueCheckboxSize.cx += 20;
    uniqueCheckboxSize.cy += 4;
    radio1Size.cx += 20;
    radio1Size.cy += 4;
    radio2Size.cx += 20;
    radio2Size.cy += 4;
    radio3Size.cx += 20;
    radio3Size.cy += 4;
    radio4Size.cx += 20;
    radio4Size.cy += 4;
    radioDiablo1Size.cx += 20;
    radioDiablo1Size.cy += 4;
    radioLaterSize.cx += 20;
    radioLaterSize.cy += 4;

    // Add padding for buttons
    browseSize.cx += 16;
    browseSize.cy = BUTTON_HEIGHT;
    okSize.cx += 24;
    okSize.cy = BUTTON_HEIGHT;
    cancelSize.cx += 24;
    cancelSize.cy = BUTTON_HEIGHT;

    // Calculate required width (widest element + margins)
    int contentWidth = descSize.cx;
    if (uniqueCheckboxSize.cx > contentWidth) contentWidth = uniqueCheckboxSize.cx;
    if (radio1Size.cx > contentWidth) contentWidth = radio1Size.cx;
    if (radio2Size.cx > contentWidth) contentWidth = radio2Size.cx;
    if (radio3Size.cx > contentWidth) contentWidth = radio3Size.cx;
    if (radio4Size.cx > contentWidth) contentWidth = radio4Size.cx;
    if (radioDiablo1Size.cx > contentWidth) contentWidth = radioDiablo1Size.cx;
    if (radioLaterSize.cx > contentWidth) contentWidth = radioLaterSize.cx;
    if (labelSize.cx > contentWidth) contentWidth = labelSize.cx;

    g_dlgWidth = contentWidth + (MARGIN * 2);
    if (g_dlgWidth < MIN_DLG_WIDTH) g_dlgWidth = MIN_DLG_WIDTH;

    // Calculate required height
    int y = MARGIN;
    y += descSize.cy + SPACING;                    // Description
    y += uniqueCheckboxSize.cy + SPACING;          // Unique checkbox
    // Group box for log format
    int logFormatGroupBoxHeight = 20 + timestampInfoSize.cy + SMALL_SPACING +
                                   radio1Size.cy + SMALL_SPACING + radio2Size.cy + SMALL_SPACING +
                                   radio3Size.cy + SMALL_SPACING + radio4Size.cy + 15;
    y += logFormatGroupBoxHeight + SPACING;        // Log format group box
    // Group box for log file name
    int logFilenameGroupBoxHeight = 20 + labelSize.cy + SPACING + EDIT_HEIGHT + 15;
    y += logFilenameGroupBoxHeight + SPACING;      // Log file name group box
    // Group box for target game
    int targetGameGroupBoxHeight = 20 + radioDiablo1Size.cy + SMALL_SPACING + radioLaterSize.cy + 15;
    y += targetGameGroupBoxHeight + SPACING;       // Target game group box
    y += SPACING;                                  // Extra spacing before buttons
    y += BUTTON_HEIGHT + MARGIN;                   // OK/Cancel buttons

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
    SIZE uniqueCheckboxSize = MeasureText(hdc, UNIQUE_CHECKBOX_TEXT);
    uniqueCheckboxSize.cx += 20;
    uniqueCheckboxSize.cy += 4;
    SIZE radio1Size = MeasureText(hdc, RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT);
    radio1Size.cx += 20;
    radio1Size.cy += 4;
    SIZE radio2Size = MeasureText(hdc, RADIO_ARCHIVE_FILENAME_TEXT);
    radio2Size.cx += 20;
    radio2Size.cy += 4;
    SIZE radio3Size = MeasureText(hdc, RADIO_TIMESTAMP_FILENAME_TEXT);
    radio3Size.cx += 20;
    radio3Size.cy += 4;
    SIZE radio4Size = MeasureText(hdc, RADIO_FILENAME_ONLY_TEXT);
    radio4Size.cx += 20;
    radio4Size.cy += 4;
    SIZE radioDiablo1Size = MeasureText(hdc, RADIO_DIABLO1_TEXT);
    radioDiablo1Size.cx += 20;
    radioDiablo1Size.cy += 4;
    SIZE radioLaterSize = MeasureText(hdc, RADIO_LATER_TEXT);
    radioLaterSize.cx += 20;
    radioLaterSize.cy += 4;
    SIZE timestampInfoSize = MeasureText(hdc, TIMESTAMP_INFO_TEXT);
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

    // Unique checkbox
    HWND hUniqueCheck = CreateWindowExA(
        0, "BUTTON", UNIQUE_CHECKBOX_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        MARGIN, y, uniqueCheckboxSize.cx + SPACING, uniqueCheckboxSize.cy,
        hDlg, (HMENU)IDC_UNIQUE_CHECKBOX, hModule, nullptr
    );
    SendMessage(hUniqueCheck, WM_SETFONT, (WPARAM)hFont, TRUE);
    CheckDlgButton(hDlg, IDC_UNIQUE_CHECKBOX, g_logUniqueOnly ? BST_CHECKED : BST_UNCHECKED);
    y += uniqueCheckboxSize.cy + SPACING;

    // Log format group box
    int logFormatGroupBoxHeight = 35 + timestampInfoSize.cy + SMALL_SPACING +
                                   radio1Size.cy + SMALL_SPACING + radio2Size.cy + SMALL_SPACING +
                                   radio3Size.cy + SMALL_SPACING + radio4Size.cy + 15;
    HWND hLogFormatGroupBox = CreateWindowExA(
        0, "BUTTON", LOG_FORMAT_GROUPBOX_TEXT,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        MARGIN, y, contentWidth, logFormatGroupBoxHeight,
        hDlg, (HMENU)IDC_LOG_FORMAT_GROUPBOX, hModule, nullptr
    );
    SendMessage(hLogFormatGroupBox, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Timestamp info label inside the log format group box
    int logFormatInnerY = y + 35;  // Offset for group box title
    int logFormatInnerX = MARGIN + 10;  // Indent inside group box

    HWND hTimestampInfo = CreateWindowExA(
        0, "STATIC", TIMESTAMP_INFO_TEXT,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        logFormatInnerX, logFormatInnerY, timestampInfoSize.cx, timestampInfoSize.cy,
        hDlg, (HMENU)IDC_TIMESTAMP_INFO_LABEL, hModule, nullptr
    );
    SendMessage(hTimestampInfo, WM_SETFONT, (WPARAM)hFont, TRUE);
    logFormatInnerY += timestampInfoSize.cy + SMALL_SPACING;

    // Radio buttons inside the log format group box

    // Radio button 1: timestamp + archive + filename (first in group)
    HWND hRadio1 = CreateWindowExA(
        0, "BUTTON", RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        logFormatInnerX, logFormatInnerY, radio1Size.cx + SPACING, radio1Size.cy,
        hDlg, (HMENU)IDC_RADIO_TIMESTAMP_ARCHIVE_FILENAME, hModule, nullptr
    );
    SendMessage(hRadio1, WM_SETFONT, (WPARAM)hFont, TRUE);
    logFormatInnerY += radio1Size.cy + SMALL_SPACING;

    // Radio button 2: archive + filename
    HWND hRadio2 = CreateWindowExA(
        0, "BUTTON", RADIO_ARCHIVE_FILENAME_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        logFormatInnerX, logFormatInnerY, radio2Size.cx + SPACING, radio2Size.cy,
        hDlg, (HMENU)IDC_RADIO_ARCHIVE_FILENAME, hModule, nullptr
    );
    SendMessage(hRadio2, WM_SETFONT, (WPARAM)hFont, TRUE);
    logFormatInnerY += radio2Size.cy + SMALL_SPACING;

    // Radio button 3: timestamp + filename
    HWND hRadio3 = CreateWindowExA(
        0, "BUTTON", RADIO_TIMESTAMP_FILENAME_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        logFormatInnerX, logFormatInnerY, radio3Size.cx + SPACING, radio3Size.cy,
        hDlg, (HMENU)IDC_RADIO_TIMESTAMP_FILENAME, hModule, nullptr
    );
    SendMessage(hRadio3, WM_SETFONT, (WPARAM)hFont, TRUE);
    logFormatInnerY += radio3Size.cy + SMALL_SPACING;

    // Radio button 4: filename only
    HWND hRadio4 = CreateWindowExA(
        0, "BUTTON", RADIO_FILENAME_ONLY_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        logFormatInnerX, logFormatInnerY, radio4Size.cx + SPACING, radio4Size.cy,
        hDlg, (HMENU)IDC_RADIO_FILENAME_ONLY, hModule, nullptr
    );
    SendMessage(hRadio4, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set initial radio button selection based on g_logFormat
    int selectedRadio = IDC_RADIO_FILENAME_ONLY;
    switch (g_logFormat)
    {
        case LogFormat::TIMESTAMP_ARCHIVE_FILENAME:
            selectedRadio = IDC_RADIO_TIMESTAMP_ARCHIVE_FILENAME;
            break;
        case LogFormat::ARCHIVE_FILENAME:
            selectedRadio = IDC_RADIO_ARCHIVE_FILENAME;
            break;
        case LogFormat::TIMESTAMP_FILENAME:
            selectedRadio = IDC_RADIO_TIMESTAMP_FILENAME;
            break;
        case LogFormat::FILENAME_ONLY:
            selectedRadio = IDC_RADIO_FILENAME_ONLY;
            break;
    }
    CheckDlgButton(hDlg, selectedRadio, BST_CHECKED);

    y += logFormatGroupBoxHeight + SPACING;

    // Log file name group box
    int logFilenameGroupBoxHeight = 35 + labelSize.cy + SPACING + EDIT_HEIGHT + 15;
    HWND hLogFilenameGroupBox = CreateWindowExA(
        0, "BUTTON", LOG_FILENAME_GROUPBOX_TEXT,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        MARGIN, y, contentWidth, logFilenameGroupBoxHeight,
        hDlg, (HMENU)IDC_LOG_FILENAME_GROUPBOX, hModule, nullptr
    );
    SendMessage(hLogFilenameGroupBox, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Controls inside the log file name group box
    int logFilenameInnerY = y + 35;  // Offset for group box title
    int logFilenameInnerX = MARGIN + 10;  // Indent inside group box

    // Path label
    HWND hLabel = CreateWindowExA(
        0, "STATIC", PATH_LABEL_TEXT,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        logFilenameInnerX, logFilenameInnerY, labelSize.cx, labelSize.cy,
        hDlg, (HMENU)IDC_PATH_LABEL, hModule, nullptr
    );
    SendMessage(hLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    logFilenameInnerY += labelSize.cy + SPACING;

    // Path edit and Browse button on same row
    int browseWidth = browseSize.cx;
    int editWidth = contentWidth - browseWidth - SPACING - 20;  // Account for group box margins

    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", g_logFileName.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        logFilenameInnerX, logFilenameInnerY, editWidth, EDIT_HEIGHT,
        hDlg, (HMENU)IDC_PATH_EDIT, hModule, nullptr
    );
    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    int browseX = logFilenameInnerX + editWidth + SPACING;
    HWND hBrowse = CreateWindowExA(
        0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        browseX, logFilenameInnerY, browseWidth, EDIT_HEIGHT,
        hDlg, (HMENU)IDC_BROWSE_BUTTON, hModule, nullptr
    );
    SendMessage(hBrowse, WM_SETFONT, (WPARAM)hFont, TRUE);

    y += logFilenameGroupBoxHeight + SPACING;

    // Target game group box
    int targetGameGroupBoxHeight = 35 + radioDiablo1Size.cy + SMALL_SPACING + radioLaterSize.cy + 15;
    HWND hTargetGameGroupBox = CreateWindowExA(
        0, "BUTTON", TARGET_GAME_GROUPBOX_TEXT,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        MARGIN, y, contentWidth, targetGameGroupBoxHeight,
        hDlg, (HMENU)IDC_TARGET_GAME_GROUPBOX, hModule, nullptr
    );
    SendMessage(hTargetGameGroupBox, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Radio buttons inside the target game group box
    int targetGameInnerY = y + 35;  // Offset for group box title
    int targetGameInnerX = MARGIN + 10;  // Indent inside group box

    // Radio button: Diablo 1 (first in target game group)
    HWND hRadioDiablo1 = CreateWindowExA(
        0, "BUTTON", RADIO_DIABLO1_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        targetGameInnerX, targetGameInnerY, radioDiablo1Size.cx + SPACING, radioDiablo1Size.cy,
        hDlg, (HMENU)IDC_RADIO_DIABLO1, hModule, nullptr
    );
    SendMessage(hRadioDiablo1, WM_SETFONT, (WPARAM)hFont, TRUE);
    targetGameInnerY += radioDiablo1Size.cy + SMALL_SPACING;

    // Radio button: Later games
    HWND hRadioLater = CreateWindowExA(
        0, "BUTTON", RADIO_LATER_TEXT,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        targetGameInnerX, targetGameInnerY, radioLaterSize.cx + SPACING, radioLaterSize.cy,
        hDlg, (HMENU)IDC_RADIO_LATER, hModule, nullptr
    );
    SendMessage(hRadioLater, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set initial target game radio button selection
    int selectedGameRadio = (g_targetGame == TargetGame::DIABLO_1) ? IDC_RADIO_DIABLO1 : IDC_RADIO_LATER;
    CheckDlgButton(hDlg, selectedGameRadio, BST_CHECKED);

    y += targetGameGroupBoxHeight + SPACING + SPACING;

    // OK and Cancel buttons (right-aligned)
    int buttonY = y;
    int cancelX = browseX;
    int cancelWidth = browseWidth;
    int okX = cancelX - SPACING - okSize.cx * 4;

    HWND hOK = CreateWindowExA(
        0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        okX, buttonY, okSize.cx * 4, BUTTON_HEIGHT,
        hDlg, (HMENU)IDC_OK_BUTTON, hModule, nullptr
    );
    SendMessage(hOK, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND hCancel = CreateWindowExA(
        0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cancelX, buttonY, cancelWidth, BUTTON_HEIGHT,
        hDlg, (HMENU)IDC_CANCEL_BUTTON, hModule, nullptr
    );
    SendMessage(hCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Calculate actual height based on where buttons ended up
    int actualHeight = buttonY + BUTTON_HEIGHT + MARGIN;

    // Resize the dialog to fit the content
    // Account for window chrome (title bar, borders)
    RECT rcClient = {0, 0, g_dlgWidth, actualHeight};
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

    // Create dialog with a reasonable initial size - it will be resized after controls are created
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DIALOG_CLASS_NAME,
        "MPQFileLister - Configuration",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,  // Not visible initially
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,  // Temporary size (will be resized)
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
    UpdateWindow(hDlg);

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
