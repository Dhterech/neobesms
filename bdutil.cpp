#include "bdutil.h"
int getbdlen(byte *bd) {
    byte *a = bd;
    int count = 0;
    u8 flag;
    for(;;) {
        flag = *(u8*)(a + 1);
        a += 16;
        count += 16;
        if(flag >= 0x7) {
            return count;
        }
    };
}

int getnsamplesfrombdlen(int len) {
    return (len / 16) * 28;
}
