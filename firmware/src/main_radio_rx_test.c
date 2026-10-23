#include <cc2510fx.h>
#include "radio.h"

#define BV(x) (1 << (x))

#define LED       P2_1
#define LED_BOOST P2_2
#define LED_R     P1_5
#define LED_G     P1_7
#define LED_B     P1_6

extern void delay_ms(uint16_t ms);

void main(void) {
    signed char maxr, r;
    uint8_t level, i;
    uint32_t w;

    P2DIR |= BV(1) | BV(2);
    P1DIR |= BV(5) | BV(6) | BV(7);
    LED_BOOST = 1;
    LED = 0;
    LED_R = 0; LED_G = 0; LED_B = 0;

    LED_R = 1; delay_ms(250); LED_R = 0; delay_ms(150);
    LED_G = 1; delay_ms(250); LED_G = 0; delay_ms(150);
    LED_B = 1; delay_ms(250); LED_B = 0; delay_ms(400);

    radio_init(0x01);
    if ((MARCSTATE & 0x1F) != 0x01) {
        while (1) { LED_R = 1; delay_ms(80); LED_R = 0; delay_ms(80); }
    }

    while (1) {
        /* Fresh RX each window so the FIFO can never overflow / freeze */
        RFST = 0x04;                        /* IDLE (flushes RX FIFO) */
        { uint16_t x = 40000; while ((MARCSTATE & 0x1F) != 0x01 && --x); }
        RFST = 0x02;                        /* RX */
        delay_ms(3);                        /* settle, RSSI valid */

        maxr = -128;
        for (w = 0; w < 120000UL; w++) {
            /* drain FIFO so RX never overflows */
            if (TCON & 0x02) { TCON &= ~0x02; (void)RFD; }
            /* recover if it overflowed anyway */
            if ((MARCSTATE & 0x1F) == 0x11) {
                RFST = 0x04;
                { uint16_t x = 20000; while ((MARCSTATE & 0x1F) != 0x01 && --x); }
                RFST = 0x02;
                delay_ms(1);
            }
            r = (signed char)RSSI;
            if (r > maxr) maxr = r;
        }

        RFST = 0x04;

        level = (uint8_t)(((int16_t)maxr + 128) >> 4);   /* 0..15 */

        for (i = 0; i <= level; i++) {
            LED_G = 1; delay_ms(90); LED_G = 0; delay_ms(110);
        }
        delay_ms(1200);
    }
}
