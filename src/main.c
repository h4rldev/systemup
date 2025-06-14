#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stringapiset.h>
#include <wingdi.h>
#include <winreg.h>
#include <powrprof.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_INFO 1002
#define ID_TRAY_FAST_STARTUP 1003
#define ID_TRAY_POWEROFF 1004
#define ID_TRAY_RESTART 1005
#define ID_TRAY_SLEEP 1006
#define ID_TRAY_LOCK 1007
#define IDI_MYICON_LIGHT 101
#define IDI_MYICON_DARK 202
#define ID_BITMAP_EXIT 303
#define ID_BITMAP_ERROR 303 // Using the same ID for the error bitmap
#define ID_BITMAP_INFO 404
#define TIMER_ID 1

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
NOTIFYICONDATA nid;
int fast_startup_enabled = 0; // Defaults to disabled

BOOL EnableShutDownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;

    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    CloseHandle(hToken);
    return result && GetLastError() == ERROR_SUCCESS;
}

int is_dark_mode() {
    DWORD value = 1; // Default to light theme
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_READ,
            &hKey) == ERROR_SUCCESS) {

        RegQueryValueExA(hKey, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return value == 0; // 1 = light, 0 = dark
}

int is_fast_startup_enabled() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY hKey;
    if (RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power",
            0,
            KEY_READ,
            &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "HiberbootEnabled", NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 1;
        }
        RegCloseKey(hKey);
    }
    // If key or value not found, treat as disabled (or handle as error as desired)
    return 0;
}

int toggle_fast_startup() {
    HKEY hKey;
    DWORD value = is_fast_startup_enabled() ? 0 : 1; // Toggle the value
    if (RegOpenKeyExA(
            HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power",
            0,
            KEY_SET_VALUE,
            &hKey) == ERROR_SUCCESS) {
        if (RegSetValueExA(hKey, "HiberbootEnabled", 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS) {
            if (value == 0) {
                if (RegSetValueExA(hKey, "HibernateEnabled", 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) == ERROR_SUCCESS) {
                    fast_startup_enabled = value; // Update the global variable
                    RegCloseKey(hKey);
                    return 1; // Success
                } else {
                    MessageBoxW(NULL, L"Failed to set HibernateEnabled value :C", L"Error", MB_OK | MB_ICONERROR);
                }
            } else {
                fast_startup_enabled = value; // Update the global variable
                RegCloseKey(hKey);
                return 1; // Success
            }
        }
        RegCloseKey(hKey);
    }
    return 0; // Failure
}

void UpdateTrayTipWithUptime() {
    ULONGLONG ms = GetTickCount64();
    ULONGLONG total_seconds = ms / 1000;
    int hours = (int)((total_seconds / 3600) % 24);
    int minutes = (int)((total_seconds / 60) % 60);
    int seconds = (int)(total_seconds % 60);
    int days = (int)(total_seconds / 86400);

    if (days > 0) {
        sprintf(nid.szTip, "Uptime: %dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    } else {
        sprintf(nid.szTip, "Uptime: %02dh %02dm %02ds", hours, minutes, seconds);
    }
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const LPCSTR CLASS_NAME = "SystemUPTrayClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "SystemUP!", 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);

    // Add tray icon
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    if (is_dark_mode()) {
        nid.hIcon = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_MYICON_LIGHT),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR | LR_LOADTRANSPARENT
        );
    } else {
        nid.hIcon = (HICON)LoadImage(
            hInstance,
            MAKEINTRESOURCE(IDI_MYICON_DARK),
            IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON),
            LR_DEFAULTCOLOR
        );
    }
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    lstrcpy(nid.szTip, "SystemUP! - Uptime in Tray");
    UpdateTrayTipWithUptime();
    Shell_NotifyIcon(NIM_ADD, &nid);

    if(!EnableShutDownPrivilege()) {
        MessageBoxW(NULL, L"Failed to enable shutdown privileges. Some features may not work.", L"Error", MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &nid);
    return 0;
}

