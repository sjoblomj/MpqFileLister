/*
    ConfigDialog.cpp - Configuration dialog for MpqFileLister plugin
*/

#include "ConfigDialog.h"
#include "Config.h"
#include "MpqFileLister.h"
#include <algorithm>
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
static const char* TIMESTAMP_INFO_TEXT = "Timestamp is in milliseconds since epoch (1970-01-01)";
static const char* RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT = "<timestamp> <MPQ archive>: <filename>";
static const char* RADIO_ARCHIVE_FILENAME_TEXT = "<MPQ archive>: <filename>";
static const char* RADIO_TIMESTAMP_FILENAME_TEXT = "<timestamp> <filename>";
static const char* RADIO_FILENAME_ONLY_TEXT = "<filename>";
static const char* LOG_FILENAME_GROUPBOX_TEXT = "Log file name";
static const char* PATH_LABEL_TEXT = "Enter filename only (not full path) to create the file in the game's directory";
static const char* BROWSE_BUTTON_TEXT = "&Browse...";
static const char* TARGET_GAME_GROUPBOX_TEXT = "Target game";
static const char* RADIO_DIABLO1_TEXT = "Diablo I";
static const char* RADIO_LATER_TEXT = "Later games (StarCraft, Diablo II, WarCraft II, etc.)";
static const char* OK_BUTTON_TEXT = "OK";
static const char* CANCEL_BUTTON_TEXT = "Cancel";
static const char* FILE_DIALOG_TITLE = "Select Log File Location";
static const char* FILE_DIALOG_FILTER = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
static const char* FILE_DIALOG_DEFAULT_EXT = "txt";
static const char* DIALOG_WINDOW_TITLE = PLUGIN_NAME " - Configuration";
static const char* DIALOG_CLASS_NAME = "MpqFileListerConfigDialog";

// Layout constants
static constexpr int MARGIN = 25;
static constexpr int SPACING = 25;
static constexpr int SMALL_SPACING = 8;  // For related items like radio buttons
static constexpr int BUTTON_HEIGHT = 60;
static constexpr int EDIT_HEIGHT = 50;
static constexpr int MIN_DLG_WIDTH = 400;
static constexpr int GROUPBOX_TITLE_HEIGHT = 35;
static constexpr int GROUPBOX_BOTTOM_PADDING = 15;
static constexpr int GROUPBOX_INNER_INDENT = 10;
static constexpr int GROUPBOX_FILENAME_INDENT = 20;

// Calculated dialog dimensions (set during control creation)
static int g_dlgWidth = 0;
static int g_dlgHeight = 0;

// Structure to hold all measured sizes
struct DialogSizes
{
    SIZE desc;
    SIZE uniqueCheckbox;
    SIZE timestampInfo;
    SIZE radio1, radio2, radio3, radio4;
    SIZE radioDiablo1, radioLater;
    SIZE label;
    SIZE browse;
    SIZE ok, cancel;
};

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

// Helper to add padding to a SIZE for radio buttons/checkboxes
static void AddRadioPadding(SIZE& size)
{
    size.cx += 20;
    size.cy += 4;
}

// Helper to add padding to a SIZE for buttons
static void AddButtonPadding(SIZE& size, int extraWidth)
{
    size.cx += extraWidth;
    size.cy = BUTTON_HEIGHT;
}

// Helper to create a control and set its font
static HWND CreateControl(const char* className, const char* text, DWORD style,
                          int x, int y, int width, int height,
                          HWND hParent, int id, HMODULE hModule, HFONT hFont,
                          DWORD exStyle = 0)
{
    HWND hControl = CreateWindowExA(
        exStyle, className, text, style,
        x, y, width, height,
        hParent, (HMENU)(intptr_t)id, hModule, nullptr
    );
    if (hControl && hFont)
        SendMessage(hControl, WM_SETFONT, (WPARAM)hFont, TRUE);
    return hControl;
}

// Helper to find maximum width from a list of widths
static int MaxWidth(std::initializer_list<int> widths)
{
    return *std::max_element(widths.begin(), widths.end());
}

