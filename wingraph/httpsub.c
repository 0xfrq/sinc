#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

static HDC      g_memDC    = NULL;
static HDC      g_memDC2   = NULL;
static int      g_imgW     = 0;
static int      g_imgH     = 0;
static int      g_imgW2    = 0;
static int      g_imgH2    = 0;
static int      g_activeImg = 1;
static HBITMAP  g_hBitmap2 = NULL;

static HWND     g_hwnd    = NULL;
static BOOL     g_visible = FALSE;

#define WM_APP_SHOW  (WM_APP + 1)
#define WM_APP_HIDE  (WM_APP + 2)
#define WM_APP_EXIT  (WM_APP + 3)
#define WM_APP_SHOW2 (WM_APP + 4)

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (g_activeImg == 1 && g_memDC && g_imgW > 0 && g_imgH > 0)
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                       g_memDC, 0, 0, g_imgW, g_imgH, SRCCOPY);
        else if (g_activeImg == 2 && g_memDC2 && g_imgW2 > 0 && g_imgH2 > 0)
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                       g_memDC2, 0, 0, g_imgW2, g_imgH2, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_APP_SHOW:
        g_activeImg = 1;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        ShowCursor(FALSE);
        g_visible = TRUE;
        return 0;

    case WM_APP_SHOW2:
        g_activeImg = (g_hBitmap2 != NULL) ? 2 : 1;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        ShowCursor(FALSE);
        g_visible = TRUE;
        return 0;

    case WM_APP_HIDE:
        ShowWindow(hwnd, SW_HIDE);
        ShowCursor(TRUE);
        g_visible = FALSE;
        return 0;

    case WM_APP_EXIT:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        else if (wp == VK_DELETE) PostMessage(hwnd, WM_APP_HIDE, 0, 0);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

typedef struct {
    char host[256];
    int  port;
} NetArgs;

