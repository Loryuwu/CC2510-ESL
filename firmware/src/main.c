#include <cc2510fx.h>
#include "epd.h"

#define BV(x) (1 << (x))

#define LED       P2_1
#define LED_BOOST P2_2

extern void delay_ms(uint16_t ms);

void main(void) {
    // LED + RGB off
    P2DIR |= BV(1) | BV(2);
    P1DIR |= BV(5) | BV(6) | BV(7);
    LED_BOOST = 0;
    LED = 0;
    P1_5 = 0; P1_6 = 0; P1_7 = 0;

    // Init display
    epd_init();

    // White background
    epd_clear(WHITE);

    // Red banner at top
    epd_rect(0, 0, SCR_W, 30, RED);

    // Title in white on red
    epd_text(70, 8, "PRICE TAG", WHITE, 2);

    // Main text
    epd_text(10, 45, "Hello World!", BLACK, 3);

    // Separator line
    epd_rect(10, 75, 276, 2, BLACK);

    // Info text
    epd_text(10, 85, "CC2510 + IL0373 EPD", BLACK, 2);

    // Color demo boxes
    epd_rect(10, 110, 80, 35, BLACK);
    epd_rect(100, 110, 80, 35, RED);

    // Labels inside boxes
    epd_text(22, 120, "BLACK", WHITE, 1);
    epd_text(117, 120, "RED", WHITE, 1);

    // Bottom info
    epd_text(200, 140, "152x296px", BLACK, 1);

    // Send to display
    epd_update();
    epd_sleep();

    // Done: slow blink
    LED_BOOST = 1;
    while (1) {
        LED = 1; delay_ms(1000);
        LED = 0; delay_ms(1000);
    }
}
