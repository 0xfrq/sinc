#include <windows.h>
#include <stdio.h>

HBITMAP hBitmap;
HDC memDC;
int imgW, imgH;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                       memDC, 0, 0, imgW, imgH, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int main() {
    HINSTANCE hInst = GetModuleHandle(NULL);

    // --- Load bitmap ---
    HDC screenDC = GetDC(NULL);

    hBitmap = (HBITMAP)LoadImage(NULL,
        "image.bmp",
        IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_DEFAULTCOLOR);

    if (!hBitmap) {
        FILE *f = fopen("image.bmp", "rb");
        if (!f) { printf("Cannot open file\n"); return 1; }
        BITMAPFILEHEADER bfh;
        BITMAPINFOHEADER bih;
        fread(&bfh, sizeof(bfh), 1, f);
        fread(&bih, sizeof(bih), 1, f);
        fseek(f, bfh.bfOffBits, SEEK_SET);
        void *bits = NULL;
        BITMAPINFO bi = {0};
        bi.bmiHeader = bih;
        hBitmap = CreateDIBSection(screenDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (hBitmap && bits)
            fread(bits, 1, bfh.bfSize - bfh.bfOffBits, f);
        fclose(f);
    }

    if (!hBitmap) { printf("Failed to load image\n"); return 1; }

    memDC = CreateCompatibleDC(screenDC);
    SelectObject(memDC, hBitmap);
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    imgW = bmp.bmWidth;
    imgH = bmp.bmHeight;
    ReleaseDC(NULL, screenDC);

    // --- Register window class ---
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = "FullscreenBMP";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // --- Create borderless topmost window ---
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST,                  // always on top
        "FullscreenBMP", "",
        WS_POPUP | WS_VISIBLE,          // no title bar, no borders
        0, 0, sw, sh,                   // covers full screen
        NULL, NULL, hInst, NULL
    );

    ShowCursor(FALSE);                  // hide mouse cursor

    // --- Message loop ---
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    ShowCursor(TRUE);
    DeleteDC(memDC);
    DeleteObject(hBitmap);
    return 0;
}