static void send_gameover(const char *host, int port)
{
    HINTERNET hSession = WinHttpOpen(L"BMP-Subscriber/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        printf("[gameover] WinHttpOpen failed\n");
        return;
    }

    // Convert host to wide char
    wchar_t whost[256];
    MultiByteToWideChar(CP_ACP, 0, host, -1, whost, 256);

    HINTERNET hConnect = WinHttpConnect(hSession, whost, port, 0);
    if (!hConnect) {
        printf("[gameover] WinHttpConnect failed\n");
        WinHttpCloseHandle(hSession);
        return;
    }

    BOOL use_https = (port == 443) ? TRUE : FALSE;
    DWORD flags = use_https ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/event", NULL,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        printf("[gameover] WinHttpOpenRequest failed\n");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Disable SSL certificate verification (for self-signed certs from tunnel)
    if (use_https) {
        DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | 
                       SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
    }

    const char *data = "event=ok";
    DWORD data_len = (DWORD)strlen(data);

    wchar_t host_header[512];
    swprintf(host_header, sizeof(host_header)/sizeof(wchar_t), L"Host: %s:%d\r\nContent-Type: application/x-www-form-urlencoded\r\n", whost, port);

    BOOL sent = WinHttpSendRequest(hRequest, host_header, (DWORD)-1, (void *)data, data_len, data_len, 0);
    if (!sent) {
        printf("[gameover] WinHttpSendRequest failed\n");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    BOOL received = WinHttpReceiveResponse(hRequest, NULL);
    if (received) {
        printf("[input] GAMEOVER sent to server\n");
    } else {
        printf("[gameover] WinHttpReceiveResponse failed\n");
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

typedef struct {
    NetArgs *netArgs;
} InputThreadArgs;

static DWORD WINAPI InputThread(LPVOID param)
{
    NetArgs *netArgs = (NetArgs *)param;
    char buf[512];
    
    while (TRUE) {
        printf("Enter answer (or 'gameover'): ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        int len = (int)strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';

        if (_stricmp(buf, "gameover") == 0) {
            send_gameover(netArgs->host, netArgs->port);
        }
    }
    return 0;
}

static DWORD WINAPI PollThread(LPVOID param)
{
    NetArgs *args = (NetArgs *)param;
    printf("[poll] starting HTTP poll loop\n");

    HINTERNET hSession = WinHttpOpen(L"BMP-Subscriber/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        printf("[poll] WinHttpOpen failed\n");
        return 1;
    }

    // Convert host to wide char
    wchar_t whost[256];
    MultiByteToWideChar(CP_ACP, 0, args->host, -1, whost, 256);

    while (TRUE) {
        HINTERNET hConnect = WinHttpConnect(hSession, whost, args->port, 0);
        if (!hConnect) {
            printf("[poll] publisher unreachable, retrying in 2 s...\n");
            Sleep(2000);
            continue;
        }

        BOOL use_https = (args->port == 443) ? TRUE : FALSE;
        DWORD flags = use_https ? WINHTTP_FLAG_SECURE : 0;

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/command", NULL,
                                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            printf("[poll] WinHttpOpenRequest failed\n");
            WinHttpCloseHandle(hConnect);
            Sleep(2000);
            continue;
        }

        // Disable SSL certificate verification (for self-signed certs from tunnel)
        if (use_https) {
            DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | 
                           SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
        }

        // Build Host header
        wchar_t host_header[512];
        swprintf(host_header, sizeof(host_header)/sizeof(wchar_t), L"Host: %s:%d\r\n", whost, args->port);

        if (!WinHttpSendRequest(hRequest, host_header, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            printf("[poll] WinHttpSendRequest failed\n");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            Sleep(2000);
            continue;
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            printf("[poll] WinHttpReceiveResponse failed\n");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            Sleep(2000);
            continue;
        }

        printf("[poll] connected to %s:%d\n", args->host, args->port);

        // Read response body
        DWORD dwSize = 0;
        char response[256] = {0};

        if (WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            if (dwSize > 0 && dwSize < sizeof(response)) {
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, (void *)response, dwSize, &dwDownloaded)) {
                    response[dwDownloaded] = '\0';

                    // Trim whitespace
                    int len = (int)strlen(response);
                    while (len > 0 && (response[len-1] == '\n' || response[len-1] == '\r' || response[len-1] == ' '))
                        response[--len] = '\0';

                    if (len > 0) {
                        printf("[poll] received command: %s\n", response);

                        if (strcmp(response, "SHOW") == 0)
                            PostMessage(g_hwnd, WM_APP_SHOW, 0, 0);
                        else if (strcmp(response, "SHOW2") == 0)
                            PostMessage(g_hwnd, WM_APP_SHOW2, 0, 0);
                        else if (strcmp(response, "HIDE") == 0)
                            PostMessage(g_hwnd, WM_APP_HIDE, 0, 0);
                        else if (strcmp(response, "EXIT") == 0) {
                            PostMessage(g_hwnd, WM_APP_EXIT, 0, 0);
                            WinHttpCloseHandle(hRequest);
                            WinHttpCloseHandle(hConnect);
                            WinHttpCloseHandle(hSession);
                            return 0;
                        }
                    }
                }
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);

        // Poll every 1 second
        Sleep(1000);
    }

    WinHttpCloseHandle(hSession);
    return 0;
}

static BOOL LoadImg(HDC screenDC, const char *path, HDC *outDC, int *outW, int *outH, HBITMAP *outBitmap)
{
    HBITMAP hbm = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0,
                                      LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    if (!hbm) {
        printf("[load] LoadImageA failed for '%s', trying manual load...\n", path);

        FILE *f = fopen(path, "rb");
        if (!f) { printf("[load] fopen failed\n"); return FALSE; }

        BITMAPFILEHEADER bfh;
        BITMAPINFOHEADER bih;
        if (fread(&bfh, sizeof(bfh), 1, f) != 1 ||
            fread(&bih, sizeof(bih), 1, f) != 1) {
            fclose(f); return FALSE;
        }

        bih.biCompression = BI_RGB;
        bih.biBitCount    = 24;
        bih.biSizeImage   = 0;

        BITMAPINFO bi = {0};
        bi.bmiHeader = bih;

        void *bits = NULL;
        hbm = CreateDIBSection(screenDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hbm || !bits) {
            printf("[load] CreateDIBSection failed\n");
            fclose(f); return FALSE;
        }

        fseek(f, bfh.bfOffBits, SEEK_SET);
        fread(bits, 1, bfh.bfSize - bfh.bfOffBits, f);
        fclose(f);
    }

    BITMAP bmp;
    GetObject(hbm, sizeof(BITMAP), &bmp);
    *outW = bmp.bmWidth;
    *outH = bmp.bmHeight;

    HDC dc = CreateCompatibleDC(screenDC);
    if (!dc) { DeleteObject(hbm); return FALSE; }

    SelectObject(dc, hbm);
    *outDC     = dc;
    *outBitmap = hbm;
    printf("[load] '%s' loaded OK  %dx%d\n", path, *outW, *outH);
    return TRUE;
}

static void prompt_address(NetArgs *out)
{
    char input[300];

    printf("==============================================\n");
    printf("  Fullscreen BMP Subscriber (HTTPS)\n");
    printf("==============================================\n\n");

    printf("Publisher host (IP or hostname) [127.0.0.1]: ");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin)) {
        int len = (int)strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r' || input[len-1] == ' '))
            input[--len] = '\0';
        if (len > 0)
            strncpy(out->host, input, sizeof(out->host) - 1);
        else
            strncpy(out->host, "127.0.0.1", sizeof(out->host) - 1);
    }
    out->host[sizeof(out->host) - 1] = '\0';

    printf("Publisher port [443]: ");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin)) {
        int p = atoi(input);
        out->port = (p > 0 && p <= 65535) ? p : 443;
    } else {
        out->port = 443;
    }
    printf("\n");
}

