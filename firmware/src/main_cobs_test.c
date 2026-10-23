/*
 * main_cobs_test.c  —  Demo fuer das eingebundene UART+COBS-Modul.
 *
 * Startet den interrupt-getriebenen UART (USART1, P0_4 TX / P0_5 RX, 115200)
 * und echot jedes empfangene COBS-Paket zurueck. Gegenstelle am PC:
 * cobs_test.py (benoetigt `pip install cobs pyserial` + TTL-USB-Adapter).
 *
 * Hinweis: P0_4/P0_5 sind mit NFC-SDA bzw. EPD-CLK geteilt -> dieser Build ist
 * ein reines UART-Debug-Image und laeuft nicht zusammen mit NFC/EPD/Flash.
 * Fuer exakte Baudrate sollte der 26-MHz-Quarz laufen (wie in radio_init);
 * fuer einen einfachen Test genuegt der Power-on-Takt meist.
 *
 * Build:  make cobs-test   ->   cobs_test.bin
 */
#include "hal_compat.h"
#include "uart.h"
#include "cobs.h"

void main(void) {
    static CobsState __xdata cobs;

    HAL_ENABLE_INTERRUPTS();   /* UART ist interrupt-getrieben */
    uart_init();
    cobs_init(&cobs);

    cobs_send_str("PriceTag COBS bereit");

    for (;;) {
        if (cobs_handle(&cobs)) {
            cobs_send(cobs.packet, cobs.packet_size);   /* Echo zurueck */
        }
    }
}
