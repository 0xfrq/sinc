#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MINUTES  90
#define TIMER_ID         1
#define TICK_MS          1000
#define WM_TRAYICON      (WM_USER + 1)
#define IDI_TRAY_MIN     100
#define IDI_TRAY_SEC     101

static HWND           g_hwnd      = NULL;
static NOTIFYICONDATA g_nidMin    = {0};   
static NOTIFYICONDATA g_nidSec    = {0};   
static HICON          g_iconMin   = NULL;
static HICON          g_iconSec   = NULL;

static long g_totalSeconds     = 0;
static long g_remainingSeconds = 0;
static int  g_finished         = 0;

static HICON MakeSquareIcon(const char *text, COLORREF fg, COLORREF bgColor)
{
    int SZ = GetSystemMetrics(SM_CXSMICON);   
    int W  = SZ;
    int H  = SZ;

    HDC screenDC = GetDC(NULL);
    HDC memDC    = CreateCompatibleDC(screenDC);

    BITMAPV5HEADER bmi = {0};
    bmi.bV5Size        = sizeof(bmi);
    bmi.bV5Width       = W;
    bmi.bV5Height      = -H;          
    bmi.bV5Planes      = 1;
    bmi.bV5BitCount    = 32;
    bmi.bV5Compression = BI_RGB;

    void   *bits = NULL;
    HBITMAP hBmp = CreateDIBSection(screenDC, (BITMAPINFO *)&bmi,
                                    DIB_RGB_COLORS, &bits, NULL, 0);
    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);

    RECT rc = {0, 0, W, H};
    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN hPen    = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
    HPEN hOldPen = (HPEN)SelectObject(memDC, hPen);
    HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH hOldBrush  = (HBRUSH)SelectObject(memDC, hNullBrush);
    RoundRect(memDC, 0, 0, W, H, 4, 4);
    SelectObject(memDC, hOldPen);
    SelectObject(memDC, hOldBrush);
    DeleteObject(hPen);

    LOGFONTA lf  = {0};
    lf.lfHeight  = -(H - 2);
    lf.lfWeight  = FW_BOLD;
    lf.lfQuality = CLEARTYPE_QUALITY;
    strcpy(lf.lfFaceName, "Consolas");   
    HFONT hFont    = CreateFontIndirectA(&lf);
    HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, fg);

    SIZE sz = {0};
    GetTextExtentPoint32A(memDC, text, (int)strlen(text), &sz);
    int x = (W - sz.cx) / 2;
    int y = (H - sz.cy) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    TextOutA(memDC, x, y, text, (int)strlen(text));

    SelectObject(memDC, hOldFont);
    DeleteObject(hFont);

    HBITMAP hMask = CreateBitmap(W, H, 1, 1, NULL);

    ICONINFO ii = {0};
    ii.fIcon    = TRUE;
    ii.hbmColor = hBmp;
    ii.hbmMask  = hMask;
    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(memDC, hOld);
    DeleteObject(hBmp);
    DeleteObject(hMask);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    return hIcon;
}

