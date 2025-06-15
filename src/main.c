#include <windows.h>
#include <shellapi.h>
#include <stringapiset.h>
#include <wingdi.h>
#include <winreg.h>
#include <powrprof.h>

#include <stdio.h>

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_INFO 1002
#define ID_TRAY_FAST_STARTUP 1003
#define ID_TRAY_POWEROFF 1004
#define ID_TRAY_RESTART 1005
#define ID_TRAY_LOCK 1006
#define IDI_MYICON_LIGHT 101
#define IDI_MYICON_DARK 202
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

char *get_version_string() {
    static char version[64];
    DWORD verHandle = 0;

    char exePath[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        MessageBoxW(NULL, L"Failed to get module file name.", L"Error", MB_OK | MB_ICONERROR);
        return "unknown";
    }

    DWORD verSize = GetFileVersionInfoSizeA(exePath, &verHandle);
    if (verSize == 0) {
        MessageBoxW(NULL, L"Failed to get version info size.", L"Error", MB_OK | MB_ICONERROR);
        return "unknown";
    }

    static char verData[256];
    if (!GetFileVersionInfoA(exePath, verHandle, verSize, verData)) {
        MessageBoxW(NULL, L"Failed to get version info data.", L"Error", MB_OK | MB_ICONERROR);
        return "unknown";
    }

    VS_FIXEDFILEINFO *fileInfo = NULL;
    UINT size = 0;
    if (VerQueryValueA(verData, "\\", (LPVOID *)&fileInfo, &size) && size) {
        sprintf(version, "%d.%d.%d.%d",
                HIWORD(fileInfo->dwFileVersionMS),
                LOWORD(fileInfo->dwFileVersionMS),
                HIWORD(fileInfo->dwFileVersionLS),
                LOWORD(fileInfo->dwFileVersionLS));
    } else {
        MessageBoxW(NULL, L"Failed to query version value.", L"Error", MB_OK | MB_ICONERROR);
        strcpy(version, "unknown");
    }
    return version;
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

    // Get the version string
    const char *version = get_version_string();

    char uptimeStr[64];
    if (days > 0) {
        sprintf(uptimeStr, "Uptime: %dd %02dh %02dm %02ds", days, hours, minutes, seconds);
    } else {
        sprintf(uptimeStr, "Uptime: %02dh %02dm %02ds", hours, minutes, seconds);
    }

    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    char *fast_startup_text;
    char title[64];

    if (fast_startup_enabled)
        fast_startup_text = "Fast Startup is: Enabled (click to disable)";
    else
        fast_startup_text = "Fast Startup is: Disabled :D (click to enable)";

    sprintf(title, "SystemUP! - Uptime in Tray v%s by h4rl", version);

    AppendMenu(hMenu, MF_STRING, ID_TRAY_INFO, title);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_INFO, uptimeStr);
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_FAST_STARTUP, fast_startup_text);
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_POWEROFF, "Shutdown Computer");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_RESTART, "Restart Computer");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_LOCK, "Lock Computer");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
            is_fast_startup_enabled();
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
