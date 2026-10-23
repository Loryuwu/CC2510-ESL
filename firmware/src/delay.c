#include <stdint.h>

void delay_ms(uint16_t ms) {
    uint16_t i;
    while (ms--) {
        for (i = 0; i < 1600; i++)
            __asm nop __endasm;
    }
}