int main(void)
{
    NetArgs netArgs;
    memset(&netArgs, 0, sizeof(netArgs));
    strncpy(netArgs.host, "127.0.0.1", sizeof(netArgs.host) - 1);
    netArgs.port = 443;

    prompt_address(&netArgs);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    HDC screenDC = GetDC(NULL);

    HBITMAP g_hBitmap = NULL;
    if (!LoadImg(screenDC, "image.bmp", &g_memDC, &g_imgW, &g_imgH, &g_hBitmap)) {
        ReleaseDC(NULL, screenDC);
        fprintf(stderr, "Failed to load image.bmp\n");
        MessageBoxA(NULL, "Failed to load image.bmp!", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!LoadImg(screenDC, "image2.bmp", &g_memDC2, &g_imgW2, &g_imgH2, &g_hBitmap2)) {
        fprintf(stderr, "image2.bmp not loaded, SHOW2 will fallback to image1\n");
    }

    ReleaseDC(NULL, screenDC);

    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "FullscreenBMP";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST,
        "FullscreenBMP", "",
        WS_POPUP,
        0, 0, sw, sh,
        NULL, NULL, hInst, NULL
    );

    HANDLE hPollThread = CreateThread(NULL, 0, PollThread, (LPVOID)&netArgs, 0, NULL);
    if (!hPollThread) {
        fprintf(stderr, "Failed to create poll thread\n");
        return 1;
    }
    CloseHandle(hPollThread);

    HANDLE hInputThread = CreateThread(NULL, 0, InputThread, (LPVOID)&netArgs, 0, NULL);
    if (!hInputThread) {
        fprintf(stderr, "Failed to create input thread\n");
        return 1;
    }
    CloseHandle(hInputThread);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_visible) ShowCursor(TRUE);
    if (g_memDC)  { DeleteDC(g_memDC);  DeleteObject(g_hBitmap);  }
    if (g_memDC2) { DeleteDC(g_memDC2); DeleteObject(g_hBitmap2); }
    WSACleanup();
    return 0;
}