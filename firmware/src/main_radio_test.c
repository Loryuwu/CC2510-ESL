#include <cc2510fx.h>
#include "radio.h"

#define BV(x) (1 << (x))

#define LED_W      P2_1
#define LED_BOOST  P2_2
#define LED_R      P1_5
#define LED_G      P1_7
#define LED_B      P1_6

extern void delay_ms(uint16_t ms);

void main(void) {
    uint8_t pkt[8];
    uint16_t seq = 0;
    uint8_t marc;

    P2DIR |= BV(1) | BV(2);
    P1DIR |= BV(5) | BV(6) | BV(7);
    LED_BOOST = 0; LED_W = 0;
    LED_R = 0; LED_G = 0; LED_B = 0;

    radio_init(0x01);

    LED_BOOST = 1;

    /* Startup: 3x green = firmware OK */
    LED_G = 1; delay_ms(200); LED_G = 0; delay_ms(200);
    LED_G = 1; delay_ms(200); LED_G = 0; delay_ms(200);
    LED_G = 1; delay_ms(200); LED_G = 0;

    /* Check MARCSTATE after init (should be IDLE=0x01) */
    marc = MARCSTATE & 0x1F;
    if (marc == 0x01) {
        /* Blue = IDLE OK */
        LED_B = 1; delay_ms(500); LED_B = 0;
    } else {
        /* Red = NOT IDLE (bad!) */
        LED_R = 1; delay_ms(2000); LED_R = 0;
    }

    delay_ms(500);

    while (1) {
        pkt[0] = (uint8_t)(seq >> 8);
        pkt[1] = (uint8_t)(seq);
        pkt[2] = 0xDE;
        pkt[3] = 0xAD;
        pkt[4] = 'T';
        pkt[5] = 'E';
        pkt[6] = 'S';
        pkt[7] = 'T';

        LED_W = 1;
        if (radio_tx(pkt, 8)) {
            /* TX OK - check marcstate returned to IDLE */
            marc = MARCSTATE & 0x1F;
            if (marc != 0x01) {
                /* Stuck! Red blink */
                LED_R = 1; delay_ms(100); LED_R = 0;
            }
        } else {
            /* TX failed - red */
            LED_R = 1; delay_ms(500); LED_R = 0;
        }
        LED_W = 0;

        seq++;
        delay_ms(50);
    }
}
