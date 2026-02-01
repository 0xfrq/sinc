#include <stdio.h>
#include <windows.h>
#include <unistd.h>

int main() {
    HDC screenDC = GetDC(NULL);

    if(screenDC != 0) {
        HBRUSH  hbrush = CreateSolidBrush(RGB(255, 255, 0));
        SelectObject(screenDC, hbrush);
    
        while(1) {
            Rectangle(screenDC, 50, 300, 100, 120);
            Rectangle(screenDC, 100, 230, 150, 190);
            Rectangle(screenDC, 150, 300, 200, 120);
            
            Rectangle(screenDC, 250, 300, 300, 120);

            Rectangle(screenDC, 350, 300, 400, 120);
            Rectangle(screenDC, 400, 160, 450, 120);
            Rectangle(screenDC, 400, 300, 450, 260);
            Rectangle(screenDC, 450, 160, 500, 260);

            Rectangle(screenDC, 550, 300, 600, 120);
            Rectangle(screenDC, 600, 300, 650, 260);
            Rectangle(screenDC, 650, 300, 700, 120);

            Rectangle(screenDC, 750, 300, 800, 120);
            Rectangle(screenDC, 800, 160, 850, 120);
            Rectangle(screenDC, 800, 230, 850, 190);
            Rectangle(screenDC, 850, 230, 900, 120);



            Rectangle(screenDC, 100, 530, 150, 480);
            Rectangle(screenDC, 150, 530, 200, 350);
            
            Rectangle(screenDC, 250, 530, 300, 350);
            Rectangle(screenDC, 300, 530, 350, 480);
            Rectangle(screenDC, 300, 400, 350, 350);
            Rectangle(screenDC, 350, 530, 400, 350);

        }
    } else {
        printf("failed, u suck\n");
    }
}