#include <windows.h>
#include <stdio.h>

LRESULT hook_proc(int code, WPARAM wParam, LPARAM lParam) {
    printf("Key was pressed!\n");

    return CallNextHookEx(NULL, code, wParam, lParam);
}


int main() {
    //hook here
    HHOOK hhook = SetWindowsHookExA(WH_KEYBOARD_LL, hook_proc, NULL, 0);
    if (hhook == NULL) 
        printf("hook wasnt installed");
    printf("hook installed succesfully");

    //loop messages here
    MSG msg;
    while((GetMessage(&msg, NULL, 0, 0)) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}