// Measure all text elements once
static DialogSizes MeasureAllSizes(HDC hdc, int maxDescWidth = 400)
{
    DialogSizes sizes;

    sizes.desc = MeasureText(hdc, DESCRIPTION_TEXT, maxDescWidth);
    sizes.uniqueCheckbox = MeasureText(hdc, UNIQUE_CHECKBOX_TEXT);
    sizes.timestampInfo = MeasureText(hdc, TIMESTAMP_INFO_TEXT);
    sizes.radio1 = MeasureText(hdc, RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT);
    sizes.radio2 = MeasureText(hdc, RADIO_ARCHIVE_FILENAME_TEXT);
    sizes.radio3 = MeasureText(hdc, RADIO_TIMESTAMP_FILENAME_TEXT);
    sizes.radio4 = MeasureText(hdc, RADIO_FILENAME_ONLY_TEXT);
    sizes.radioDiablo1 = MeasureText(hdc, RADIO_DIABLO1_TEXT);
    sizes.radioLater = MeasureText(hdc, RADIO_LATER_TEXT);
    sizes.label = MeasureText(hdc, PATH_LABEL_TEXT);
    sizes.browse = MeasureText(hdc, BROWSE_BUTTON_TEXT);
    sizes.ok = MeasureText(hdc, OK_BUTTON_TEXT);
    sizes.cancel = MeasureText(hdc, CANCEL_BUTTON_TEXT);

    // Add padding
    AddRadioPadding(sizes.uniqueCheckbox);
    AddRadioPadding(sizes.radio1);
    AddRadioPadding(sizes.radio2);
    AddRadioPadding(sizes.radio3);
    AddRadioPadding(sizes.radio4);
    AddRadioPadding(sizes.radioDiablo1);
    AddRadioPadding(sizes.radioLater);
    AddButtonPadding(sizes.browse, 16);
    AddButtonPadding(sizes.ok, 24);
    AddButtonPadding(sizes.cancel, 24);

    return sizes;
}

// Calculate height of log format group box
static int CalculateLogFormatGroupBoxHeight(const DialogSizes& sizes)
{
    return GROUPBOX_TITLE_HEIGHT + sizes.timestampInfo.cy + SMALL_SPACING +
           sizes.radio1.cy + SMALL_SPACING + sizes.radio2.cy + SMALL_SPACING +
           sizes.radio3.cy + SMALL_SPACING + sizes.radio4.cy + GROUPBOX_BOTTOM_PADDING;
}

// Calculate height of log filename group box
static int CalculateLogFilenameGroupBoxHeight(const DialogSizes& sizes)
{
    return GROUPBOX_TITLE_HEIGHT + sizes.label.cy + SPACING + EDIT_HEIGHT + GROUPBOX_BOTTOM_PADDING;
}

// Calculate height of target game group box
static int CalculateTargetGameGroupBoxHeight(const DialogSizes& sizes)
{
    return GROUPBOX_TITLE_HEIGHT + sizes.radioDiablo1.cy + SMALL_SPACING +
           sizes.radioLater.cy + GROUPBOX_BOTTOM_PADDING;
}