void ShowTrayMenu(HWND hwnd) {
    ULONGLONG ms = GetTickCount64();
    ULONGLONG total_seconds = ms / 1000;
    int hours = (int)((total_seconds / 3600) % 24);
    int minutes = (int)((total_seconds / 60) % 60);
    int seconds = (int)(total_seconds % 60);
    int days = (int)(total_seconds / 86400);

    char uptimeStr[64];
    if (days > 0) {
        sprintf(uptimeStr, "Uptime: %dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    } else {
        sprintf(uptimeStr, "Uptime: %02dh %02dm %02ds", hours, minutes, seconds);
    }

    wchar_t wideUptimeStr[64];
    MultiByteToWideChar(CP_UTF8, 0, uptimeStr, -1, wideUptimeStr, sizeof(wideUptimeStr) / sizeof(wideUptimeStr[0]));

    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();

    MENUITEMINFO mii0 = {
        .cbSize = sizeof(mii0),
        .fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE,
        .wID = ID_TRAY_INFO,
        .dwTypeData = "SystemUP! - Uptime in Tray v0.0.0.1 by h4rl",
        .fType = MFT_STRING
    }; // Use the bitmap for the menu item

    MENUITEMINFO mii1 = {
        .cbSize = sizeof(mii1),
        .fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE,
        .wID = ID_TRAY_INFO,
        .dwTypeData = uptimeStr,
        .fType = MFT_STRING
    };

    MENUITEMINFO mii2 = {
        .cbSize = sizeof(mii2),
        .fMask = MIIM_STRING | MIIM_FTYPE,
        .fType = MFT_SEPARATOR
    };

    MENUITEMINFO mii3 = {
        .cbSize = sizeof(mii3),
        .fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE,
        .wID = ID_TRAY_FAST_STARTUP,
        .fType = MFT_STRING,
    };

    if (fast_startup_enabled)
        mii3.dwTypeData = "Fast Startup is: Enabled (click to disable)";
    else
        mii3.dwTypeData = "Fast Startup is: Disabled :D (click to enable)";


    MENUITEMINFO mii4 = {
        .cbSize = sizeof(mii4),
        .fMask = MIIM_STRING | MIIM_FTYPE,
        .fType = MFT_SEPARATOR
    };

    MENUITEMINFO mii5 = {
        .cbSize = sizeof(mii5),
        .fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID,
        .wID = ID_TRAY_POWEROFF,
        .fType = MFT_STRING,
        .dwTypeData = "Shutdown Computer"
    };

    MENUITEMINFO mii6 = {
        .cbSize = sizeof(mii6),
        .fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID,
        .wID = ID_TRAY_RESTART,
        .fType = MFT_STRING,
        .dwTypeData = "Restart Computer"
    };


    MENUITEMINFO mii7 = {
        .cbSize = sizeof(mii7),
        .fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID,
        .wID = ID_TRAY_LOCK,
        .fType = MFT_STRING,
        .dwTypeData = "Lock Computer"
    };

    MENUITEMINFO mii8 = {
        .cbSize = sizeof(mii8),
        .fMask = MIIM_STRING | MIIM_FTYPE,
        .fType = MFT_SEPARATOR
    };

    MENUITEMINFO mii9 = {
        .cbSize = sizeof(mii9),
        .fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE,
        .wID = ID_TRAY_EXIT,
        .dwTypeData = "Exit",
        .fType = MFT_STRING
    };

    InsertMenuItem(hMenu, 0, TRUE, &mii0);
    InsertMenuItem(hMenu, 1, TRUE, &mii1);
    InsertMenuItem(hMenu, 2, TRUE, &mii2);
    InsertMenuItem(hMenu, 3, TRUE, &mii3);
    InsertMenuItem(hMenu, 4, TRUE, &mii4);
    InsertMenuItem(hMenu, 5, TRUE, &mii5);
    InsertMenuItem(hMenu, 6, TRUE, &mii6);
    InsertMenuItem(hMenu, 7, TRUE, &mii7);
    InsertMenuItem(hMenu, 8, TRUE, &mii8);
    InsertMenuItem(hMenu, 9, TRUE, &mii9);

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LPDRAWITEMSTRUCT lpdi = (LPDRAWITEMSTRUCT)lParam;
    HICON hIconInfo = LoadIcon(NULL, IDI_INFORMATION); // Built-in info icon

    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, 1000, NULL); // Update every second
        break;
    case WM_TRAYICON:
        if (lParam == WM_MOUSEMOVE) {
            UpdateTrayTipWithUptime();
            is_fast_startup_enabled();
        }

        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
            ShowTrayMenu(hwnd);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_FAST_STARTUP:
            toggle_fast_startup();
            break;
        case ID_TRAY_POWEROFF:
            if (MessageBoxW(hwnd, L"Are you sure you want to shutdown the computer?", L"Confirm Shutdown", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG, 0);
            }
            break;
        case ID_TRAY_RESTART:
            if (MessageBoxW(hwnd, L"Are you sure you want to restart the computer?", L"Confirm Restart", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, 0);
            }
            break;
        case ID_TRAY_LOCK:
            if (MessageBoxW(hwnd, L"Are you sure you want to lock the computer?", L"Confirm Lock", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                LockWorkStation();
            }
            break;
        case ID_TRAY_INFO:
            {
                wchar_t message[256];
                swprintf(message, sizeof(message) / sizeof(wchar_t), L"SystemUP! - Uptime in Tray v0.0.0.1 by h4rl\n%s", nid.szTip);
                MessageBoxW(hwnd, message, L"SystemUP Info", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;
    case WM_TIMER:
        if (wParam == TIMER_ID) {
            is_fast_startup_enabled();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
