#include <stdio.h>
#include <windows.h>
#include <unistd.h>

int main() {
    // wipe the shit of your screen
    HDC screenDC = GetDC(NULL);

    if(screenDC != 0) {
        HBRUSH  hbrush = CreateSolidBrush(RGB(0, 0, 0));
        SelectObject(screenDC, hbrush);
        
        int move = 1;
        while(move) {
            Rectangle(screenDC, 100+move, 0, 200+move, 1200);
            usleep(10000);
        }
    } else {
        printf("failed, u suck\n");
    }
}