static void HandleBrowseButton(HWND hDlg)
{
    char filename[MAX_PATH] = "";
    GetDlgItemTextA(hDlg, IDC_PATH_EDIT, filename, MAX_PATH);

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hDlg;
    ofn.lpstrFilter = FILE_DIALOG_FILTER;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = FILE_DIALOG_TITLE;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = FILE_DIALOG_DEFAULT_EXT;

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
    DialogSizes sizes = MeasureAllSizes(hdc, 400);

    // Calculate required width (widest element + margins)
    int contentWidth = MaxWidth({
        sizes.desc.cx, sizes.uniqueCheckbox.cx,
        sizes.radio1.cx, sizes.radio2.cx, sizes.radio3.cx, sizes.radio4.cx,
        sizes.radioDiablo1.cx, sizes.radioLater.cx, sizes.label.cx
    });

    g_dlgWidth = contentWidth + (MARGIN * 3);
    if (g_dlgWidth < MIN_DLG_WIDTH) g_dlgWidth = MIN_DLG_WIDTH;

    // Calculate required height
    int y = MARGIN;
    y += sizes.desc.cy + SPACING;                           // Description
    y += sizes.uniqueCheckbox.cy + SPACING;                 // Unique checkbox
    y += CalculateLogFormatGroupBoxHeight(sizes) + SPACING; // Log format group box
    y += CalculateLogFilenameGroupBoxHeight(sizes) + SPACING; // Log file name group box
    y += CalculateTargetGameGroupBoxHeight(sizes) + SPACING;  // Target game group box
    y += SPACING;                                           // Extra spacing before buttons
    y += BUTTON_HEIGHT + MARGIN;                            // OK/Cancel buttons

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

    // Measure all elements for positioning (with actual dialog width for description wrapping)
    DialogSizes sizes = MeasureAllSizes(hdc, g_dlgWidth - (MARGIN * 2));

    SelectObject(hdc, hOldFont);
    ReleaseDC(hDlg, hdc);

    int y = MARGIN;
    int contentWidth = g_dlgWidth - (MARGIN * 2);

    // Description (multiline static text)
    CreateControl("STATIC", DESCRIPTION_TEXT, WS_CHILD | WS_VISIBLE | SS_LEFT,
                  MARGIN, y, contentWidth, sizes.desc.cy,
                  hDlg, IDC_DESCRIPTION, hModule, hFont);
    y += sizes.desc.cy + SPACING;

    // Unique checkbox
    CreateControl("BUTTON", UNIQUE_CHECKBOX_TEXT, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                  MARGIN, y, sizes.uniqueCheckbox.cx + SPACING, sizes.uniqueCheckbox.cy,
                  hDlg, IDC_UNIQUE_CHECKBOX, hModule, hFont);
    CheckDlgButton(hDlg, IDC_UNIQUE_CHECKBOX, g_logUniqueOnly ? BST_CHECKED : BST_UNCHECKED);
    y += sizes.uniqueCheckbox.cy + SPACING;

    // Log format group box
    int logFormatGroupBoxHeight = CalculateLogFormatGroupBoxHeight(sizes);
    CreateControl("BUTTON", LOG_FORMAT_GROUPBOX_TEXT, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  MARGIN, y, contentWidth, logFormatGroupBoxHeight,
                  hDlg, IDC_LOG_FORMAT_GROUPBOX, hModule, hFont);

    // Timestamp info label inside the log format group box
    int logFormatInnerY = y + GROUPBOX_TITLE_HEIGHT;
    int logFormatInnerX = MARGIN + GROUPBOX_INNER_INDENT;

    CreateControl("STATIC", TIMESTAMP_INFO_TEXT, WS_CHILD | WS_VISIBLE | SS_LEFT,
                  logFormatInnerX, logFormatInnerY, sizes.timestampInfo.cx, sizes.timestampInfo.cy,
                  hDlg, IDC_TIMESTAMP_INFO_LABEL, hModule, hFont);
    logFormatInnerY += sizes.timestampInfo.cy + SMALL_SPACING;

    // Radio buttons inside the log format group box
    CreateControl("BUTTON", RADIO_TIMESTAMP_ARCHIVE_FILENAME_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
                  logFormatInnerX, logFormatInnerY, sizes.radio1.cx + SPACING, sizes.radio1.cy,
                  hDlg, IDC_RADIO_TIMESTAMP_ARCHIVE_FILENAME, hModule, hFont);
    logFormatInnerY += sizes.radio1.cy + SMALL_SPACING;

    CreateControl("BUTTON", RADIO_ARCHIVE_FILENAME_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                  logFormatInnerX, logFormatInnerY, sizes.radio2.cx + SPACING, sizes.radio2.cy,
                  hDlg, IDC_RADIO_ARCHIVE_FILENAME, hModule, hFont);
    logFormatInnerY += sizes.radio2.cy + SMALL_SPACING;

    CreateControl("BUTTON", RADIO_TIMESTAMP_FILENAME_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                  logFormatInnerX, logFormatInnerY, sizes.radio3.cx + SPACING, sizes.radio3.cy,
                  hDlg, IDC_RADIO_TIMESTAMP_FILENAME, hModule, hFont);
    logFormatInnerY += sizes.radio3.cy + SMALL_SPACING;

    CreateControl("BUTTON", RADIO_FILENAME_ONLY_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                  logFormatInnerX, logFormatInnerY, sizes.radio4.cx + SPACING, sizes.radio4.cy,
                  hDlg, IDC_RADIO_FILENAME_ONLY, hModule, hFont);

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
    int logFilenameGroupBoxHeight = CalculateLogFilenameGroupBoxHeight(sizes);
    CreateControl("BUTTON", LOG_FILENAME_GROUPBOX_TEXT, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  MARGIN, y, contentWidth, logFilenameGroupBoxHeight,
                  hDlg, IDC_LOG_FILENAME_GROUPBOX, hModule, hFont);

    // Controls inside the log file name group box
    int logFilenameInnerY = y + GROUPBOX_TITLE_HEIGHT;
    int logFilenameInnerX = MARGIN + GROUPBOX_FILENAME_INDENT;

    CreateControl("STATIC", PATH_LABEL_TEXT, WS_CHILD | WS_VISIBLE | SS_LEFT,
                  logFilenameInnerX, logFilenameInnerY, sizes.label.cx, sizes.label.cy,
                  hDlg, IDC_PATH_LABEL, hModule, hFont);
    logFilenameInnerY += sizes.label.cy + SPACING;

    // Path edit and Browse button on same row
    int browseWidth = sizes.browse.cx;
    int editWidth = contentWidth - browseWidth - SPACING - 20;  // Account for group box margins

    CreateControl("EDIT", g_logFileName.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                  logFilenameInnerX, logFilenameInnerY, editWidth, EDIT_HEIGHT,
                  hDlg, IDC_PATH_EDIT, hModule, hFont, WS_EX_CLIENTEDGE);

    int browseX = logFilenameInnerX + editWidth;
    CreateControl("BUTTON", BROWSE_BUTTON_TEXT, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  browseX, logFilenameInnerY, browseWidth, EDIT_HEIGHT,
                  hDlg, IDC_BROWSE_BUTTON, hModule, hFont);

    y += logFilenameGroupBoxHeight + SPACING;

    // Target game group box
    int targetGameGroupBoxHeight = CalculateTargetGameGroupBoxHeight(sizes);
    CreateControl("BUTTON", TARGET_GAME_GROUPBOX_TEXT, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                  MARGIN, y, contentWidth, targetGameGroupBoxHeight,
                  hDlg, IDC_TARGET_GAME_GROUPBOX, hModule, hFont);

    // Radio buttons inside the target game group box
    int targetGameInnerY = y + GROUPBOX_TITLE_HEIGHT;
    int targetGameInnerX = MARGIN + GROUPBOX_INNER_INDENT;

    CreateControl("BUTTON", RADIO_DIABLO1_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
                  targetGameInnerX, targetGameInnerY, sizes.radioDiablo1.cx + SPACING, sizes.radioDiablo1.cy,
                  hDlg, IDC_RADIO_DIABLO1, hModule, hFont);
    targetGameInnerY += sizes.radioDiablo1.cy + SMALL_SPACING;

    CreateControl("BUTTON", RADIO_LATER_TEXT,
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
                  targetGameInnerX, targetGameInnerY, sizes.radioLater.cx + SPACING, sizes.radioLater.cy,
                  hDlg, IDC_RADIO_LATER, hModule, hFont);

    // Set initial target game radio button selection
    int selectedGameRadio = (g_targetGame == TargetGame::DIABLO_1) ? IDC_RADIO_DIABLO1 : IDC_RADIO_LATER;
    CheckDlgButton(hDlg, selectedGameRadio, BST_CHECKED);

    y += targetGameGroupBoxHeight + SPACING + SPACING;

    // OK and Cancel buttons (right-aligned)
    int buttonY = y;
    int cancelX = browseX;
    int cancelWidth = browseWidth;
    int okX = cancelX - SPACING - sizes.ok.cx * 4;

    CreateControl("BUTTON", OK_BUTTON_TEXT, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                  okX, buttonY, sizes.ok.cx * 4, BUTTON_HEIGHT,
                  hDlg, IDC_OK_BUTTON, hModule, hFont);

    CreateControl("BUTTON", CANCEL_BUTTON_TEXT, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  cancelX, buttonY, cancelWidth, BUTTON_HEIGHT,
                  hDlg, IDC_CANCEL_BUTTON, hModule, hFont);

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
            if (cmd == IDC_OK_BUTTON || cmd == IDOK)
            {
                HandleOkButton(hDlg);
                s_dialogRunning = false;
                return 0;
            }
            else if (cmd == IDC_CANCEL_BUTTON || cmd == IDCANCEL)
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

        // Handle dialog messages (Tab navigation, Enter, Escape, etc.)
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

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
        DIALOG_WINDOW_TITLE,
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