static void UpdateTray(void)
{
    long m = g_remainingSeconds / 60;
    long s = g_remainingSeconds % 60;

    char minBuf[8], secBuf[8];
    char tipMin[64], tipSec[64];

    if (g_finished) {
        strcpy(minBuf, "00");
        strcpy(secBuf, "00");
        strcpy(tipMin, "Timer finished! (minutes)");
        strcpy(tipSec, "Timer finished! (seconds)");
    } else {
        sprintf(minBuf, "%02ld", m);
        sprintf(secBuf, "%02ld", s);
        sprintf(tipMin, "%02ld min %02ld sec remaining", m, s);
        sprintf(tipSec, "%02ld min %02ld sec remaining", m, s);
    }

    double frac = (g_totalSeconds > 0)
                  ? (double)g_remainingSeconds / (double)g_totalSeconds
                  : 0.0;

    COLORREF fg, bg;
    if (g_finished || frac < 0.15) {
        fg = RGB(255, 255, 255);
        bg = RGB(180,  30,  30);  
    } else if (frac < 0.33) {
        fg = RGB( 30,  30,  30);
        bg = RGB(220, 180,   0);   
    } else {
        fg = RGB(255, 255, 255);
        bg = RGB( 25,  25,  25);   
    }

    HICON newMin = MakeSquareIcon(minBuf, fg, bg);
    if (g_iconMin) DestroyIcon(g_iconMin);
    g_iconMin = newMin;

    g_nidMin.uFlags = NIF_ICON | NIF_TIP;
    g_nidMin.hIcon  = g_iconMin;
    strcpy(g_nidMin.szTip, tipMin);
    Shell_NotifyIcon(NIM_MODIFY, &g_nidMin);

    HICON newSec = MakeSquareIcon(secBuf, fg, bg);
    if (g_iconSec) DestroyIcon(g_iconSec);
    g_iconSec = newSec;

    g_nidSec.uFlags = NIF_ICON | NIF_TIP;
    g_nidSec.hIcon  = g_iconSec;
    strcpy(g_nidSec.szTip, tipSec);
    Shell_NotifyIcon(NIM_MODIFY, &g_nidSec);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                 WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, TIMER_ID, TICK_MS, NULL);
        return 0;

    case WM_TIMER:
        if (!g_finished) {
            if (g_remainingSeconds > 0) g_remainingSeconds--;
            if (g_remainingSeconds == 0) {
                g_finished = 1;
                KillTimer(hwnd, TIMER_ID);
                UpdateTray();
                g_nidSec.uFlags      = NIF_INFO;
                g_nidSec.dwInfoFlags = NIIF_INFO;
                g_nidSec.uTimeout    = 5000;
                strcpy(g_nidSec.szInfoTitle, "Timer finished!");
                strcpy(g_nidSec.szInfo,      "Your countdown has reached zero.");
                Shell_NotifyIcon(NIM_MODIFY, &g_nidSec);
            } else {
                UpdateTray();
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lp == WM_RBUTTONUP) {
            if (MessageBoxA(NULL, "Exit the timer?", "Taskbar Timer",
                            MB_YESNO | MB_ICONQUESTION) == IDYES)
                DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ID);
        Shell_NotifyIcon(NIM_DELETE, &g_nidMin);
        Shell_NotifyIcon(NIM_DELETE, &g_nidSec);
        if (g_iconMin) { DestroyIcon(g_iconMin); g_iconMin = NULL; }
        if (g_iconSec) { DestroyIcon(g_iconSec); g_iconSec = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static int AskMinutes(void)
{
    AllocConsole();
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);

    const char *prompt =
        "=== Taskbar Countdown Timer ===\r\n"
        "Enter minutes [default 90]: ";
    DWORD written;
    WriteConsoleA(hOut, prompt, (DWORD)strlen(prompt), &written, NULL);

    char buf[32] = {0};
    DWORD nread  = 0;
    ReadConsoleA(hIn, buf, sizeof(buf) - 1, &nread, NULL);
    for (int i = 0; i < (int)nread; i++)
        if (buf[i] == '\r' || buf[i] == '\n') { buf[i] = 0; break; }

    int minutes = DEFAULT_MINUTES;
    if (buf[0] != '\0') {
        int parsed = atoi(buf);
        if (parsed > 0) minutes = parsed;
    }

    char confirm[256];
    sprintf(confirm,
        "Starting %d-minute timer.\r\n"
        "Two icons will appear in the system tray:\r\n"
        "  Left  = minutes remaining\r\n"
        "  Right = seconds remaining\r\n"
        "Right-click either icon to quit.\r\n",
        minutes);
    WriteConsoleA(hOut, confirm, (DWORD)strlen(confirm), &written, NULL);
    Sleep(1500);
    FreeConsole();
    return minutes;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow)
{
    (void)hPrev; (void)lpCmd; (void)nShow;

    int minutes        = AskMinutes();
    g_totalSeconds     = (long)minutes * 60;
    g_remainingSeconds = g_totalSeconds;

    WNDCLASSEXA wc   = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "TrayTimerClass";
    RegisterClassExA(&wc);

    g_hwnd = CreateWindowExA(0, "TrayTimerClass", "TrayTimer",
                             0, 0, 0, 0, 0,
                             HWND_MESSAGE, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    g_nidMin.cbSize           = sizeof(g_nidMin);
    g_nidMin.hWnd             = g_hwnd;
    g_nidMin.uID              = IDI_TRAY_MIN;
    g_nidMin.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nidMin.uCallbackMessage = WM_TRAYICON;
    g_nidMin.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nidMin.szTip, "Minutes remaining");
    Shell_NotifyIcon(NIM_ADD, &g_nidMin);

    g_nidSec.cbSize           = sizeof(g_nidSec);
    g_nidSec.hWnd             = g_hwnd;
    g_nidSec.uID              = IDI_TRAY_SEC;
    g_nidSec.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nidSec.uCallbackMessage = WM_TRAYICON;
    g_nidSec.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
    strcpy(g_nidSec.szTip, "Seconds remaining");
    Shell_NotifyIcon(NIM_ADD, &g_nidSec);

    UpdateTray();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}