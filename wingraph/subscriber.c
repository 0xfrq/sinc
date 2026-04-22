#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ws2_32.lib")

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

static int read_line(SOCKET s, char *buf, int maxlen)
{
    int total = 0;
    while (total < maxlen - 1) {
        char c;
        int n = recv(s, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[total++] = c;
    }
    buf[total] = '\0';
    return total;
}

static DWORD WINAPI NetworkThread(LPVOID param)
{
    NetArgs *args = (NetArgs *)param;
    printf("[net] connecting to %s:%d ...\n", args->host, args->port);

    while (TRUE) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) { Sleep(2000); continue; }

        struct sockaddr_in sa = {0};
        sa.sin_family = AF_INET;
        sa.sin_port   = htons((u_short)args->port);
        inet_pton(AF_INET, args->host, &sa.sin_addr);

        if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
            closesocket(sock);
            printf("[net] publisher unreachable, retrying in 2 s...\n");
            Sleep(2000);
            continue;
        }

        printf("[net] connected to %s:%d\n", args->host, args->port);

        char line[64];
        while (TRUE) {
            int n = read_line(sock, line, sizeof(line));
            if (n <= 0) break;
            printf("[net] received: %s\n", line);
            if      (strcmp(line, "SHOW")  == 0) PostMessage(g_hwnd, WM_APP_SHOW,  0, 0);
            else if (strcmp(line, "SHOW2") == 0) PostMessage(g_hwnd, WM_APP_SHOW2, 0, 0);
            else if (strcmp(line, "HIDE")  == 0) PostMessage(g_hwnd, WM_APP_HIDE,  0, 0);
            else if (strcmp(line, "EXIT")  == 0) {
                PostMessage(g_hwnd, WM_APP_EXIT, 0, 0);
                closesocket(sock);
                return 0;
            }
        }

        closesocket(sock);
        printf("[net] disconnected, retrying in 2 s...\n");
        Sleep(2000);
    }
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
    printf("  Fullscreen BMP Subscriber\n");
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

    printf("Publisher port [9000]: ");
    fflush(stdout);

    if (fgets(input, sizeof(input), stdin)) {
        int p = atoi(input);
        out->port = (p > 0 && p <= 65535) ? p : 9000;
    } else {
        out->port = 9000;
    }
    printf("\n");
}

int main(void)
{
    NetArgs netArgs;
    memset(&netArgs, 0, sizeof(netArgs));
    strncpy(netArgs.host, "127.0.0.1", sizeof(netArgs.host) - 1);
    netArgs.port = 9000;

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

    HANDLE hThread = CreateThread(NULL, 0, NetworkThread, (LPVOID)&netArgs, 0, NULL);
    if (!hThread) {
        fprintf(stderr, "Failed to create network thread\n");
        return 1;
    }
    CloseHandle(hThread);

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