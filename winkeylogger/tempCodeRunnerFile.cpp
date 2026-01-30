#include <windows.h>
#include <stdio.h>

LRESULT hook_proc(int code, WPARAM wParam, LPARAM lParam) {
    printf("Key was pressed\n");
}


int main() {
    SetWindowsHookExA(WH_KEYBOARD_LL, hook_proc, NULL, 0);
}